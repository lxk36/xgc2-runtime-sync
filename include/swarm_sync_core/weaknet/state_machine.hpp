#pragma once

#include <map>
#include <string>

#include "swarm_sync_core/weaknet/link_health.hpp"

namespace swarm_sync::weaknet {

struct StateTransition {
  LinkState previous{LinkState::Good};
  LinkState current{LinkState::Good};
  bool changed{false};
  std::string reason;
  std::map<std::string, double> evidence;
};

class WeaknetStateMachine {
public:
  StateTransition update(const PeerChannelHealth& health);
  LinkState current() const { return current_; }

private:
  LinkState current_{LinkState::Good};
};

}  // namespace swarm_sync::weaknet
