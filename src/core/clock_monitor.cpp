#include "swarm_sync_core/clock_monitor.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace swarm_sync {
namespace {

std::string trim(const std::string& text) {
  const auto begin = std::find_if_not(text.begin(), text.end(), [](unsigned char c) {
    return std::isspace(c);
  });
  const auto end = std::find_if_not(text.rbegin(), text.rend(), [](unsigned char c) {
    return std::isspace(c);
  }).base();
  if (begin >= end) {
    return "";
  }
  return std::string(begin, end);
}

std::string lower(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return text;
}

bool startsWith(const std::string& text, const std::string& prefix) {
  return text.size() >= prefix.size() &&
         std::equal(prefix.begin(), prefix.end(), text.begin());
}

double parseFirstDouble(const std::string& text, double fallback = 0.0) {
  std::istringstream is(text);
  double value = fallback;
  is >> value;
  return is ? value : fallback;
}

std::string valueAfterColon(const std::string& line) {
  const auto pos = line.find(':');
  if (pos == std::string::npos) {
    return "";
  }
  return trim(line.substr(pos + 1));
}

std::string parseParenthesizedName(const std::string& value) {
  const auto open = value.find('(');
  const auto close = value.find(')', open == std::string::npos ? 0 : open + 1);
  if (open == std::string::npos || close == std::string::npos || close <= open + 1) {
    return "";
  }
  return trim(value.substr(open + 1, close - open - 1));
}

int64_t secondsToNs(double seconds) {
  return static_cast<int64_t>(std::llround(seconds * 1000000000.0));
}

uint64_t uncertaintyNs(const ChronyTracking& tracking) {
  const double root_distance_s =
      std::fabs(tracking.root_dispersion_s) + std::fabs(tracking.root_delay_s) * 0.5;
  return static_cast<uint64_t>(std::llround(root_distance_s * 1000000000.0));
}

bool sourceMatches(const std::string& candidate, const std::string& expected) {
  if (expected.empty()) {
    return true;
  }
  const auto c = lower(candidate);
  const auto e = lower(expected);
  return c == e || c.find(e) != std::string::npos || e.find(c) != std::string::npos;
}

std::pair<int, std::string> runCommand(const std::string& command) {
  std::array<char, 256> buffer{};
  std::string output;
  FILE* pipe = popen((command + " 2>&1").c_str(), "r");
  if (!pipe) {
    return {127, "failed to start command"};
  }
  while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
    output += buffer.data();
  }
  const int rc = pclose(pipe);
  return {rc, output};
}

}  // namespace

ClockMonitor::ClockMonitor(CommandRunner runner)
    : runner_(std::move(runner)) {}

ChronyTracking ClockMonitor::parseTracking(const std::string& text) {
  ChronyTracking tracking;
  std::istringstream lines(text);
  std::string line;
  while (std::getline(lines, line)) {
    line = trim(line);
    if (startsWith(line, "Reference ID")) {
      const auto value = valueAfterColon(line);
      std::istringstream is(value);
      is >> tracking.reference_id;
      tracking.reference_name = parseParenthesizedName(value);
    } else if (startsWith(line, "System time")) {
      const auto value = valueAfterColon(line);
      const double magnitude = parseFirstDouble(value);
      const auto lowered = lower(value);
      tracking.system_time_s =
          lowered.find("slow") != std::string::npos ? -std::fabs(magnitude) : std::fabs(magnitude);
    } else if (startsWith(line, "Root delay")) {
      tracking.root_delay_s = parseFirstDouble(valueAfterColon(line));
    } else if (startsWith(line, "Root dispersion")) {
      tracking.root_dispersion_s = parseFirstDouble(valueAfterColon(line));
    } else if (startsWith(line, "Leap status")) {
      tracking.leap_status = valueAfterColon(line);
    }
  }
  tracking.ok = !tracking.leap_status.empty();
  if (!tracking.ok) {
    tracking.error = "chronyc tracking output missing leap status";
  }
  return tracking;
}

ChronySources ClockMonitor::parseSources(const std::string& text) {
  ChronySources sources;
  std::istringstream lines(text);
  std::string line;
  while (std::getline(lines, line)) {
    line = trim(line);
    if (line.size() < 3) {
      continue;
    }
    const char mode = line[0];
    const char state = line[1];
    if (mode != '^' && mode != '=' && mode != '#') {
      continue;
    }
    std::istringstream is(line.substr(2));
    std::string name;
    is >> name;
    if (name.empty()) {
      continue;
    }
    sources.sources.push_back(name);
    if (state == '*') {
      sources.selected_source = name;
    }
  }
  sources.ok = !sources.selected_source.empty();
  if (!sources.ok) {
    sources.error = "chronyc sources output has no selected source";
  }
  return sources;
}

ClockMonitorResult ClockMonitor::evaluateSnapshot(const ClockMonitorSnapshot& snapshot,
                                                  const ClockPolicy& policy,
                                                  ClockPhase phase) {
  ClockMonitorResult result;
  result.phase = phase;
  result.state.source = snapshot.sources.selected_source.empty()
                            ? snapshot.tracking.reference_name
                            : snapshot.sources.selected_source;
  result.state.offset_ns = secondsToNs(snapshot.tracking.system_time_s);
  result.state.uncertainty_ns = uncertaintyNs(snapshot.tracking);

  const bool tracking_ok = snapshot.tracking.ok;
  const bool sources_ok = snapshot.sources.ok;
  const bool leap_ok = lower(snapshot.tracking.leap_status) == "normal";
  const bool offset_ok = std::llabs(result.state.offset_ns) <= policy.preflight_max_offset_ns;
  const bool uncertainty_ok =
      result.state.uncertainty_ns <= policy.preflight_max_uncertainty_ns;
  result.source_is_ground =
      sourceMatches(snapshot.sources.selected_source, policy.ground_time_source) ||
      sourceMatches(snapshot.tracking.reference_name, policy.ground_time_source) ||
      sourceMatches(snapshot.tracking.reference_id, policy.ground_time_source);

  std::vector<std::string> reasons;
  if (!tracking_ok) {
    reasons.push_back(snapshot.tracking.error.empty() ? "chronyc tracking unavailable"
                                                      : snapshot.tracking.error);
  }
  if (!sources_ok) {
    reasons.push_back(snapshot.sources.error.empty() ? "chronyc sources unavailable"
                                                    : snapshot.sources.error);
  }
  if (tracking_ok && !leap_ok) {
    reasons.push_back("leap status is " + snapshot.tracking.leap_status);
  }
  if (!result.source_is_ground) {
    reasons.push_back("selected source is not ground station");
  }
  if (!offset_ok) {
    reasons.push_back("clock offset exceeds preflight gate");
  }
  if (!uncertainty_ok) {
    reasons.push_back("clock uncertainty exceeds preflight gate");
  }

  const bool healthy = tracking_ok && sources_ok && leap_ok && result.source_is_ground &&
                       offset_ok && uncertainty_ok;
  result.state.clock_ok = healthy;
  result.state.quality = healthy ? ClockQuality::OK : ClockQuality::BAD;
  if (!healthy && tracking_ok && sources_ok && leap_ok && result.source_is_ground &&
      (offset_ok || uncertainty_ok)) {
    result.state.quality = ClockQuality::DEGRADED;
  }
  result.start_allowed = healthy || !policy.require_clock_ok_to_start;

  if (reasons.empty()) {
    result.reason = "clock synchronized to ground station";
  } else {
    std::ostringstream os;
    for (size_t i = 0; i < reasons.size(); ++i) {
      if (i > 0) {
        os << "; ";
      }
      os << reasons[i];
    }
    result.reason = os.str();
  }
  if (phase == ClockPhase::InFlight && !policy.in_flight_allow_step) {
    result.reason += "; in-flight step is disabled";
  }
  return result;
}

ClockMonitorResult ClockMonitor::sample(const ClockPolicy& policy, ClockPhase phase) const {
  const auto& runner = runner_ ? runner_ : runCommand;
  const auto tracking = runner("chronyc tracking");
  const auto sources = runner("chronyc sources -v");

  ClockMonitorSnapshot snapshot;
  if (tracking.first == 0) {
    snapshot.tracking = parseTracking(tracking.second);
  } else {
    snapshot.tracking.error = "chronyc tracking failed: " + tracking.second;
  }
  if (sources.first == 0) {
    snapshot.sources = parseSources(sources.second);
  } else {
    snapshot.sources.error = "chronyc sources failed: " + sources.second;
  }
  return evaluateSnapshot(snapshot, policy, phase);
}

ClockPhase ClockMonitor::phaseFromString(const std::string& text) {
  const auto value = lower(trim(text));
  if (value == "in_flight" || value == "in-flight" || value == "flight") {
    return ClockPhase::InFlight;
  }
  return ClockPhase::Preflight;
}

std::string ClockMonitor::phaseToString(ClockPhase phase) {
  return phase == ClockPhase::InFlight ? "in_flight" : "preflight";
}

}  // namespace swarm_sync
