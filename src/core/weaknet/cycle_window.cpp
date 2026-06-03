#include "swarm_sync_core/weaknet/cycle_window.hpp"

namespace swarm_sync::weaknet {

CycleWindow::CycleWindow(CycleWindowConfig cfg) : cfg_(cfg) {}

bool CycleWindow::valid(std::string* error) const {
  auto fail = [error](const std::string& message) {
    if (error) {
      *error = message;
    }
    return false;
  };
  if (cfg_.period_ns <= 0) {
    return fail("period_ns must be positive");
  }
  if (cfg_.receive_cutoff_offset_ns < 0 ||
      cfg_.late_record_window_ns < 0 ||
      cfg_.ttl_ns < 0) {
    return fail("window durations must be non-negative");
  }
  if (cfg_.receive_cutoff_offset_ns > cfg_.period_ns) {
    return fail("receive cutoff must be inside the current cycle");
  }
  if (error) {
    error->clear();
  }
  return true;
}

WindowDecision CycleWindow::classify(uint64_t current_cycle,
                                     int64_t cycle_start_ns,
                                     const SampleTiming& sample) const {
  WindowDecision decision;
  if (!sample.sender_clock_ok) {
    decision.status = SampleStatus::SenderClockBad;
    decision.reason = "sender clock is bad";
    return decision;
  }
  if (!sample.producer_ok) {
    decision.status = SampleStatus::SenderReportedFail;
    decision.reason = "producer reported failure";
    return decision;
  }
  if (!sample.schema_ok) {
    decision.status = SampleStatus::BadSchema;
    decision.reason = "schema check failed";
    return decision;
  }
  if (!sample.payload_ok) {
    decision.status = SampleStatus::BadPayload;
    decision.reason = "payload check failed";
    return decision;
  }
  if (sample.duplicate) {
    decision.status = SampleStatus::Duplicate;
    decision.reason = "duplicate sample";
    return decision;
  }
  if (sample.target_cycle != current_cycle) {
    decision.status = SampleStatus::WrongCycle;
    decision.reason = "target cycle mismatch";
    return decision;
  }

  const int64_t cutoff_ns = cycle_start_ns + cfg_.receive_cutoff_offset_ns;
  if (cfg_.receive_cutoff_offset_ns > 0 && sample.local_receive_ns > cutoff_ns) {
    const int64_t late_until_ns = cutoff_ns + cfg_.late_record_window_ns;
    if (cfg_.late_record_window_ns == 0 || sample.local_receive_ns <= late_until_ns) {
      decision.status = SampleStatus::Late;
      decision.late_disposition = LateDisposition::LatePocket;
      decision.record_in_late_pocket = true;
      decision.reason = "arrived after receive cutoff";
      return decision;
    }
    decision.status = SampleStatus::Late;
    decision.late_disposition = LateDisposition::Expired;
    decision.reason = "arrived after late record window";
    return decision;
  }

  if (cfg_.ttl_ns > 0 && sample.local_receive_ns > cycle_start_ns + cfg_.ttl_ns) {
    decision.status = SampleStatus::Late;
    decision.late_disposition = LateDisposition::Expired;
    decision.reason = "sample ttl expired";
    return decision;
  }

  decision.status = SampleStatus::Fresh;
  decision.usable_for_cycle = true;
  decision.reason = "fresh";
  return decision;
}

}  // namespace swarm_sync::weaknet
