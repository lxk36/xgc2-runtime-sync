#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <ros/ros.h>

#include "periodic_sync/GetRuntimeStatus.h"
#include "periodic_sync/CycleSnapshot.h"
#include "periodic_sync/PeerSampleStatus.h"
#include "periodic_sync/RuntimeHealth.h"
#include "periodic_sync/SampleStats.h"
#include "periodic_sync/StartSession.h"
#include "periodic_sync/StopSession.h"
#include "periodic_sync/SyncedCycle.h"
#include "swarm_sync_core/sample_buffer.hpp"
#include "swarm_sync_core/snapshot_builder.hpp"
#include "swarm_sync_core/cycle_scheduler.hpp"
#include "swarm_sync_core/session.hpp"
#include "swarm_sync_ros1/adapter_config.hpp"

namespace {

int64_t toNs(const ros::Time& time) {
  return static_cast<int64_t>(time.sec) * 1000000000LL + static_cast<int64_t>(time.nsec);
}

int64_t toNs(const ros::Duration& duration) {
  return static_cast<int64_t>(duration.sec) * 1000000000LL + static_cast<int64_t>(duration.nsec);
}

ros::Duration fromNs(int64_t ns) {
  return ros::Duration(static_cast<double>(ns) / 1.0e9);
}

ros::Time timeFromNs(int64_t ns) {
  if (ns <= 0) {
    return ros::Time(0);
  }
  return ros::Time().fromNSec(static_cast<uint64_t>(ns));
}

uint8_t toMsgClockQuality(swarm_sync::ClockQuality quality) {
  return static_cast<uint8_t>(quality);
}

std::string stateName(swarm_sync::SessionState state) {
  switch (state) {
    case swarm_sync::SessionState::IDLE:
      return "IDLE";
    case swarm_sync::SessionState::CONFIGURED:
      return "CONFIGURED";
    case swarm_sync::SessionState::ARMED:
      return "ARMED";
    case swarm_sync::SessionState::RUNNING:
      return "RUNNING";
    case swarm_sync::SessionState::STOPPED:
      return "STOPPED";
    case swarm_sync::SessionState::ERROR:
      return "ERROR";
  }
  return "UNKNOWN";
}

}  // namespace

class SwarmRuntimeNode {
 public:
  bool init() {
    ros::NodeHandle pnh("~");
    pnh.param<std::string>("team_id", config_.team_id, "alpha");
    pnh.param<std::string>("session_id", config_.session_id, "demo_session_001");
    pnh.param<std::string>("task_id", config_.task_id, "distributed_task");
    pnh.param<std::string>("self_id", config_.self_id, "uav01");
    pnh.param<double>("frequency_hz", frequency_hz_, 20.0);
    pnh.param<bool>("auto_start", auto_start_, true);
    pnh.param<double>("start_delay_s", start_delay_s_, 0.1);
    pnh.param<double>("poll_rate_hz", poll_rate_hz_, 200.0);
    pnh.param<std::string>("expected_sample_channel", expected_sample_channel_, "runtime_state");

    if (!loadAdapterConfig(pnh)) {
      return false;
    }

    std::vector<std::string> required_participants;
    if (pnh.getParam("required_participants", required_participants) &&
        !required_participants.empty()) {
      config_.required_participants = required_participants;
    } else {
      config_.required_participants = {config_.self_id};
    }
    const auto period_ns = swarm_sync::CycleScheduler::periodNsFromFrequencyHz(frequency_hz_);
    if (!period_ns) {
      ROS_ERROR_STREAM("frequency_hz must be in ["
                       << swarm_sync::CycleScheduler::kMinFrequencyHz << ", "
                       << swarm_sync::CycleScheduler::kMaxFrequencyHz << "], got "
                       << frequency_hz_);
      return false;
    }
    config_.period_ns = *period_ns;
    config_.epoch_ns = toNs(ros::Time::now()) + static_cast<int64_t>(start_delay_s_ * 1.0e9);
    configureExpectedSamples();

    if (!session_.configure(config_)) {
      ROS_ERROR_STREAM("failed to configure swarm runtime session: " << session_.reason());
      return false;
    }

    swarm_sync::ClockState clock;
    clock.clock_ok = true;
    clock.quality = swarm_sync::ClockQuality::OK;
    clock.source = "ros_time";
    if (auto_start_ && !startSession(config_.epoch_ns, config_.period_ns, clock)) {
      ROS_ERROR_STREAM("failed to auto-start swarm runtime session: " << session_.reason());
      return false;
    }

    cycle_pub_ = nh_.advertise<periodic_sync::SyncedCycle>("/swarm_sync/cycle", 10, false);
    snapshot_pub_ = nh_.advertise<periodic_sync::CycleSnapshot>("cycle_snapshot", 10, false);
    sample_stats_pub_ = nh_.advertise<periodic_sync::SampleStats>("sample_stats", 10, false);
    health_pub_ = nh_.advertise<periodic_sync::RuntimeHealth>("/swarm_sync/runtime_health", 2, true);
    start_srv_ = nh_.advertiseService("/swarm_sync/start_session", &SwarmRuntimeNode::startCallback, this);
    stop_srv_ = nh_.advertiseService("/swarm_sync/stop_session", &SwarmRuntimeNode::stopCallback, this);
    status_srv_ = nh_.advertiseService("/swarm_sync/get_runtime_status", &SwarmRuntimeNode::statusCallback, this);

    timer_ = nh_.createTimer(
        ros::Duration(1.0 / std::max(1.0, poll_rate_hz_)),
        &SwarmRuntimeNode::timerCallback,
        this);

    ROS_INFO_STREAM("swarm_runtime_node ready: session=" << config_.session_id
                    << " self=" << config_.self_id
                    << " frequency_hz=" << frequency_hz_
                    << " " << adapter_summary_);
    return true;
  }

 private:
  bool startSession(int64_t epoch_ns, int64_t period_ns, const swarm_sync::ClockState& clock) {
    if (!swarm_sync::CycleScheduler::isValidPeriodNs(period_ns)) {
      session_.markError("period_ns outside supported 0.5-50 Hz range");
      return false;
    }
    config_.epoch_ns = epoch_ns;
    config_.period_ns = period_ns;
    if (!session_.configure(config_)) {
      return false;
    }
    configureExpectedSamples();
    if (!session_.arm(epoch_ns, clock)) {
      return false;
    }

    swarm_sync::CycleSchedulerConfig scheduler_config;
    scheduler_config.session_id = config_.session_id;
    scheduler_config.task_id = config_.task_id;
    scheduler_config.epoch_ns = epoch_ns;
    scheduler_config.period_ns = period_ns;
    scheduler_config.start_cycle = config_.start_cycle;
    scheduler_config.max_cycle = config_.max_cycle;
    if (!scheduler_.configure(scheduler_config)) {
      session_.markError("failed to configure cycle scheduler");
      return false;
    }
    scheduler_.start();
    return true;
  }

  bool startCallback(periodic_sync::StartSession::Request& request,
                     periodic_sync::StartSession::Response& response) {
    auto next_config = config_;
    if (!request.session_id.empty()) {
      next_config.session_id = request.session_id;
    }
    if (!request.task_id.empty()) {
      next_config.task_id = request.task_id;
    }
    int64_t requested_period_ns = next_config.period_ns;
    if (request.period.toSec() > 0.0) {
      requested_period_ns = toNs(request.period);
    }
    if (!swarm_sync::CycleScheduler::isValidPeriodNs(requested_period_ns)) {
      response.accepted = false;
      response.reason = "period_ns outside supported 0.5-50 Hz range";
      return true;
    }
    next_config.period_ns = requested_period_ns;
    next_config.epoch_ns = toNs(request.epoch_time);
    config_ = next_config;

    swarm_sync::ClockState clock;
    clock.clock_ok = true;
    clock.quality = swarm_sync::ClockQuality::OK;
    clock.source = "ros_time";
    response.accepted = startSession(config_.epoch_ns, config_.period_ns, clock);
    response.reason = response.accepted ? "accepted" : session_.reason();
    return true;
  }

  bool stopCallback(periodic_sync::StopSession::Request& request,
                    periodic_sync::StopSession::Response& response) {
    scheduler_.stop();
    session_.stop(request.reason.empty() ? "stopped by service" : request.reason);
    response.accepted = true;
    response.message = "stopped";
    publishHealth();
    return true;
  }

  bool statusCallback(periodic_sync::GetRuntimeStatus::Request&,
                      periodic_sync::GetRuntimeStatus::Response& response) {
    response.running = session_.running();
    response.state = stateName(session_.state());
    response.current_cycle = current_cycle_;
    response.clock_ok = true;
    response.zenoh_connected = false;
    response.reason = runtimeReason();
    return true;
  }

  void timerCallback(const ros::TimerEvent&) {
    const ros::Time now = ros::Time::now();
    swarm_sync::ClockState clock;
    clock.clock_ok = true;
    clock.quality = swarm_sync::ClockQuality::OK;
    clock.source = "ros_time";

    session_.startIfDue(toNs(now), clock);
    const auto event = scheduler_.tick(toNs(now), clock.clock_ok);
    if (event) {
      current_cycle_ = event->cycle_id;
      last_jitter_ns_ = event->jitter_ns;
      publishCycle(*event, now);
      publishSnapshot(*event, now, clock);
      publishHealth();
    }
  }

  void publishCycle(const swarm_sync::CycleEvent& event, const ros::Time& stamp) {
    periodic_sync::SyncedCycle msg;
    msg.header.stamp = stamp;
    msg.team_id = config_.team_id;
    msg.session_id = config_.session_id;
    msg.task_id = config_.task_id;
    msg.self_id = config_.self_id;
    msg.cycle_id = event.cycle_id;
    msg.expected_time = ros::Time().fromNSec(static_cast<uint64_t>(event.expected_time_ns));
    msg.actual_time = ros::Time().fromNSec(static_cast<uint64_t>(event.actual_time_ns));
    msg.period = fromNs(event.period_ns);
    msg.jitter = fromNs(event.jitter_ns);
    msg.clock_ok = event.clock_ok;
    msg.clock_quality = toMsgClockQuality(swarm_sync::ClockQuality::OK);
    msg.session_running = session_.running();
    msg.runtime_degraded = false;
    cycle_pub_.publish(msg);
  }

  void publishSnapshot(const swarm_sync::CycleEvent& event,
                       const ros::Time& stamp,
                       const swarm_sync::ClockState& clock) {
    swarm_sync::SnapshotBuildRequest request;
    request.session_id = config_.session_id;
    request.task_id = config_.task_id;
    request.self_id = config_.self_id;
    request.cycle_id = event.cycle_id;
    request.cycle_start_ns = event.expected_time_ns;
    request.period_ns = event.period_ns;
    request.build_time_ns = toNs(stamp);
    request.local_clock = clock;
    request.local_runtime.running = session_.running();
    request.local_runtime.zenoh_connected = false;
    request.local_runtime.current_cycle = event.cycle_id;
    request.local_runtime.jitter_ns = event.jitter_ns;
    request.local_runtime.state = stateName(session_.state());
    request.local_runtime.reason = session_.reason();
    request.expected_samples = expected_samples_;

    const auto snapshot = snapshot_builder_.build(request, sample_buffer_);
    const auto snapshot_msg = toSnapshotMsg(snapshot, stamp);
    snapshot_pub_.publish(snapshot_msg);
    sample_stats_pub_.publish(toSampleStatsMsg(snapshot, stamp));
  }

  periodic_sync::CycleSnapshot toSnapshotMsg(const swarm_sync::CycleSnapshot& snapshot,
                                             const ros::Time& stamp) const {
    periodic_sync::CycleSnapshot msg;
    msg.header.stamp = stamp;
    msg.team_id = config_.team_id;
    msg.session_id = snapshot.session_id;
    msg.task_id = snapshot.task_id;
    msg.self_id = snapshot.self_id;
    msg.cycle_id = snapshot.cycle_id;
    msg.cycle_start_time = timeFromNs(snapshot.t_cycle_start_ns);
    msg.period = fromNs(snapshot.period_ns);
    msg.samples.reserve(snapshot.samples.size());
    for (const auto& sample : snapshot.samples) {
      msg.samples.push_back(toPeerSampleStatusMsg(sample));
    }
    msg.fresh_count = snapshot.stats.fresh_count;
    msg.missing_count = snapshot.stats.missing_count;
    msg.late_count = snapshot.stats.late_count;
    msg.wrong_cycle_count = snapshot.stats.wrong_cycle_count;
    msg.bad_count = snapshot.stats.bad_count;
    msg.all_required_fresh = snapshot.all_required_fresh;
    msg.all_required_usable = snapshot.all_required_usable;
    msg.local_clock_ok = snapshot.local_clock.clock_ok;
    msg.runtime_degraded = !snapshot.local_runtime.running || !snapshot.local_clock.clock_ok;
    return msg;
  }

  periodic_sync::PeerSampleStatus toPeerSampleStatusMsg(
      const swarm_sync::PeerSampleView& sample) const {
    periodic_sync::PeerSampleStatus msg;
    msg.peer_id = sample.peer_id;
    msg.channel = sample.channel;
    msg.status = static_cast<uint8_t>(sample.status);
    msg.expected_target_cycle = sample.expected_target_cycle;
    msg.received_target_cycle = sample.received_target_cycle;
    if (sample.sample) {
      msg.seq = sample.sample->seq;
      msg.schema_id = sample.sample->schema_id;
    }
    msg.local_receive_time = timeFromNs(sample.t_local_receive_ns);
    msg.receive_latency = fromNs(sample.receive_latency_ns);
    msg.fresh_for_this_cycle = sample.fresh_for_this_cycle;
    msg.late_for_this_cycle = sample.late_for_this_cycle;
    msg.duplicate = sample.duplicate;
    msg.wrong_cycle = sample.wrong_cycle;
    msg.bad_schema = sample.bad_schema;
    msg.bad_crc = sample.bad_crc;
    msg.sender_clock_bad = sample.sender_clock_bad;
    msg.drop_reason = sample.drop_reason;
    return msg;
  }

  periodic_sync::SampleStats toSampleStatsMsg(const swarm_sync::CycleSnapshot& snapshot,
                                              const ros::Time& stamp) const {
    periodic_sync::SampleStats msg;
    msg.header.stamp = stamp;
    msg.team_id = config_.team_id;
    msg.session_id = snapshot.session_id;
    msg.task_id = snapshot.task_id;
    msg.self_id = snapshot.self_id;
    msg.cycle_id = snapshot.cycle_id;
    msg.received_count = snapshot.stats.fresh_count + snapshot.stats.late_count;
    msg.fresh_count = snapshot.stats.fresh_count;
    msg.missing_count = snapshot.stats.missing_count;
    msg.late_count = snapshot.stats.late_count;
    msg.wrong_cycle_count = snapshot.stats.wrong_cycle_count;
    for (const auto& sample : snapshot.samples) {
      if (sample.duplicate) {
        ++msg.duplicate_count;
      }
      if (sample.bad_schema) {
        ++msg.bad_schema_count;
      }
      if (sample.bad_crc) {
        ++msg.bad_payload_count;
      }
      if (sample.sender_clock_bad) {
        ++msg.sender_clock_bad_count;
      }
    }
    return msg;
  }

  void configureExpectedSamples() {
    expected_samples_.clear();
    swarm_sync::ChannelTiming timing;
    timing.payload_type = "bytes";
    timing.schema_id = expected_sample_channel_ + ".v1";
    timing.receive_cutoff_ns = config_.period_ns > 0 ? config_.period_ns : 50000000;
    timing.ttl_ns = timing.receive_cutoff_ns;
    for (const auto& participant : config_.required_participants) {
      swarm_sync::ExpectedSample expected;
      expected.peer_id = participant;
      expected.channel = expected_sample_channel_;
      expected.required = true;
      expected.timing = timing;
      expected_samples_.push_back(std::move(expected));
    }
  }

  void publishHealth() {
    periodic_sync::RuntimeHealth msg;
    msg.header.stamp = ros::Time::now();
    msg.team_id = config_.team_id;
    msg.session_id = config_.session_id;
    msg.self_id = config_.self_id;
    msg.alive = true;
    msg.session_running = session_.running();
    msg.zenoh_connected = false;
    msg.clock_ok = true;
    msg.clock_offset_ns = 0;
    msg.clock_uncertainty_ns = 0;
    msg.clock_quality = toMsgClockQuality(swarm_sync::ClockQuality::OK);
    msg.current_cycle = current_cycle_;
    msg.cycle_jitter_p95 = fromNs(last_jitter_ns_);
    msg.cycle_jitter_p99 = fromNs(last_jitter_ns_);
    msg.state = stateName(session_.state());
    msg.reason = runtimeReason();
    health_pub_.publish(msg);
  }

  bool loadAdapterConfig(const ros::NodeHandle& pnh) {
    std::string config_path;
    pnh.param<std::string>("ros1_adapters_config", config_path, "");

    XmlRpc::XmlRpcValue raw_config;
    if (pnh.getParam("ros1_adapters", raw_config)) {
      const auto parsed = swarm_sync_ros1::parseAdapterConfigXmlRpc(raw_config);
      if (!parsed.ok) {
        ROS_ERROR_STREAM("failed to load ros1_adapters config: " << parsed.error);
        return false;
      }
      adapter_config_ = parsed.config;
      adapter_summary_ = swarm_sync_ros1::summarizeAdapterConfig(adapter_config_);
      ROS_INFO_STREAM("loaded ros1_adapters config: " << adapter_summary_);
      return true;
    }

    if (!config_path.empty()) {
      ROS_ERROR_STREAM("ros1_adapters_config is set to " << config_path
                       << " but private parameter ros1_adapters was not loaded");
      return false;
    }

    adapter_config_ = swarm_sync_ros1::defaultAdapterConfig();
    adapter_summary_ = swarm_sync_ros1::summarizeAdapterConfig(adapter_config_) + " source=built_in_default";
    ROS_WARN_STREAM("no ros1_adapters config loaded; using " << adapter_summary_);
    return true;
  }

  std::string runtimeReason() const {
    if (session_.reason().empty()) {
      return adapter_summary_;
    }
    return session_.reason() + "; " + adapter_summary_;
  }

  ros::NodeHandle nh_;
  swarm_sync::SessionConfig config_;
  swarm_sync_ros1::AdapterConfig adapter_config_;
  std::string adapter_summary_;
  swarm_sync::SessionManager session_;
  swarm_sync::CycleScheduler scheduler_;
  swarm_sync::SnapshotBuilder snapshot_builder_;
  swarm_sync::SampleBuffer sample_buffer_;
  std::vector<swarm_sync::ExpectedSample> expected_samples_;
  ros::Publisher cycle_pub_;
  ros::Publisher snapshot_pub_;
  ros::Publisher sample_stats_pub_;
  ros::Publisher health_pub_;
  ros::ServiceServer start_srv_;
  ros::ServiceServer stop_srv_;
  ros::ServiceServer status_srv_;
  ros::Timer timer_;
  uint64_t current_cycle_ = 0;
  int64_t last_jitter_ns_ = 0;
  double frequency_hz_ = 20.0;
  double poll_rate_hz_ = 200.0;
  double start_delay_s_ = 0.1;
  std::string expected_sample_channel_ = "runtime_state";
  bool auto_start_ = true;
};

int main(int argc, char** argv) {
  ros::init(argc, argv, "swarm_runtime_node");
  SwarmRuntimeNode node;
  if (!node.init()) {
    return 1;
  }
  ros::spin();
  return 0;
}
