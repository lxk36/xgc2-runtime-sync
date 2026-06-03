#include "swarm_sync_core/weaknet/state_machine.hpp"

namespace swarm_sync::weaknet {

StateTransition WeaknetStateMachine::update(const PeerChannelHealth& health) {
  StateTransition transition;
  transition.previous = current_;
  transition.evidence = {
      {"fresh_ratio", health.fresh_ratio},
      {"late_ratio", health.late_ratio},
      {"missing_ratio", health.missing_ratio},
      {"rx_latency_p99_ns", static_cast<double>(health.rx_latency_p99_ns)},
      {"seq_gap_count", static_cast<double>(health.seq_gap_count)},
  };

  LinkState next = current_;
  if (current_ == LinkState::Partitioned) {
    next = health.fresh_ratio > 0.50 ? LinkState::Recovering : LinkState::Partitioned;
  } else if (current_ == LinkState::Recovering) {
    next = (health.fresh_ratio > 0.995 && health.late_ratio < 0.005 &&
            health.missing_ratio < 0.001)
               ? LinkState::Good
               : health.state;
  } else {
    next = health.state;
  }

  current_ = next;
  transition.current = current_;
  transition.changed = transition.previous != transition.current;
  transition.reason = health.primary_reason.empty() ? "state evaluated" : health.primary_reason;
  return transition;
}

}  // namespace swarm_sync::weaknet
