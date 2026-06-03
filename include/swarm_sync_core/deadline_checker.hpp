#pragma once

#include <cstdint>
#include <string>

#include "swarm_sync_core/sample_envelope.hpp"
#include "swarm_sync_core/types.hpp"

namespace swarm_sync {

struct ChannelTiming {
  int64_t publish_deadline_ns{0};
  int64_t receive_cutoff_ns{0};
  int64_t ttl_ns{0};
  std::string payload_type;
  std::string schema_id;
  bool allow_empty_payload{true};
};

struct DeadlineDecision {
  SampleStatus status{SampleStatus::Missing};
  std::string reason;
  bool usable{false};
};

class DeadlineChecker {
public:
  DeadlineDecision checkReceivedSample(
      const SampleEnvelope& envelope,
      uint64_t current_cycle,
      int64_t cycle_start_ns,
      int64_t local_receive_ns,
      const ChannelTiming& timing) const;
};

}  // namespace swarm_sync
