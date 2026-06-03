#pragma once

#include <string>
#include <vector>

#include "swarm_sync_core/cycle_snapshot.hpp"
#include "swarm_sync_core/deadline_checker.hpp"
#include "swarm_sync_core/sample_buffer.hpp"

namespace swarm_sync {

struct ExpectedSample {
  std::string peer_id;
  std::string channel;
  bool required{true};
  ChannelTiming timing;
};

struct SnapshotBuildRequest {
  std::string session_id;
  std::string task_id;
  std::string self_id;
  uint64_t cycle_id{0};
  int64_t cycle_start_ns{0};
  int64_t period_ns{0};
  int64_t build_time_ns{0};
  ClockState local_clock;
  RuntimeState local_runtime;
  std::vector<ExpectedSample> expected_samples;
};

class SnapshotBuilder {
public:
  CycleSnapshot build(const SnapshotBuildRequest& request, const SampleBuffer& buffer) const;

private:
  DeadlineChecker deadline_checker_;
};

}  // namespace swarm_sync
