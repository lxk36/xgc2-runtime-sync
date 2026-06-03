#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include <XmlRpcValue.h>

#include "swarm_sync_core/sample_envelope.hpp"

namespace swarm_sync_ros1 {

constexpr size_t kDefaultAdapterMaxPayloadBytes = 64 * 1024;

struct AdapterRule {
  std::string name;
  std::string topic;
  std::string channel;
  std::string schema_id;
  std::string payload_type{"bytes"};
  bool enabled{true};
  size_t max_payload_bytes{kDefaultAdapterMaxPayloadBytes};
  bool required{false};
  bool allow_large_payload{false};
};

struct AdapterConfig {
  bool deny_by_default{true};
  std::vector<AdapterRule> rules;

  size_t enabledRuleCount() const;
};

struct AdapterConfigResult {
  bool ok{false};
  std::string error;
  AdapterConfig config;
};

AdapterConfig defaultAdapterConfig();

bool isDefaultDeniedTopic(const std::string& topic);

AdapterConfigResult parseAdapterConfigXmlRpc(XmlRpc::XmlRpcValue value);
AdapterConfigResult validateAdapterConfig(AdapterConfig config);

std::string summarizeAdapterConfig(const AdapterConfig& config);

}  // namespace swarm_sync_ros1
