#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "swarm_sync_core/types.hpp"

namespace swarm_sync::weaknet {

enum class LinkState : uint8_t {
  Good = 0,
  Degraded = 1,
  Congested = 2,
  Partitioned = 3,
  Recovering = 4
};

struct PeerChannelHealth {
  std::string peer_id;
  std::string channel;
  double fresh_ratio{0.0};
  double late_ratio{0.0};
  double missing_ratio{0.0};
  double duplicate_ratio{0.0};
  double wrong_cycle_ratio{0.0};
  double bad_payload_ratio{0.0};
  int64_t rx_latency_p50_ns{0};
  int64_t rx_latency_p95_ns{0};
  int64_t rx_latency_p99_ns{0};
  uint64_t seq_gap_count{0};
  uint64_t late_pocket_count{0};
  uint64_t oversized_drop_count{0};
  uint64_t observed_count{0};
  LinkState state{LinkState::Good};
  std::string primary_reason;
};

struct LinkHealthSample {
  SampleStatus status{SampleStatus::Missing};
  int64_t latency_ns{0};
  uint64_t seq_gap{0};
  bool late_pocket{false};
  bool oversized_drop{false};
};

class LinkHealthWindow {
public:
  explicit LinkHealthWindow(size_t max_samples = 100);

  void observe(const LinkHealthSample& sample);
  PeerChannelHealth summarize(const std::string& peer_id, const std::string& channel) const;
  void clear();

private:
  size_t max_samples_{100};
  std::vector<LinkHealthSample> samples_;
};

std::string toString(LinkState state);

}  // namespace swarm_sync::weaknet
