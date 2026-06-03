#include "swarm_sync_core/transport_keys.hpp"

#include <vector>

namespace swarm_sync {
namespace {

constexpr const char* kPrefix = "swarm_sync";
constexpr const char* kVersion = "v1";

bool isValidChar(char value) {
  return (value >= 'a' && value <= 'z') ||
         (value >= 'A' && value <= 'Z') ||
         (value >= '0' && value <= '9') ||
         value == '_' ||
         value == '-' ||
         value == '.';
}

bool validateField(const std::string& name, const std::string& value, std::string* reason) {
  if (value.empty()) {
    if (reason) *reason = name + " is required";
    return false;
  }
  for (const char value_char : value) {
    if (!isValidChar(value_char)) {
      if (reason) *reason = name + " contains an illegal character";
      return false;
    }
  }
  return true;
}

std::vector<std::string> splitSampleKey(const std::string& key) {
  std::vector<std::string> parts;
  size_t start = 0;
  while (start <= key.size()) {
    const size_t slash = key.find('/', start);
    if (slash == std::string::npos) {
      parts.push_back(key.substr(start));
      break;
    }
    parts.push_back(key.substr(start, slash - start));
    start = slash + 1;
  }
  return parts;
}

}  // namespace

TransportKeyResult buildTaskSampleKey(const TransportKeyFields& fields) {
  std::string reason;
  if (!validateField("session_id", fields.session_id, &reason) ||
      !validateField("task_id", fields.task_id, &reason) ||
      !validateField("channel", fields.channel, &reason) ||
      !validateField("schema_id", fields.schema_id, &reason)) {
    return TransportKeyResult{false, "", reason};
  }

  TransportKeyResult result;
  result.ok = true;
  result.key = std::string(kPrefix) + "/" + kVersion +
               "/session/" + fields.session_id +
               "/task/" + fields.task_id +
               "/channel/" + fields.channel +
               "/schema/" + fields.schema_id;
  return result;
}

TransportKeyResult buildTaskSampleKey(const SampleEnvelope& envelope) {
  TransportKeyFields fields;
  fields.session_id = envelope.session_id;
  fields.task_id = envelope.task_id;
  fields.channel = envelope.channel;
  fields.schema_id = envelope.schema_id;
  return buildTaskSampleKey(fields);
}

bool isValidTaskKeyField(const std::string& value) {
  return validateField("field", value, nullptr);
}

bool validateTaskSampleKey(const std::string& key, std::string* reason) {
  const auto parts = splitSampleKey(key);
  if (parts.size() != 10) {
    if (reason) *reason = "sample key must have the task-semantic sample shape";
    return false;
  }

  if (parts[0] != kPrefix ||
      parts[1] != kVersion ||
      parts[2] != "session" ||
      parts[4] != "task" ||
      parts[6] != "channel" ||
      parts[8] != "schema") {
    if (reason) *reason = "sample key must use task-semantic labels";
    return false;
  }

  return validateField("session_id", parts[3], reason) &&
         validateField("task_id", parts[5], reason) &&
         validateField("channel", parts[7], reason) &&
         validateField("schema_id", parts[9], reason);
}

}  // namespace swarm_sync
