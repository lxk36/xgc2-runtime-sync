#pragma once

#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include "swarm_sync_core/sample_envelope.hpp"
#include "swarm_sync_core/types.hpp"

namespace swarm_sync {

struct DecodeResult {
  bool ok{false};
  SampleStatus status{SampleStatus::TransportError};
  std::string error;
  SampleEnvelope envelope;
};

class EnvelopeCodec {
public:
  explicit EnvelopeCodec(uint32_t supported_envelope_version = 1);

  void allowSchema(std::string channel, std::string payload_type, std::string schema_id);
  void clearSchemaRules();

  std::vector<uint8_t> encode(const SampleEnvelope& envelope) const;
  DecodeResult decode(const std::vector<uint8_t>& bytes) const;
  DecodeResult validate(const SampleEnvelope& envelope) const;

  bool isSchemaAccepted(const SampleEnvelope& envelope) const;

  static uint32_t crc32c(const uint8_t* data, size_t size);
  static bool verifyPayloadCrc(const SampleEnvelope& envelope);

private:
  using SchemaKey = std::tuple<std::string, std::string, std::string>;

  uint32_t supported_envelope_version_{1};
  std::set<SchemaKey> allowed_schemas_;
};

}  // namespace swarm_sync
