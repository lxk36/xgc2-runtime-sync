#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "swarm_sync_core/types.hpp"

namespace swarm_sync {

enum class SessionState {
  IDLE,
  CONFIGURED,
  ARMED,
  RUNNING,
  STOPPED,
  ERROR
};

struct SessionConfig {
  std::string team_id;
  std::string session_id;
  std::string task_id;
  std::string self_id;

  std::vector<std::string> required_participants;
  std::vector<std::string> optional_participants;

  int64_t epoch_ns{0};
  int64_t period_ns{0};
  uint64_t start_cycle{0};
  uint64_t max_cycle{0};
};

class SessionManager {
public:
  bool configure(const SessionConfig& config);
  bool arm(int64_t epoch_ns, const ClockState& clock = ClockState{true, 0, 0, ClockQuality::OK, ""});
  bool startIfDue(int64_t now_ns, const ClockState& clock = ClockState{true, 0, 0, ClockQuality::OK, ""});
  void stop(const std::string& reason);
  void markError(const std::string& reason);

  SessionState state() const;
  const SessionConfig& config() const;
  const std::string& reason() const;
  bool running() const;

private:
  static bool isValidConfig(const SessionConfig& config, std::string* reason);

  SessionConfig config_;
  SessionState state_{SessionState::IDLE};
  std::string reason_;
};

}  // namespace swarm_sync
