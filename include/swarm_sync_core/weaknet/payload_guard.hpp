#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace swarm_sync::weaknet {

enum class PayloadGuardAction : uint8_t {
  Accept = 0,
  Warn = 1,
  Reject = 2
};

struct PayloadGuardConfig {
  size_t warn_bytes{800};
  size_t reject_bytes{1200};
  bool allow_empty_payload{true};
  bool require_schema_id{true};
  bool require_payload_type{true};
};

struct PayloadGuardDecision {
  PayloadGuardAction action{PayloadGuardAction::Accept};
  size_t payload_bytes{0};
  std::string reason;
};

PayloadGuardDecision checkPayload(const PayloadGuardConfig& config,
                                  size_t payload_bytes,
                                  const std::string& payload_type,
                                  const std::string& schema_id);

}  // namespace swarm_sync::weaknet
