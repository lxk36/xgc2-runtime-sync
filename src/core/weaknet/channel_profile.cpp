#include "swarm_sync_core/weaknet/channel_profile.hpp"

#include <cmath>

namespace swarm_sync::weaknet {

ChannelProfile defaultProfileFor(ChannelKind kind) {
  ChannelProfile profile;
  profile.kind = kind;
  switch (kind) {
    case ChannelKind::RealtimeSample:
      profile.name = "realtime_sample";
      profile.qos = {Reliability::BestEffort, CongestionControl::Drop, Priority::RealTime, true};
      profile.allow_ack = false;
      profile.max_queue_cycles = 1;
      profile.max_hz = 50.0;
      break;
    case ChannelKind::Health:
      profile.name = "health";
      profile.qos = {Reliability::BestEffort, CongestionControl::Drop, Priority::DataLow, false};
      profile.allow_ack = false;
      profile.max_queue_cycles = 1;
      profile.max_hz = 2.0;
      break;
    case ChannelKind::CommandConfig:
      profile.name = "command_config";
      profile.qos = {Reliability::Reliable, CongestionControl::Block, Priority::InteractiveHigh, true};
      profile.allow_ack = true;
      profile.max_queue_cycles = 0;
      break;
    case ChannelKind::BulkDebug:
      profile.name = "bulk_debug";
      profile.qos = {Reliability::BestEffort, CongestionControl::Drop, Priority::Background, false};
      profile.allow_ack = false;
      profile.max_queue_cycles = 2;
      profile.max_bandwidth_kbps = 50;
      break;
  }
  return profile;
}

bool validateChannelProfile(const ChannelProfile& profile, std::string* error) {
  auto fail = [error](const std::string& message) {
    if (error) {
      *error = message;
    }
    return false;
  };

  if (profile.name.empty()) {
    return fail("channel profile name is required");
  }
  if (profile.kind == ChannelKind::RealtimeSample) {
    if (profile.allow_ack) {
      return fail("realtime_sample must not use per-sample ACK");
    }
    if (profile.qos.congestion != CongestionControl::Drop) {
      return fail("realtime_sample must use drop congestion control");
    }
    if (profile.max_queue_cycles > 1) {
      return fail("realtime_sample max_queue_cycles must be <= 1");
    }
  }
  if (profile.kind == ChannelKind::CommandConfig &&
      profile.qos.reliability != Reliability::Reliable) {
    return fail("command_config must use reliable QoS");
  }
  if (profile.max_hz < 0.0 || !std::isfinite(profile.max_hz)) {
    return fail("max_hz must be finite and non-negative");
  }
  if (error) {
    error->clear();
  }
  return true;
}

std::string toString(ChannelKind kind) {
  switch (kind) {
    case ChannelKind::RealtimeSample:
      return "realtime_sample";
    case ChannelKind::Health:
      return "health";
    case ChannelKind::CommandConfig:
      return "command_config";
    case ChannelKind::BulkDebug:
      return "bulk_debug";
  }
  return "unknown";
}

std::string toString(Reliability reliability) {
  return reliability == Reliability::Reliable ? "reliable" : "best_effort";
}

std::string toString(CongestionControl congestion) {
  return congestion == CongestionControl::Block ? "block" : "drop";
}

std::string toString(Priority priority) {
  switch (priority) {
    case Priority::RealTime:
      return "real_time";
    case Priority::InteractiveHigh:
      return "interactive_high";
    case Priority::DataHigh:
      return "data_high";
    case Priority::DataLow:
      return "data_low";
    case Priority::Background:
      return "background";
  }
  return "unknown";
}

}  // namespace swarm_sync::weaknet
