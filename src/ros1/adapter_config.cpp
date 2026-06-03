#include "swarm_sync_ros1/adapter_config.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <tuple>

#include "swarm_sync_core/transport_keys.hpp"

namespace swarm_sync_ros1 {
namespace {

bool hasMember(XmlRpc::XmlRpcValue& value, const std::string& key) {
  return value.getType() == XmlRpc::XmlRpcValue::TypeStruct && value.hasMember(key);
}

std::string typeName(XmlRpc::XmlRpcValue::Type type) {
  switch (type) {
    case XmlRpc::XmlRpcValue::TypeInvalid:
      return "invalid";
    case XmlRpc::XmlRpcValue::TypeBoolean:
      return "bool";
    case XmlRpc::XmlRpcValue::TypeInt:
      return "int";
    case XmlRpc::XmlRpcValue::TypeDouble:
      return "double";
    case XmlRpc::XmlRpcValue::TypeString:
      return "string";
    case XmlRpc::XmlRpcValue::TypeDateTime:
      return "datetime";
    case XmlRpc::XmlRpcValue::TypeBase64:
      return "base64";
    case XmlRpc::XmlRpcValue::TypeArray:
      return "array";
    case XmlRpc::XmlRpcValue::TypeStruct:
      return "struct";
  }
  return "unknown";
}

bool readString(XmlRpc::XmlRpcValue& value,
                const std::string& key,
                bool required,
                std::string* out,
                std::string* error) {
  if (!hasMember(value, key)) {
    if (required && error) *error = key + " is required";
    return !required;
  }
  XmlRpc::XmlRpcValue& field = value[key];
  if (field.getType() != XmlRpc::XmlRpcValue::TypeString) {
    if (error) *error = key + " must be a string, got " + typeName(field.getType());
    return false;
  }
  *out = static_cast<std::string>(field);
  return true;
}

bool readBool(XmlRpc::XmlRpcValue& value,
              const std::string& key,
              bool* out,
              std::string* error) {
  if (!hasMember(value, key)) {
    return true;
  }
  XmlRpc::XmlRpcValue& field = value[key];
  if (field.getType() != XmlRpc::XmlRpcValue::TypeBoolean) {
    if (error) *error = key + " must be a bool, got " + typeName(field.getType());
    return false;
  }
  *out = static_cast<bool>(field);
  return true;
}

bool readSize(XmlRpc::XmlRpcValue& value,
              const std::string& key,
              size_t* out,
              std::string* error) {
  if (!hasMember(value, key)) {
    return true;
  }
  XmlRpc::XmlRpcValue& field = value[key];
  if (field.getType() != XmlRpc::XmlRpcValue::TypeInt) {
    if (error) *error = key + " must be an int, got " + typeName(field.getType());
    return false;
  }
  const int raw = static_cast<int>(field);
  if (raw <= 0) {
    if (error) *error = key + " must be positive";
    return false;
  }
  *out = static_cast<size_t>(raw);
  return true;
}

std::string lowerCopy(const std::string& value) {
  std::string lowered = value;
  std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char value_char) {
    return static_cast<char>(std::tolower(value_char));
  });
  return lowered;
}

bool isReservedSystemTopic(const std::string& topic) {
  return topic == "/tf" ||
         topic == "/tf_static" ||
         topic == "/rosout" ||
         topic == "/clock";
}

bool isLargeDebugTopic(const std::string& topic) {
  const std::string lowered = lowerCopy(topic);
  return lowered.find("image") != std::string::npos ||
         lowered.find("pointcloud") != std::string::npos ||
         lowered.find("debug") != std::string::npos ||
         lowered.find("camera") != std::string::npos ||
         lowered.find("raw") != std::string::npos;
}

bool validateTaskField(const std::string& field_name,
                       const std::string& value,
                       std::string* error) {
  if (value.empty()) {
    if (error) *error = field_name + " is required";
    return false;
  }
  if (!swarm_sync::isValidTaskKeyField(value)) {
    if (error) *error = field_name + " contains an illegal character";
    return false;
  }
  return true;
}

std::string ruleLabel(const AdapterRule& rule, size_t index) {
  return rule.name.empty() ? ("rule[" + std::to_string(index) + "]") : rule.name;
}

AdapterConfigResult errorResult(std::string error) {
  AdapterConfigResult result;
  result.ok = false;
  result.error = std::move(error);
  return result;
}

}  // namespace

size_t AdapterConfig::enabledRuleCount() const {
  return static_cast<size_t>(std::count_if(rules.begin(), rules.end(), [](const AdapterRule& rule) {
    return rule.enabled;
  }));
}

AdapterConfig defaultAdapterConfig() {
  AdapterConfig config;
  config.deny_by_default = true;
  return config;
}

bool isDefaultDeniedTopic(const std::string& topic) {
  return isReservedSystemTopic(topic) || isLargeDebugTopic(topic);
}

AdapterConfigResult parseAdapterConfigXmlRpc(XmlRpc::XmlRpcValue value) {
  if (hasMember(value, "ros1_adapters")) {
    value = value["ros1_adapters"];
  }
  if (value.getType() != XmlRpc::XmlRpcValue::TypeStruct) {
    return errorResult("ros1_adapters must be a struct, got " + typeName(value.getType()));
  }

  AdapterConfig config;
  std::string error;
  if (!readBool(value, "deny_by_default", &config.deny_by_default, &error)) {
    return errorResult(error);
  }

  if (!hasMember(value, "rules")) {
    return validateAdapterConfig(std::move(config));
  }
  XmlRpc::XmlRpcValue& rules = value["rules"];
  if (rules.getType() != XmlRpc::XmlRpcValue::TypeArray) {
    return errorResult("rules must be an array, got " + typeName(rules.getType()));
  }

  for (int i = 0; i < rules.size(); ++i) {
    XmlRpc::XmlRpcValue& raw_rule = rules[i];
    if (raw_rule.getType() != XmlRpc::XmlRpcValue::TypeStruct) {
      return errorResult("rules[" + std::to_string(i) + "] must be a struct, got " +
                         typeName(raw_rule.getType()));
    }

    AdapterRule rule;
    if (!readString(raw_rule, "name", true, &rule.name, &error) ||
        !readString(raw_rule, "topic", true, &rule.topic, &error) ||
        !readString(raw_rule, "channel", true, &rule.channel, &error) ||
        !readString(raw_rule, "schema_id", true, &rule.schema_id, &error) ||
        !readString(raw_rule, "payload_type", true, &rule.payload_type, &error) ||
        !readBool(raw_rule, "enabled", &rule.enabled, &error) ||
        !readBool(raw_rule, "required", &rule.required, &error) ||
        !readBool(raw_rule, "allow_large_payload", &rule.allow_large_payload, &error) ||
        !readSize(raw_rule, "max_payload_bytes", &rule.max_payload_bytes, &error)) {
      return errorResult("rules[" + std::to_string(i) + "]: " + error);
    }
    config.rules.push_back(std::move(rule));
  }

  return validateAdapterConfig(std::move(config));
}

AdapterConfigResult validateAdapterConfig(AdapterConfig config) {
  std::set<std::string> rule_names;
  std::set<std::tuple<std::string, std::string>> enabled_channel_schemas;

  for (size_t i = 0; i < config.rules.size(); ++i) {
    const AdapterRule& rule = config.rules[i];
    const std::string label = ruleLabel(rule, i);

    std::string error;
    if (!validateTaskField("name", rule.name, &error) ||
        !validateTaskField("channel", rule.channel, &error) ||
        !validateTaskField("schema_id", rule.schema_id, &error)) {
      return errorResult(label + ": " + error);
    }
    if (rule.topic.empty() || rule.topic.front() != '/') {
      return errorResult(label + ": topic must start with /");
    }
    if (rule.payload_type.empty()) {
      return errorResult(label + ": payload_type is required");
    }
    if (rule.max_payload_bytes == 0) {
      return errorResult(label + ": max_payload_bytes must be positive");
    }
    if (rule.max_payload_bytes > swarm_sync::kDefaultMaxPayloadBytes) {
      return errorResult(label + ": max_payload_bytes exceeds envelope maximum");
    }
    if (!rule.allow_large_payload &&
        rule.max_payload_bytes > kDefaultAdapterMaxPayloadBytes) {
      return errorResult(label + ": allow_large_payload is required above adapter default size");
    }
    if (!rule.allow_large_payload && isDefaultDeniedTopic(rule.topic)) {
      return errorResult(label + ": topic is default-denied; set allow_large_payload=true for explicit override");
    }
    if (!rule_names.insert(rule.name).second) {
      return errorResult(label + ": duplicate rule name");
    }
    if (rule.enabled &&
        !enabled_channel_schemas.insert(std::make_tuple(rule.channel, rule.schema_id)).second) {
      return errorResult(label + ": duplicate enabled channel/schema mapping");
    }
  }

  AdapterConfigResult result;
  result.ok = true;
  result.config = std::move(config);
  return result;
}

std::string summarizeAdapterConfig(const AdapterConfig& config) {
  std::ostringstream out;
  out << "adapters=" << config.enabledRuleCount() << "/" << config.rules.size()
      << " deny_by_default=" << (config.deny_by_default ? "true" : "false");
  return out.str();
}

}  // namespace swarm_sync_ros1
