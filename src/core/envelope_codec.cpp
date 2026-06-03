#include "swarm_sync_core/envelope_codec.hpp"

#include <cstring>
#include <limits>
#include <stdexcept>

#include "swarm_sync_core/transport_keys.hpp"

namespace swarm_sync {
namespace {

constexpr uint32_t kMagic = 0x4e455353;  // "SSEN", little-endian.
constexpr uint32_t kFormatVersion = 1;

void writeU8(std::vector<uint8_t>* out, uint8_t value) {
  out->push_back(value);
}

void writeU32(std::vector<uint8_t>* out, uint32_t value) {
  for (int i = 0; i < 4; ++i) {
    out->push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xffu));
  }
}

void writeU64(std::vector<uint8_t>* out, uint64_t value) {
  for (int i = 0; i < 8; ++i) {
    out->push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xffu));
  }
}

void writeI64(std::vector<uint8_t>* out, int64_t value) {
  writeU64(out, static_cast<uint64_t>(value));
}

void writeString(std::vector<uint8_t>* out, const std::string& value) {
  writeU32(out, static_cast<uint32_t>(value.size()));
  out->insert(out->end(), value.begin(), value.end());
}

void writeBytes(std::vector<uint8_t>* out, const std::vector<uint8_t>& value) {
  writeU32(out, static_cast<uint32_t>(value.size()));
  out->insert(out->end(), value.begin(), value.end());
}

class Reader {
public:
  explicit Reader(const std::vector<uint8_t>& bytes) : bytes_(bytes) {}

  bool readU8(uint8_t* value) {
    if (!has(1)) return false;
    *value = bytes_[offset_++];
    return true;
  }

  bool readU32(uint32_t* value) {
    if (!has(4)) return false;
    uint32_t out = 0;
    for (int i = 0; i < 4; ++i) {
      out |= static_cast<uint32_t>(bytes_[offset_++]) << (i * 8);
    }
    *value = out;
    return true;
  }

  bool readU64(uint64_t* value) {
    if (!has(8)) return false;
    uint64_t out = 0;
    for (int i = 0; i < 8; ++i) {
      out |= static_cast<uint64_t>(bytes_[offset_++]) << (i * 8);
    }
    *value = out;
    return true;
  }

  bool readI64(int64_t* value) {
    uint64_t raw = 0;
    if (!readU64(&raw)) return false;
    *value = static_cast<int64_t>(raw);
    return true;
  }

  bool readString(const char* field_name, std::string* value, std::string* error) {
    uint32_t size = 0;
    if (!readU32(&size)) {
      if (error) *error = std::string("truncated field length for ") + field_name;
      return false;
    }
    if (size > kMaxEnvelopeFieldBytes) {
      if (error) *error = std::string("field length exceeds limit for ") + field_name;
      return false;
    }
    if (!has(size)) {
      if (error) *error = std::string("bad field length for ") + field_name;
      return false;
    }
    value->assign(reinterpret_cast<const char*>(bytes_.data() + offset_), size);
    offset_ += size;
    return true;
  }

  bool readBytes(const char* field_name,
                 size_t max_size,
                 std::vector<uint8_t>* value,
                 std::string* error) {
    uint32_t size = 0;
    if (!readU32(&size)) {
      if (error) *error = std::string("truncated field length for ") + field_name;
      return false;
    }
    if (size > max_size) {
      if (error) *error = std::string(field_name) + " exceeds maximum size";
      return false;
    }
    if (!has(size)) {
      if (error) *error = std::string("bad field length for ") + field_name;
      return false;
    }
    value->assign(bytes_.begin() + static_cast<std::ptrdiff_t>(offset_),
                  bytes_.begin() + static_cast<std::ptrdiff_t>(offset_ + size));
    offset_ += size;
    return true;
  }

  bool done() const { return offset_ == bytes_.size(); }

private:
  bool has(size_t size) const {
    return size <= bytes_.size() && offset_ <= bytes_.size() - size;
  }

  const std::vector<uint8_t>& bytes_;
  size_t offset_{0};
};

DecodeResult errorResult(SampleStatus status, std::string error, SampleEnvelope envelope = {}) {
  DecodeResult result;
  result.ok = false;
  result.status = status;
  result.error = std::move(error);
  result.envelope = std::move(envelope);
  return result;
}

bool isKnownProducerStatus(ProducerStatus status) {
  return status == ProducerStatus::Success ||
         status == ProducerStatus::Timeout ||
         status == ProducerStatus::Failed ||
         status == ProducerStatus::Skipped ||
         status == ProducerStatus::UserFallback ||
         status == ProducerStatus::Unknown;
}

bool validateRequiredKeyField(const std::string& name,
                              const std::string& value,
                              std::string* reason) {
  if (value.empty()) {
    if (reason) *reason = name + " is required";
    return false;
  }
  if (!isValidTaskKeyField(value)) {
    if (reason) *reason = name + " contains an illegal character";
    return false;
  }
  return true;
}

}  // namespace

EnvelopeCodec::EnvelopeCodec(size_t max_payload_bytes)
    : max_payload_bytes_(max_payload_bytes) {}

void EnvelopeCodec::allowSchema(std::string channel,
                                std::string payload_type,
                                std::string schema_id) {
  allowed_schemas_.insert(SchemaKey{std::move(channel),
                                    std::move(payload_type),
                                    std::move(schema_id)});
}

void EnvelopeCodec::clearSchemaRules() {
  allowed_schemas_.clear();
}

std::vector<uint8_t> EnvelopeCodec::encode(const SampleEnvelope& envelope) const {
  SampleEnvelope encoded = envelope;
  if (encoded.sender_id.empty()) {
    encoded.sender_id = encoded.participant_id;
  }
  const SampleEnvelopeValidationOptions options{max_payload_bytes_, false};
  const auto validation = validateSampleEnvelope(encoded, options);
  if (!validation.ok) {
    throw std::invalid_argument(validation.error);
  }
  if (!isSchemaAccepted(encoded)) {
    throw std::invalid_argument("schema_id is not accepted for channel");
  }

  encoded.payload_crc32c = crc32c(encoded.payload.data(), encoded.payload.size());

  std::vector<uint8_t> out;
  out.reserve(192 + encoded.payload.size());
  writeU32(&out, kMagic);
  writeU32(&out, kFormatVersion);
  writeU32(&out, encoded.schema_version);
  writeString(&out, encoded.session_id);
  writeString(&out, encoded.team_id);
  writeString(&out, encoded.task_id);
  writeString(&out, encoded.participant_id);
  writeString(&out, encoded.channel);
  writeString(&out, encoded.sender_id);
  writeString(&out, encoded.node_id);
  writeU64(&out, encoded.seq);
  writeU64(&out, encoded.produce_cycle);
  writeU64(&out, encoded.target_cycle);
  writeI64(&out, encoded.t_cycle_start_ns);
  writeI64(&out, encoded.t_produce_start_ns);
  writeI64(&out, encoded.t_produce_finish_ns);
  writeI64(&out, encoded.t_publish_ns);
  writeI64(&out, encoded.period_ns);
  writeI64(&out, encoded.publish_deadline_ns);
  writeI64(&out, encoded.ttl_ns);
  writeU8(&out, static_cast<uint8_t>(encoded.producer_status));
  writeU8(&out, encoded.clock_ok ? 1 : 0);
  writeI64(&out, encoded.clock_offset_ns);
  writeU64(&out, encoded.clock_uncertainty_ns);
  writeString(&out, encoded.payload_type);
  writeString(&out, encoded.schema_id);
  writeU32(&out, encoded.payload_crc32c);
  writeBytes(&out, encoded.payload);
  return out;
}

DecodeResult EnvelopeCodec::decode(const std::vector<uint8_t>& bytes) const {
  Reader reader(bytes);
  uint32_t magic = 0;
  uint32_t format_version = 0;
  SampleEnvelope envelope;
  uint8_t producer_status = 0;
  uint8_t clock_ok = 0;
  std::string malformed_reason;

  if (!reader.readU32(&magic) || magic != kMagic) {
    return errorResult(SampleStatus::TransportError, "bad envelope magic");
  }
  if (!reader.readU32(&format_version) || format_version != kFormatVersion) {
    return errorResult(SampleStatus::TransportError, "unsupported codec format");
  }
  if (!reader.readU32(&envelope.schema_version) ||
      !reader.readString("session_id", &envelope.session_id, &malformed_reason) ||
      !reader.readString("team_id", &envelope.team_id, &malformed_reason) ||
      !reader.readString("task_id", &envelope.task_id, &malformed_reason) ||
      !reader.readString("participant_id", &envelope.participant_id, &malformed_reason) ||
      !reader.readString("channel", &envelope.channel, &malformed_reason) ||
      !reader.readString("sender_id", &envelope.sender_id, &malformed_reason) ||
      !reader.readString("node_id", &envelope.node_id, &malformed_reason) ||
      !reader.readU64(&envelope.seq) ||
      !reader.readU64(&envelope.produce_cycle) ||
      !reader.readU64(&envelope.target_cycle) ||
      !reader.readI64(&envelope.t_cycle_start_ns) ||
      !reader.readI64(&envelope.t_produce_start_ns) ||
      !reader.readI64(&envelope.t_produce_finish_ns) ||
      !reader.readI64(&envelope.t_publish_ns) ||
      !reader.readI64(&envelope.period_ns) ||
      !reader.readI64(&envelope.publish_deadline_ns) ||
      !reader.readI64(&envelope.ttl_ns) ||
      !reader.readU8(&producer_status) ||
      !reader.readU8(&clock_ok) ||
      !reader.readI64(&envelope.clock_offset_ns) ||
      !reader.readU64(&envelope.clock_uncertainty_ns) ||
      !reader.readString("payload_type", &envelope.payload_type, &malformed_reason) ||
      !reader.readString("schema_id", &envelope.schema_id, &malformed_reason) ||
      !reader.readU32(&envelope.payload_crc32c) ||
      !reader.readBytes("payload", max_payload_bytes_, &envelope.payload, &malformed_reason)) {
    return errorResult(SampleStatus::TransportError,
                       malformed_reason.empty() ? "truncated or malformed envelope"
                                                : malformed_reason,
                       envelope);
  }
  if (!reader.done()) {
    return errorResult(SampleStatus::TransportError, "trailing data after envelope", envelope);
  }

  envelope.producer_status = static_cast<ProducerStatus>(producer_status);
  envelope.clock_ok = clock_ok != 0;
  if (envelope.sender_id.empty()) {
    envelope.sender_id = envelope.participant_id;
  }
  return validate(envelope);
}

DecodeResult EnvelopeCodec::validate(const SampleEnvelope& envelope) const {
  const SampleEnvelopeValidationOptions options{max_payload_bytes_, true};
  const auto validation = validateSampleEnvelope(envelope, options);
  if (!validation.ok) return validation;

  if (!isSchemaAccepted(envelope)) {
    return errorResult(SampleStatus::BadSchema, "schema_id is not accepted for channel", envelope);
  }
  return validation;
}

bool EnvelopeCodec::isSchemaAccepted(const SampleEnvelope& envelope) const {
  if (allowed_schemas_.empty()) {
    return true;
  }
  return allowed_schemas_.count(SchemaKey{envelope.channel,
                                          envelope.payload_type,
                                          envelope.schema_id}) > 0;
}

uint32_t EnvelopeCodec::crc32c(const uint8_t* data, size_t size) {
  uint32_t crc = 0xffffffffu;
  for (size_t i = 0; i < size; ++i) {
    crc ^= data ? data[i] : 0u;
    for (int bit = 0; bit < 8; ++bit) {
      const uint32_t mask = static_cast<uint32_t>(-static_cast<int32_t>(crc & 1u));
      crc = (crc >> 1) ^ (0x82f63b78u & mask);
    }
  }
  return ~crc;
}

bool EnvelopeCodec::verifyPayloadCrc(const SampleEnvelope& envelope) {
  return crc32c(envelope.payload.data(), envelope.payload.size()) == envelope.payload_crc32c;
}

DecodeResult validateSampleEnvelope(const SampleEnvelope& envelope,
                                    const SampleEnvelopeValidationOptions& options) {
  if (envelope.schema_version != kCurrentEnvelopeSchemaVersion) {
    return errorResult(SampleStatus::TransportError,
                       "unsupported schema_version: only 1 is supported",
                       envelope);
  }
  if (envelope.payload.size() > options.max_payload_bytes) {
    return errorResult(SampleStatus::BadPayload, "payload exceeds maximum size", envelope);
  }

  std::string reason;
  if (!validateRequiredKeyField("session_id", envelope.session_id, &reason) ||
      !validateRequiredKeyField("task_id", envelope.task_id, &reason) ||
      !validateRequiredKeyField("participant_id", envelope.participant_id, &reason) ||
      !validateRequiredKeyField("channel", envelope.channel, &reason) ||
      !validateRequiredKeyField("schema_id", envelope.schema_id, &reason)) {
    return errorResult(SampleStatus::TransportError, reason, envelope);
  }

  if (!isKnownProducerStatus(envelope.producer_status)) {
    return errorResult(SampleStatus::TransportError, "producer_status is invalid", envelope);
  }
  if (options.verify_payload_crc && !EnvelopeCodec::verifyPayloadCrc(envelope)) {
    return errorResult(SampleStatus::BadPayload, "payload crc mismatch", envelope);
  }

  DecodeResult result;
  result.ok = true;
  result.status = SampleStatus::Fresh;
  result.envelope = envelope;
  return result;
}

}  // namespace swarm_sync
