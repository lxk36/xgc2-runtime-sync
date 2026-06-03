#include "swarm_sync_core/weaknet/payload_guard.hpp"

namespace swarm_sync::weaknet {

PayloadGuardDecision checkPayload(const PayloadGuardConfig& config,
                                  size_t payload_bytes,
                                  const std::string& payload_type,
                                  const std::string& schema_id) {
  PayloadGuardDecision decision;
  decision.payload_bytes = payload_bytes;
  if (!config.allow_empty_payload && payload_bytes == 0) {
    decision.action = PayloadGuardAction::Reject;
    decision.reason = "empty payload is not allowed";
    return decision;
  }
  if (config.require_payload_type && payload_type.empty()) {
    decision.action = PayloadGuardAction::Reject;
    decision.reason = "payload_type is required";
    return decision;
  }
  if (config.require_schema_id && schema_id.empty()) {
    decision.action = PayloadGuardAction::Reject;
    decision.reason = "schema_id is required";
    return decision;
  }
  if (config.reject_bytes > 0 && payload_bytes > config.reject_bytes) {
    decision.action = PayloadGuardAction::Reject;
    decision.reason = "payload exceeds reject threshold";
    return decision;
  }
  if (config.warn_bytes > 0 && payload_bytes > config.warn_bytes) {
    decision.action = PayloadGuardAction::Warn;
    decision.reason = "payload exceeds warning threshold";
    return decision;
  }
  decision.action = PayloadGuardAction::Accept;
  decision.reason = "accepted";
  return decision;
}

}  // namespace swarm_sync::weaknet
