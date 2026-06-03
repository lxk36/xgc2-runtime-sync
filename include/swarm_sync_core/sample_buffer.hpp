#pragma once

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <tuple>

#include "swarm_sync_core/sample_envelope.hpp"
#include "swarm_sync_core/types.hpp"

namespace swarm_sync {

struct BufferedSample {
  SampleEnvelope envelope;
  int64_t local_receive_ns{0};
};

struct BufferAddResult {
  SampleStatus status{SampleStatus::Fresh};
  bool inserted{false};
  bool duplicate{false};
  uint64_t seq_gap{0};
  std::string reason;
};

class SampleBuffer {
public:
  explicit SampleBuffer(size_t max_samples_per_key = 8);

  BufferAddResult add(const SampleEnvelope& envelope, int64_t local_receive_ns);

  std::optional<BufferedSample> find(
      const std::string& peer,
      const std::string& channel,
      uint64_t target_cycle) const;

  std::optional<BufferedSample> getLastGood(
      const std::string& peer,
      const std::string& channel) const;

  size_t size() const;
  uint64_t seqGapCount(const std::string& peer, const std::string& channel) const;

private:
  using Key = std::tuple<std::string, std::string, uint64_t>;
  using StreamKey = std::tuple<std::string, std::string>;

  struct StreamState {
    bool has_seq{false};
    uint64_t last_seq{0};
    uint64_t gap_count{0};
  };

  bool isGood(const SampleEnvelope& envelope) const;
  void pruneStream(const StreamKey& stream);

  size_t max_samples_per_key_{8};
  std::map<Key, BufferedSample> samples_;
  std::map<StreamKey, BufferedSample> last_good_;
  std::map<StreamKey, StreamState> stream_state_;
};

}  // namespace swarm_sync
