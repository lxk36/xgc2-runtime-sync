#pragma once

#include <cstdint>
#include <string>

namespace swarm_sync::weaknet {

enum class ChannelKind : uint8_t {
  RealtimeSample = 0,
  Health = 1,
  CommandConfig = 2,
  BulkDebug = 3
};

enum class Reliability : uint8_t { BestEffort = 0, Reliable = 1 };
enum class CongestionControl : uint8_t { Drop = 0, Block = 1 };
enum class Priority : uint8_t {
  RealTime = 0,
  InteractiveHigh = 1,
  DataHigh = 2,
  DataLow = 3,
  Background = 4
};

struct ZenohQosProfile {
  Reliability reliability{Reliability::BestEffort};
  CongestionControl congestion{CongestionControl::Drop};
  Priority priority{Priority::DataHigh};
  bool express{false};
};

struct ChannelProfile {
  std::string name;
  ChannelKind kind{ChannelKind::RealtimeSample};
  ZenohQosProfile qos;
  bool allow_ack{false};
  uint32_t max_queue_cycles{1};
  double max_hz{0.0};
  uint32_t max_bandwidth_kbps{0};
};

ChannelProfile defaultProfileFor(ChannelKind kind);
bool validateChannelProfile(const ChannelProfile& profile, std::string* error = nullptr);
std::string toString(ChannelKind kind);
std::string toString(Reliability reliability);
std::string toString(CongestionControl congestion);
std::string toString(Priority priority);

}  // namespace swarm_sync::weaknet
