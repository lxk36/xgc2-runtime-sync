#include "swarm_sync_core/snapshot_builder.hpp"

namespace swarm_sync {
namespace {

bool isBadStatus(SampleStatus status) {
  return status == SampleStatus::BadSchema ||
         status == SampleStatus::BadPayload ||
         status == SampleStatus::SenderClockBad ||
         status == SampleStatus::SenderReportedFail ||
         status == SampleStatus::TransportError;
}

void accountStatus(SnapshotStats* stats, SampleStatus status) {
  switch (status) {
    case SampleStatus::Fresh:
      ++stats->fresh_count;
      break;
    case SampleStatus::Missing:
      ++stats->missing_count;
      break;
    case SampleStatus::Late:
      ++stats->late_count;
      break;
    case SampleStatus::WrongCycle:
      ++stats->wrong_cycle_count;
      break;
    default:
      if (isBadStatus(status) || status == SampleStatus::Duplicate) {
        ++stats->bad_count;
      }
      break;
  }
}

}  // namespace

CycleSnapshot SnapshotBuilder::build(const SnapshotBuildRequest& request,
                                     const SampleBuffer& buffer) const {
  CycleSnapshot snapshot;
  snapshot.session_id = request.session_id;
  snapshot.task_id = request.task_id;
  snapshot.self_id = request.self_id;
  snapshot.cycle_id = request.cycle_id;
  snapshot.t_cycle_start_ns = request.cycle_start_ns;
  snapshot.period_ns = request.period_ns;
  snapshot.local_clock = request.local_clock;
  snapshot.local_runtime = request.local_runtime;
  snapshot.local_runtime.current_cycle = request.cycle_id;
  snapshot.all_required_fresh = true;
  snapshot.all_required_usable = true;

  for (const auto& expected : request.expected_samples) {
    PeerSampleView view;
    view.peer_id = expected.peer_id;
    view.channel = expected.channel;
    view.expected_target_cycle = request.cycle_id;

    const auto buffered = buffer.find(expected.peer_id, expected.channel, request.cycle_id);
    if (!buffered) {
      view.status = SampleStatus::Missing;
      view.drop_reason = "missing";
    } else {
      const DeadlineDecision decision = deadline_checker_.checkReceivedSample(
          buffered->envelope,
          request.cycle_id,
          request.cycle_start_ns,
          buffered->local_receive_ns,
          expected.timing);
      view.status = decision.status;
      view.sample = buffered->envelope;
      view.received_target_cycle = buffered->envelope.target_cycle;
      view.t_local_receive_ns = buffered->local_receive_ns;
      view.receive_latency_ns = buffered->local_receive_ns - buffered->envelope.t_publish_ns;
      view.fresh_for_this_cycle = decision.status == SampleStatus::Fresh;
      view.late_for_this_cycle = decision.status == SampleStatus::Late;
      view.wrong_cycle = decision.status == SampleStatus::WrongCycle;
      view.bad_schema = decision.status == SampleStatus::BadSchema;
      view.bad_crc = decision.status == SampleStatus::BadPayload;
      view.sender_clock_bad = decision.status == SampleStatus::SenderClockBad;
      view.drop_reason = decision.reason;
    }

    if (expected.required) {
      snapshot.all_required_fresh =
          snapshot.all_required_fresh && view.status == SampleStatus::Fresh;
      snapshot.all_required_usable =
          snapshot.all_required_usable && view.status == SampleStatus::Fresh;
    }

    accountStatus(&snapshot.stats, view.status);
    snapshot.samples.push_back(std::move(view));
  }

  return snapshot;
}

}  // namespace swarm_sync
