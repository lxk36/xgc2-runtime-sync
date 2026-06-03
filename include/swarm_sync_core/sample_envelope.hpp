#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "swarm_sync_core/types.hpp"

namespace swarm_sync {

struct SampleEnvelope {
  uint32_t envelope_version{1};

  std::string session_id;
  std::string team_id;
  std::string task_id;
  std::string channel;

  std::string sender_id;
  std::string node_id;

  uint64_t seq{0};

  uint64_t produce_cycle{0};
  uint64_t target_cycle{0};

  int64_t t_cycle_start_ns{0};
  int64_t t_produce_start_ns{0};
  int64_t t_produce_finish_ns{0};
  int64_t t_publish_ns{0};

  int64_t period_ns{0};
  int64_t publish_deadline_ns{0};
  int64_t ttl_ns{0};

  ProducerStatus producer_status{ProducerStatus::Unknown};

  bool clock_ok{false};
  int64_t clock_offset_ns{0};
  uint64_t clock_uncertainty_ns{0};

  std::string payload_type;
  std::string schema_id;
  uint32_t payload_crc32c{0};

  std::vector<uint8_t> payload;
};

}  // namespace swarm_sync
