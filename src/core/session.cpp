#include "swarm_sync_core/session.hpp"

#include <algorithm>

namespace swarm_sync {

bool SessionManager::configure(const SessionConfig& config) {
  std::string reason;
  if (!isValidConfig(config, &reason)) {
    markError(reason);
    return false;
  }

  config_ = config;
  state_ = SessionState::CONFIGURED;
  reason_.clear();
  return true;
}

bool SessionManager::arm(int64_t epoch_ns, const ClockState& clock) {
  if (state_ != SessionState::CONFIGURED && state_ != SessionState::STOPPED) {
    markError("cannot arm before configured");
    return false;
  }
  if (!clock.clock_ok || clock.quality == ClockQuality::BAD) {
    markError("cannot arm with bad clock");
    return false;
  }

  config_.epoch_ns = epoch_ns;
  state_ = SessionState::ARMED;
  reason_.clear();
  return true;
}

bool SessionManager::startIfDue(int64_t now_ns, const ClockState& clock) {
  if (state_ == SessionState::RUNNING) {
    return true;
  }
  if (state_ != SessionState::ARMED) {
    return false;
  }
  if (!clock.clock_ok || clock.quality == ClockQuality::BAD) {
    markError("cannot start with bad clock");
    return false;
  }
  if (now_ns < config_.epoch_ns) {
    return false;
  }

  state_ = SessionState::RUNNING;
  reason_.clear();
  return true;
}

void SessionManager::stop(const std::string& reason) {
  reason_ = reason;
  state_ = SessionState::STOPPED;
}

void SessionManager::markError(const std::string& reason) {
  reason_ = reason;
  state_ = SessionState::ERROR;
}

SessionState SessionManager::state() const {
  return state_;
}

const SessionConfig& SessionManager::config() const {
  return config_;
}

const std::string& SessionManager::reason() const {
  return reason_;
}

bool SessionManager::running() const {
  return state_ == SessionState::RUNNING;
}

bool SessionManager::isValidConfig(const SessionConfig& config, std::string* reason) {
  if (config.team_id.empty()) {
    if (reason) *reason = "team_id is required";
    return false;
  }
  if (config.session_id.empty()) {
    if (reason) *reason = "session_id is required";
    return false;
  }
  if (config.task_id.empty()) {
    if (reason) *reason = "task_id is required";
    return false;
  }
  if (config.self_id.empty()) {
    if (reason) *reason = "self_id is required";
    return false;
  }
  if (config.period_ns <= 0) {
    if (reason) *reason = "period_ns must be positive";
    return false;
  }
  if (config.max_cycle > 0 && config.max_cycle < config.start_cycle) {
    if (reason) *reason = "max_cycle must be >= start_cycle";
    return false;
  }
  if (std::find(config.required_participants.begin(),
                config.required_participants.end(),
                config.self_id) == config.required_participants.end()) {
    if (reason) *reason = "self_id must be in required_participants";
    return false;
  }
  return true;
}

}  // namespace swarm_sync
