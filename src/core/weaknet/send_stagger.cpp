#include "swarm_sync_core/weaknet/send_stagger.hpp"

#include <functional>

namespace swarm_sync::weaknet {

DeterministicSendStagger::DeterministicSendStagger(SendStaggerConfig cfg) : cfg_(cfg) {}

SendStaggerDecision DeterministicSendStagger::decide(const std::string& session_id,
                                                     const std::string& channel,
                                                     const std::string& sender_id,
                                                     uint64_t cycle_id) const {
  SendStaggerDecision decision;
  if (!cfg_.enabled || cfg_.max_spread_ns <= 0 || cfg_.slot_width_ns <= 0) {
    decision.reason = "stagger disabled";
    return decision;
  }
  const uint32_t slot_count = static_cast<uint32_t>(cfg_.max_spread_ns / cfg_.slot_width_ns) + 1;
  const std::string key = session_id + "|" + channel + "|" + sender_id + "|" + std::to_string(cycle_id);
  const auto hash = std::hash<std::string>{}(key);
  decision.enabled = true;
  decision.slot_index = static_cast<uint32_t>(hash % slot_count);
  decision.offset_ns = static_cast<int64_t>(decision.slot_index) * cfg_.slot_width_ns;
  if (decision.offset_ns > cfg_.max_spread_ns) {
    decision.offset_ns = cfg_.max_spread_ns;
  }
  decision.reason = "deterministic_micro_slot";
  return decision;
}

}  // namespace swarm_sync::weaknet
