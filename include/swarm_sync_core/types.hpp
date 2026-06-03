#pragma once

#include <cstdint>
#include <string>

namespace swarm_sync {

enum class ClockQuality : uint8_t {
  OK = 0,
  DEGRADED = 1,
  BAD = 2,
  UNKNOWN = 3
};

enum class ProducerStatus : uint8_t {
  Success = 0,
  Timeout = 1,
  Failed = 2,
  Skipped = 3,
  UserFallback = 4,
  Unknown = 255
};

enum class SampleStatus : uint8_t {
  Fresh = 0,
  Missing = 1,
  Late = 2,
  WrongCycle = 3,
  Duplicate = 4,
  BadSchema = 5,
  BadPayload = 6,
  SenderClockBad = 7,
  SenderReportedFail = 8,
  TransportError = 9
};

struct ClockState {
  bool clock_ok{false};
  int64_t offset_ns{0};
  uint64_t uncertainty_ns{0};
  ClockQuality quality{ClockQuality::UNKNOWN};
  std::string source;
};

struct RuntimeState {
  bool running{false};
  bool zenoh_connected{false};
  uint64_t current_cycle{0};
  int64_t jitter_ns{0};
  std::string state;
  std::string reason;
};

}  // namespace swarm_sync
