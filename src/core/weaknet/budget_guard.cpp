#include "swarm_sync_core/weaknet/budget_guard.hpp"

#include <cmath>

namespace swarm_sync::weaknet {

BudgetReport computeTrafficBudget(const std::vector<ChannelBudgetInput>& inputs,
                                  double max_realtime_bps,
                                  double safety_factor) {
  BudgetReport report;
  report.max_realtime_bps = max_realtime_bps;
  if (max_realtime_bps <= 0.0 || !std::isfinite(max_realtime_bps)) {
    report.accepted = false;
    report.reason = "max_realtime_bps must be positive";
    return report;
  }
  const double factor = safety_factor > 0.0 && std::isfinite(safety_factor) ? safety_factor : 1.0;
  for (const auto& input : inputs) {
    const double bytes_per_sample =
        static_cast<double>(input.payload_bytes_p95 + input.envelope_overhead_bytes +
                            input.transport_overhead_bytes);
    report.total_realtime_bps += static_cast<double>(input.publishers) *
                                 static_cast<double>(input.effective_fanout) *
                                 input.frequency_hz * bytes_per_sample * 8.0 * factor;
  }
  report.utilization_ratio = report.total_realtime_bps / max_realtime_bps;
  report.accepted = report.total_realtime_bps <= max_realtime_bps;
  report.reason = report.accepted ? "within budget" : "traffic budget exceeded";
  return report;
}

}  // namespace swarm_sync::weaknet
