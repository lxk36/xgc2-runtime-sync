#include "swarm_sync_core/sample_buffer.hpp"

#include <algorithm>

#include "swarm_sync_core/envelope_codec.hpp"

namespace swarm_sync {

SampleBuffer::SampleBuffer(size_t max_samples_per_key)
    : max_samples_per_key_(std::max<size_t>(1, max_samples_per_key)) {}

BufferAddResult SampleBuffer::add(const SampleEnvelope& envelope, int64_t local_receive_ns) {
  const Key key{envelope.sender_id, envelope.channel, envelope.target_cycle};
  const StreamKey stream{envelope.sender_id, envelope.channel};

  BufferAddResult result;
  auto& stream_state = stream_state_[stream];
  if (stream_state.has_seq) {
    if (envelope.seq == stream_state.last_seq) {
      result.status = SampleStatus::Duplicate;
      result.duplicate = true;
      result.reason = "duplicate seq";
      return result;
    }
    if (envelope.seq > stream_state.last_seq + 1) {
      result.seq_gap = envelope.seq - stream_state.last_seq - 1;
      stream_state.gap_count += result.seq_gap;
      result.reason = "seq gap";
    }
  }

  const auto existing = samples_.find(key);
  if (existing != samples_.end() && existing->second.envelope.seq == envelope.seq) {
    result.status = SampleStatus::Duplicate;
    result.duplicate = true;
    result.reason = "duplicate sample key";
    return result;
  }

  samples_[key] = BufferedSample{envelope, local_receive_ns};
  result.inserted = true;
  result.status = result.seq_gap > 0 ? SampleStatus::TransportError : SampleStatus::Fresh;

  if (!stream_state.has_seq || envelope.seq > stream_state.last_seq) {
    stream_state.has_seq = true;
    stream_state.last_seq = envelope.seq;
  }
  if (isGood(envelope)) {
    last_good_[stream] = BufferedSample{envelope, local_receive_ns};
  }
  pruneStream(stream);
  return result;
}

std::optional<BufferedSample> SampleBuffer::find(
    const std::string& peer,
    const std::string& channel,
    uint64_t target_cycle) const {
  const Key key{peer, channel, target_cycle};
  const auto it = samples_.find(key);
  if (it == samples_.end()) {
    return std::nullopt;
  }
  return it->second;
}

std::optional<BufferedSample> SampleBuffer::getLastGood(
    const std::string& peer,
    const std::string& channel) const {
  const StreamKey stream{peer, channel};
  const auto it = last_good_.find(stream);
  if (it == last_good_.end()) {
    return std::nullopt;
  }
  return it->second;
}

size_t SampleBuffer::size() const {
  return samples_.size();
}

uint64_t SampleBuffer::seqGapCount(const std::string& peer, const std::string& channel) const {
  const StreamKey stream{peer, channel};
  const auto it = stream_state_.find(stream);
  if (it == stream_state_.end()) {
    return 0;
  }
  return it->second.gap_count;
}

bool SampleBuffer::isGood(const SampleEnvelope& envelope) const {
  return envelope.clock_ok &&
         envelope.producer_status == ProducerStatus::Success &&
         EnvelopeCodec::verifyPayloadCrc(envelope);
}

void SampleBuffer::pruneStream(const StreamKey& stream) {
  size_t count = 0;
  for (const auto& item : samples_) {
    if (std::get<0>(item.first) == std::get<0>(stream) &&
        std::get<1>(item.first) == std::get<1>(stream)) {
      ++count;
    }
  }

  while (count > max_samples_per_key_) {
    auto oldest = samples_.end();
    for (auto it = samples_.begin(); it != samples_.end(); ++it) {
      if (std::get<0>(it->first) != std::get<0>(stream) ||
          std::get<1>(it->first) != std::get<1>(stream)) {
        continue;
      }
      if (oldest == samples_.end() ||
          it->second.envelope.target_cycle < oldest->second.envelope.target_cycle) {
        oldest = it;
      }
    }
    if (oldest == samples_.end()) {
      return;
    }
    samples_.erase(oldest);
    --count;
  }
}

}  // namespace swarm_sync
