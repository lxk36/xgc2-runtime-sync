#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace swarm_sync {

struct CycleEvent {
  std::string session_id;
  std::string task_id;
  uint64_t cycle_id{0};
  int64_t expected_time_ns{0};
  int64_t actual_time_ns{0};
  int64_t jitter_ns{0};
  int64_t period_ns{0};
  bool clock_ok{false};
};

struct CycleSchedulerConfig {
  std::string session_id;
  std::string task_id;
  int64_t epoch_ns{0};
  int64_t period_ns{0};
  uint64_t start_cycle{0};
  uint64_t max_cycle{0};
};

class CycleScheduler {
public:
  using Callback = std::function<void(const CycleEvent&)>;

  bool configure(const CycleSchedulerConfig& config);
  bool configure(std::string session_id,
                 std::string task_id,
                 int64_t epoch_ns,
                 int64_t period_ns);

  void setCallback(Callback cb);
  void start();
  void stop();

  uint64_t computeCycleId(int64_t now_ns) const;
  int64_t expectedTimeNs(uint64_t cycle_id) const;
  std::optional<CycleEvent> tick(int64_t now_ns, bool clock_ok = true);

  bool running() const;
  const CycleSchedulerConfig& config() const;

private:
  CycleSchedulerConfig config_;
  Callback callback_;
  bool running_{false};
  bool has_emitted_{false};
  uint64_t last_emitted_cycle_{0};
};

}  // namespace swarm_sync
