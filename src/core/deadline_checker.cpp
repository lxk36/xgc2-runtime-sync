#include "swarm_sync_core/deadline_checker.hpp"

#include "swarm_sync_core/envelope_codec.hpp"

namespace swarm_sync {

DeadlineDecision DeadlineChecker::checkReceivedSample(
    const SampleEnvelope& envelope,
    uint64_t current_cycle,
    int64_t cycle_start_ns,
    int64_t local_receive_ns,
    const ChannelTiming& timing) const {
  DeadlineDecision decision;

  if (!envelope.clock_ok) {
    decision.status = SampleStatus::SenderClockBad;
    decision.reason = "sender clock is bad";
    return decision;
  }
  if (envelope.producer_status != ProducerStatus::Success) {
    decision.status = SampleStatus::SenderReportedFail;
    decision.reason = "sender reported non-success producer status";
    return decision;
  }
  if (!timing.payload_type.empty() && envelope.payload_type != timing.payload_type) {
    decision.status = SampleStatus::BadSchema;
    decision.reason = "payload_type mismatch";
    return decision;
  }
  if (!timing.schema_id.empty() && envelope.schema_id != timing.schema_id) {
    decision.status = SampleStatus::BadSchema;
    decision.reason = "schema_id mismatch";
    return decision;
  }
  if (!timing.allow_empty_payload && envelope.payload.empty()) {
    decision.status = SampleStatus::BadPayload;
    decision.reason = "payload is empty";
    return decision;
  }
  if (!EnvelopeCodec::verifyPayloadCrc(envelope)) {
    decision.status = SampleStatus::BadPayload;
    decision.reason = "payload crc mismatch";
    return decision;
  }
  if (envelope.target_cycle != current_cycle) {
    decision.status = SampleStatus::WrongCycle;
    decision.reason = "target_cycle does not match current cycle";
    return decision;
  }
  if (timing.publish_deadline_ns > 0 &&
      envelope.t_publish_ns > cycle_start_ns + timing.publish_deadline_ns) {
    decision.status = SampleStatus::Late;
    decision.reason = "sample was published after publish deadline";
    return decision;
  }
  if (timing.receive_cutoff_ns > 0 &&
      local_receive_ns > cycle_start_ns + timing.receive_cutoff_ns) {
    decision.status = SampleStatus::Late;
    decision.reason = "sample arrived after receive cutoff";
    return decision;
  }
  const int64_t ttl_ns = timing.ttl_ns > 0 ? timing.ttl_ns : envelope.ttl_ns;
  if (ttl_ns > 0 && local_receive_ns > envelope.t_publish_ns + ttl_ns) {
    decision.status = SampleStatus::Late;
    decision.reason = "sample ttl expired";
    return decision;
  }

  decision.status = SampleStatus::Fresh;
  decision.reason = "fresh";
  decision.usable = true;
  return decision;
}

}  // namespace swarm_sync
