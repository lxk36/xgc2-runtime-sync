#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace swarm_sync::weaknet {

struct ChannelBudgetInput {
  std::string channel;
  uint32_t publishers{0};
  uint32_t effective_fanout{0};
  double frequency_hz{0.0};
  uint32_t payload_bytes_p95{0};
  uint32_t envelope_overhead_bytes{128};
  uint32_t transport_overhead_bytes{64};
};

struct BudgetReport {
  bool accepted{true};
  double total_realtime_bps{0.0};
  double max_realtime_bps{0.0};
  double utilization_ratio{0.0};
  std::string reason;
};

BudgetReport computeTrafficBudget(const std::vector<ChannelBudgetInput>& inputs,
                                  double max_realtime_bps,
                                  double safety_factor);

}  // namespace swarm_sync::weaknet
