#pragma once

#include <cstdint>
#include <string>

#include "swarm_sync_core/types.hpp"

namespace swarm_sync::weaknet {

enum class LateDisposition : uint8_t {
  NotLate = 0,
  LatePocket = 1,
  Expired = 2
};

struct CycleWindowConfig {
  int64_t period_ns{0};
  int64_t receive_cutoff_offset_ns{0};
  int64_t late_record_window_ns{0};
  int64_t ttl_ns{0};
};

struct SampleTiming {
  uint64_t target_cycle{0};
  int64_t local_receive_ns{0};
  bool sender_clock_ok{true};
  bool schema_ok{true};
  bool payload_ok{true};
  bool producer_ok{true};
  bool duplicate{false};
};

struct WindowDecision {
  SampleStatus status{SampleStatus::Missing};
  LateDisposition late_disposition{LateDisposition::NotLate};
  bool usable_for_cycle{false};
  bool record_in_late_pocket{false};
  std::string reason;
};

class CycleWindow {
public:
  explicit CycleWindow(CycleWindowConfig cfg);

  bool valid(std::string* error = nullptr) const;
  WindowDecision classify(uint64_t current_cycle,
                          int64_t cycle_start_ns,
                          const SampleTiming& sample) const;

private:
  CycleWindowConfig cfg_;
};

}  // namespace swarm_sync::weaknet
