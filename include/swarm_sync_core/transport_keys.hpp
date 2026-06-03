#pragma once

#include <string>

#include "swarm_sync_core/sample_envelope.hpp"

namespace swarm_sync {

struct TransportKeyFields {
  std::string session_id;
  std::string task_id;
  std::string channel;
  std::string schema_id;
};

struct TransportKeyResult {
  bool ok{false};
  std::string key;
  std::string error;
};

TransportKeyResult buildTaskSampleKey(const TransportKeyFields& fields);
TransportKeyResult buildTaskSampleKey(const SampleEnvelope& envelope);

bool isValidTaskKeyField(const std::string& value);
bool validateTaskSampleKey(const std::string& key, std::string* reason = nullptr);

}  // namespace swarm_sync
