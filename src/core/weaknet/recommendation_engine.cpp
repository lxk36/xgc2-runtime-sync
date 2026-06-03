#include "swarm_sync_core/weaknet/recommendation_engine.hpp"

namespace swarm_sync::weaknet {

std::vector<RuntimeRecommendation> RecommendationEngine::evaluate(
    uint64_t cycle_id,
    const PeerChannelHealth& health,
    const BudgetReport* budget) const {
  std::vector<RuntimeRecommendation> recommendations;
  auto add = [&](RecommendationLevel level,
                 RecommendationType type,
                 double confidence,
                 const std::string& reason) {
    RuntimeRecommendation rec;
    rec.cycle_id = cycle_id;
    rec.level = level;
    rec.type = type;
    rec.channel = health.channel;
    rec.peer = health.peer_id;
    rec.confidence = confidence;
    rec.reason = reason;
    rec.evidence = {
        {"fresh_ratio", health.fresh_ratio},
        {"late_ratio", health.late_ratio},
        {"missing_ratio", health.missing_ratio},
        {"rx_latency_p99_ns", static_cast<double>(health.rx_latency_p99_ns)},
    };
    recommendations.push_back(std::move(rec));
  };

  if (health.state == LinkState::Degraded) {
    add(RecommendationLevel::Warn, RecommendationType::DisableChannel, 0.60,
        "consider disabling bulk/debug channels; realtime remains suggestion-only");
  } else if (health.state == LinkState::Congested) {
    add(RecommendationLevel::Warn, RecommendationType::ChangeFrequency, 0.75,
        "consider lowering realtime frequency");
    add(RecommendationLevel::Warn, RecommendationType::ChangeTargetCycleOffset, 0.70,
        "consider increasing target_cycle_offset");
  } else if (health.state == LinkState::Partitioned) {
    add(RecommendationLevel::Critical, RecommendationType::ChangeTopology, 0.85,
        "peer appears partitioned; consider router/topology recovery");
  }

  if (budget && !budget->accepted) {
    RuntimeRecommendation rec;
    rec.cycle_id = cycle_id;
    rec.level = RecommendationLevel::Critical;
    rec.type = RecommendationType::ReducePayload;
    rec.channel = health.channel;
    rec.peer = health.peer_id;
    rec.confidence = 0.95;
    rec.reason = budget->reason;
    rec.evidence = {
        {"budget_utilization_ratio", budget->utilization_ratio},
        {"total_realtime_bps", budget->total_realtime_bps},
        {"max_realtime_bps", budget->max_realtime_bps},
    };
    recommendations.push_back(std::move(rec));
  }

  return recommendations;
}

std::string toString(RecommendationLevel level) {
  switch (level) {
    case RecommendationLevel::Info:
      return "info";
    case RecommendationLevel::Warn:
      return "warn";
    case RecommendationLevel::Critical:
      return "critical";
  }
  return "unknown";
}

std::string toString(RecommendationType type) {
  switch (type) {
    case RecommendationType::ChangeFrequency:
      return "change_frequency";
    case RecommendationType::ChangeTargetCycleOffset:
      return "change_target_cycle_offset";
    case RecommendationType::DisableChannel:
      return "disable_channel";
    case RecommendationType::ReducePayload:
      return "reduce_payload";
    case RecommendationType::ChangeTopology:
      return "change_topology";
  }
  return "unknown";
}

}  // namespace swarm_sync::weaknet
