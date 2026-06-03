#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "swarm_sync_core/weaknet/budget_guard.hpp"
#include "swarm_sync_core/weaknet/link_health.hpp"

namespace swarm_sync::weaknet {

enum class RecommendationLevel : uint8_t { Info = 0, Warn = 1, Critical = 2 };
enum class RecommendationType : uint8_t {
  ChangeFrequency = 0,
  ChangeTargetCycleOffset = 1,
  DisableChannel = 2,
  ReducePayload = 3,
  ChangeTopology = 4
};

struct RuntimeRecommendation {
  uint64_t cycle_id{0};
  RecommendationLevel level{RecommendationLevel::Info};
  RecommendationType type{RecommendationType::ChangeFrequency};
  std::string channel;
  std::string peer;
  double confidence{0.0};
  std::string reason;
  std::map<std::string, double> evidence;
};

class RecommendationEngine {
public:
  std::vector<RuntimeRecommendation> evaluate(uint64_t cycle_id,
                                              const PeerChannelHealth& health,
                                              const BudgetReport* budget) const;
};

std::string toString(RecommendationLevel level);
std::string toString(RecommendationType type);

}  // namespace swarm_sync::weaknet
