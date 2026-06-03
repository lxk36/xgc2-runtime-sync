#pragma once

#include <cstdint>
#include <string>

namespace swarm_sync::weaknet {

struct SendStaggerConfig {
  bool enabled{false};
  int64_t max_spread_ns{0};
  int64_t slot_width_ns{500000};
};

struct SendStaggerDecision {
  bool enabled{false};
  uint32_t slot_index{0};
  int64_t offset_ns{0};
  std::string reason;
};

class DeterministicSendStagger {
public:
  explicit DeterministicSendStagger(SendStaggerConfig cfg);

  SendStaggerDecision decide(const std::string& session_id,
                             const std::string& channel,
                             const std::string& sender_id,
                             uint64_t cycle_id) const;

private:
  SendStaggerConfig cfg_;
};

}  // namespace swarm_sync::weaknet
