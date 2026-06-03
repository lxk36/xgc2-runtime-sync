#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include <ros/ros.h>

#include "periodic_sync/GetRuntimeStatus.h"
#include "periodic_sync/RuntimeHealth.h"
#include "periodic_sync/StartSession.h"
#include "periodic_sync/StopSession.h"
#include "periodic_sync/SyncedCycle.h"
#include "swarm_sync_core/cycle_scheduler.hpp"
#include "swarm_sync_core/session.hpp"

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

    config_.required_participants = {config_.self_id};
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
                    << " frequency_hz=" << frequency_hz_);
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
    response.reason = session_.reason();
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
    msg.reason = session_.reason();
    health_pub_.publish(msg);
  }

  ros::NodeHandle nh_;
  swarm_sync::SessionConfig config_;
  swarm_sync::SessionManager session_;
  swarm_sync::CycleScheduler scheduler_;
  ros::Publisher cycle_pub_;
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
