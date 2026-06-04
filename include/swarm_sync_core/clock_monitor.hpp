#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "swarm_sync_core/types.hpp"

namespace swarm_sync {

enum class ClockPhase {
  Preflight,
  InFlight
};

struct ClockPolicy {
  std::string provider{"chrony"};
  std::string authority{"ground_station"};
  std::string ground_time_source;
  bool require_clock_ok_to_start{true};
  int64_t preflight_max_offset_ns{2000000};
  uint64_t preflight_max_uncertainty_ns{2000000};
  bool in_flight_allow_step{false};
  bool external_sources_allowed_in_flight{false};
  std::string phase_source{"external"};
};

struct ChronyTracking {
  bool ok{false};
  std::string reference_id;
  std::string reference_name;
  double system_time_s{0.0};
  double root_delay_s{0.0};
  double root_dispersion_s{0.0};
  std::string leap_status;
  std::string error;
};

struct ChronySources {
  bool ok{false};
  std::string selected_source;
  std::vector<std::string> sources;
  std::string error;
};

struct ClockMonitorSnapshot {
  ChronyTracking tracking;
  ChronySources sources;
};

struct ClockMonitorResult {
  ClockState state;
  bool start_allowed{false};
  bool source_is_ground{false};
  ClockPhase phase{ClockPhase::Preflight};
  std::string reason;
};

class ClockMonitor {
public:
  using CommandRunner = std::function<std::pair<int, std::string>(const std::string&)>;

  explicit ClockMonitor(CommandRunner runner = CommandRunner{});

  ClockMonitorResult sample(const ClockPolicy& policy, ClockPhase phase) const;

  static ChronyTracking parseTracking(const std::string& text);
  static ChronySources parseSources(const std::string& text);
  static ClockMonitorResult evaluateSnapshot(const ClockMonitorSnapshot& snapshot,
                                             const ClockPolicy& policy,
                                             ClockPhase phase);
  static ClockPhase phaseFromString(const std::string& text);
  static std::string phaseToString(ClockPhase phase);

private:
  CommandRunner runner_;
};

}  // namespace swarm_sync
