#pragma once

#include <optional>
#include <string>
#include <vector>

#include "swarm_sync_core/sample_envelope.hpp"
#include "swarm_sync_core/types.hpp"

namespace swarm_sync {

struct PeerSampleView {
  std::string peer_id;
  std::string channel;

  SampleStatus status{SampleStatus::Missing};
  std::optional<SampleEnvelope> sample;

  uint64_t expected_target_cycle{0};
  uint64_t received_target_cycle{0};

  int64_t t_local_receive_ns{0};
  int64_t receive_latency_ns{0};

  bool fresh_for_this_cycle{false};
  bool late_for_this_cycle{false};
  bool duplicate{false};
  bool wrong_cycle{false};
  bool bad_schema{false};
  bool bad_crc{false};
  bool sender_clock_bad{false};

  std::string drop_reason;
};

struct SnapshotStats {
  uint32_t fresh_count{0};
  uint32_t missing_count{0};
  uint32_t late_count{0};
  uint32_t wrong_cycle_count{0};
  uint32_t bad_count{0};
};

struct CycleSnapshot {
  std::string session_id;
  std::string task_id;
  std::string self_id;

  uint64_t cycle_id{0};
  int64_t t_cycle_start_ns{0};
  int64_t period_ns{0};

  ClockState local_clock;
  RuntimeState local_runtime;

  std::vector<PeerSampleView> samples;
  SnapshotStats stats;

  bool all_required_fresh{false};
  bool all_required_usable{false};
};

}  // namespace swarm_sync
