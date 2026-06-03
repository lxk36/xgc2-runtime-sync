#include "swarm_sync_core/weaknet/link_health.hpp"

#include <algorithm>

namespace swarm_sync::weaknet {
namespace {

bool badPayloadStatus(SampleStatus status) {
  return status == SampleStatus::BadPayload ||
         status == SampleStatus::BadSchema ||
         status == SampleStatus::SenderClockBad ||
         status == SampleStatus::SenderReportedFail ||
         status == SampleStatus::TransportError;
}

int64_t percentile(std::vector<int64_t> values, double q) {
  if (values.empty()) {
    return 0;
  }
  std::sort(values.begin(), values.end());
  const size_t index = static_cast<size_t>(
      std::min<double>(values.size() - 1, std::max<double>(0.0, q * (values.size() - 1))));
  return values[index];
}

}  // namespace

LinkHealthWindow::LinkHealthWindow(size_t max_samples)
    : max_samples_(std::max<size_t>(1, max_samples)) {}

void LinkHealthWindow::observe(const LinkHealthSample& sample) {
  samples_.push_back(sample);
  if (samples_.size() > max_samples_) {
    samples_.erase(samples_.begin(), samples_.begin() + static_cast<long>(samples_.size() - max_samples_));
  }
}

PeerChannelHealth LinkHealthWindow::summarize(const std::string& peer_id,
                                              const std::string& channel) const {
  PeerChannelHealth health;
  health.peer_id = peer_id;
  health.channel = channel;
  health.observed_count = samples_.size();
  if (samples_.empty()) {
    health.state = LinkState::Partitioned;
    health.primary_reason = "no samples observed";
    return health;
  }

  uint64_t fresh = 0;
  uint64_t late = 0;
  uint64_t missing = 0;
  uint64_t duplicate = 0;
  uint64_t wrong = 0;
  uint64_t bad = 0;
  std::vector<int64_t> latencies;
  for (const auto& sample : samples_) {
    switch (sample.status) {
      case SampleStatus::Fresh:
        ++fresh;
        latencies.push_back(sample.latency_ns);
        break;
      case SampleStatus::Late:
        ++late;
        latencies.push_back(sample.latency_ns);
        break;
      case SampleStatus::Missing:
        ++missing;
        break;
      case SampleStatus::Duplicate:
        ++duplicate;
        break;
      case SampleStatus::WrongCycle:
        ++wrong;
        break;
      default:
        if (badPayloadStatus(sample.status)) {
          ++bad;
        }
        break;
    }
    health.seq_gap_count += sample.seq_gap;
    health.late_pocket_count += sample.late_pocket ? 1 : 0;
    health.oversized_drop_count += sample.oversized_drop ? 1 : 0;
  }

  const double denom = static_cast<double>(samples_.size());
  health.fresh_ratio = static_cast<double>(fresh) / denom;
  health.late_ratio = static_cast<double>(late) / denom;
  health.missing_ratio = static_cast<double>(missing) / denom;
  health.duplicate_ratio = static_cast<double>(duplicate) / denom;
  health.wrong_cycle_ratio = static_cast<double>(wrong) / denom;
  health.bad_payload_ratio = static_cast<double>(bad) / denom;
  health.rx_latency_p50_ns = percentile(latencies, 0.50);
  health.rx_latency_p95_ns = percentile(latencies, 0.95);
  health.rx_latency_p99_ns = percentile(latencies, 0.99);

  if (health.fresh_ratio == 0.0 && samples_.size() >= 3) {
    health.state = LinkState::Partitioned;
    health.primary_reason = "fresh ratio is zero";
  } else if (health.fresh_ratio < 0.90 || health.rx_latency_p99_ns > 40000000) {
    health.state = LinkState::Congested;
    health.primary_reason = "fresh ratio low or p99 latency high";
  } else if (health.late_ratio > 0.02 || health.missing_ratio > 0.01) {
    health.state = LinkState::Degraded;
    health.primary_reason = "late or missing ratio above degraded threshold";
  } else {
    health.state = LinkState::Good;
    health.primary_reason = "healthy";
  }
  return health;
}

void LinkHealthWindow::clear() {
  samples_.clear();
}

std::string toString(LinkState state) {
  switch (state) {
    case LinkState::Good:
      return "GOOD";
    case LinkState::Degraded:
      return "DEGRADED";
    case LinkState::Congested:
      return "CONGESTED";
    case LinkState::Partitioned:
      return "PARTITIONED";
    case LinkState::Recovering:
      return "RECOVERING";
  }
  return "UNKNOWN";
}

}  // namespace swarm_sync::weaknet
