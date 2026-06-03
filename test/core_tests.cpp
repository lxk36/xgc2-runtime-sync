#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "swarm_sync_core/cycle_scheduler.hpp"
#include "swarm_sync_core/deadline_checker.hpp"
#include "swarm_sync_core/envelope_codec.hpp"
#include "swarm_sync_core/sample_buffer.hpp"
#include "swarm_sync_core/session.hpp"
#include "swarm_sync_core/snapshot_builder.hpp"

namespace {

using namespace swarm_sync;

SampleEnvelope makeEnvelope(uint64_t cycle, uint64_t seq = 1) {
  SampleEnvelope envelope;
  envelope.session_id = "session-a";
  envelope.team_id = "team-a";
  envelope.task_id = "formation";
  envelope.channel = "solution";
  envelope.sender_id = "uav2";
  envelope.node_id = "planner";
  envelope.seq = seq;
  envelope.produce_cycle = cycle;
  envelope.target_cycle = cycle;
  envelope.t_cycle_start_ns = 1000000000;
  envelope.t_produce_start_ns = 1000001000;
  envelope.t_produce_finish_ns = 1000002000;
  envelope.t_publish_ns = 1000003000;
  envelope.period_ns = 50000000;
  envelope.publish_deadline_ns = 10000000;
  envelope.ttl_ns = 40000000;
  envelope.producer_status = ProducerStatus::Success;
  envelope.clock_ok = true;
  envelope.payload_type = "bytes";
  envelope.schema_id = "solution.v1";
  envelope.payload = {1, 2, 3, 4};
  envelope.payload_crc32c = EnvelopeCodec::crc32c(envelope.payload.data(), envelope.payload.size());
  return envelope;
}

SessionConfig makeSessionConfig() {
  SessionConfig config;
  config.team_id = "team-a";
  config.session_id = "session-a";
  config.task_id = "formation";
  config.self_id = "uav1";
  config.required_participants = {"uav1", "uav2"};
  config.epoch_ns = 1000;
  config.period_ns = 100;
  return config;
}

}  // namespace

TEST(SessionManagerTest, ConfiguresArmsAndStartsWhenDue) {
  SessionManager manager;
  const auto config = makeSessionConfig();

  EXPECT_TRUE(manager.configure(config));
  EXPECT_EQ(SessionState::CONFIGURED, manager.state());
  EXPECT_TRUE(manager.arm(2000));
  EXPECT_EQ(SessionState::ARMED, manager.state());
  EXPECT_FALSE(manager.startIfDue(1999));
  EXPECT_EQ(SessionState::ARMED, manager.state());
  EXPECT_TRUE(manager.startIfDue(2000));
  EXPECT_TRUE(manager.running());
}

TEST(SessionManagerTest, RejectsInvalidConfigAndBadClock) {
  SessionManager manager;
  auto config = makeSessionConfig();
  config.period_ns = 0;
  EXPECT_FALSE(manager.configure(config));
  EXPECT_EQ(SessionState::ERROR, manager.state());

  EXPECT_TRUE(manager.configure(makeSessionConfig()));
  ClockState bad_clock;
  bad_clock.clock_ok = false;
  bad_clock.quality = ClockQuality::BAD;
  EXPECT_FALSE(manager.arm(2000, bad_clock));
  EXPECT_EQ(SessionState::ERROR, manager.state());
}

TEST(CycleSchedulerTest, TickIsControlledAndDoesNotBackfillMissedCycles) {
  CycleScheduler scheduler;
  CycleSchedulerConfig config;
  config.session_id = "session-a";
  config.task_id = "formation";
  config.epoch_ns = 1000;
  config.period_ns = 100;
  ASSERT_TRUE(scheduler.configure(config));

  std::vector<uint64_t> callback_cycles;
  scheduler.setCallback([&callback_cycles](const CycleEvent& event) {
    callback_cycles.push_back(event.cycle_id);
  });
  scheduler.start();

  EXPECT_FALSE(scheduler.tick(999).has_value());
  auto first = scheduler.tick(1000);
  ASSERT_TRUE(first.has_value());
  EXPECT_EQ(0u, first->cycle_id);
  EXPECT_EQ(0, first->jitter_ns);

  EXPECT_FALSE(scheduler.tick(1049).has_value());
  auto skipped_to_three = scheduler.tick(1305);
  ASSERT_TRUE(skipped_to_three.has_value());
  EXPECT_EQ(3u, skipped_to_three->cycle_id);
  EXPECT_EQ(5, skipped_to_three->jitter_ns);
  EXPECT_EQ((std::vector<uint64_t>{0, 3}), callback_cycles);

  scheduler.stop();
  EXPECT_FALSE(scheduler.tick(1400).has_value());
}

TEST(EnvelopeCodecTest, EncodesDecodesAndValidatesSchemaAndCrc) {
  EnvelopeCodec codec;
  codec.allowSchema("solution", "bytes", "solution.v1");

  const auto envelope = makeEnvelope(5);
  const auto bytes = codec.encode(envelope);
  const auto decoded = codec.decode(bytes);

  ASSERT_TRUE(decoded.ok) << decoded.error;
  EXPECT_EQ(envelope.session_id, decoded.envelope.session_id);
  EXPECT_EQ(envelope.channel, decoded.envelope.channel);
  EXPECT_EQ(envelope.payload, decoded.envelope.payload);
  EXPECT_EQ(EnvelopeCodec::crc32c(envelope.payload.data(), envelope.payload.size()),
            decoded.envelope.payload_crc32c);

  EnvelopeCodec schema_rejecting_codec;
  schema_rejecting_codec.allowSchema("solution", "bytes", "solution.v2");
  const auto bad_schema = schema_rejecting_codec.decode(bytes);
  EXPECT_FALSE(bad_schema.ok);
  EXPECT_EQ(SampleStatus::BadSchema, bad_schema.status);

  auto corrupted = bytes;
  corrupted.back() ^= 0xffu;
  const auto bad_crc = codec.decode(corrupted);
  EXPECT_FALSE(bad_crc.ok);
  EXPECT_EQ(SampleStatus::BadPayload, bad_crc.status);
}

TEST(DeadlineCheckerTest, ClassifiesFreshLateWrongCycleAndBadSchema) {
  DeadlineChecker checker;
  ChannelTiming timing;
  timing.publish_deadline_ns = 10000000;
  timing.receive_cutoff_ns = 20000000;
  timing.ttl_ns = 40000000;
  timing.payload_type = "bytes";
  timing.schema_id = "solution.v1";

  const auto envelope = makeEnvelope(7);
  auto decision = checker.checkReceivedSample(envelope, 7, 1000000000, 1000005000, timing);
  EXPECT_EQ(SampleStatus::Fresh, decision.status);
  EXPECT_TRUE(decision.usable);

  decision = checker.checkReceivedSample(envelope, 8, 1000000000, 1000005000, timing);
  EXPECT_EQ(SampleStatus::WrongCycle, decision.status);

  decision = checker.checkReceivedSample(envelope, 7, 1000000000, 1030000000, timing);
  EXPECT_EQ(SampleStatus::Late, decision.status);

  auto bad_schema = envelope;
  bad_schema.schema_id = "solution.v2";
  decision = checker.checkReceivedSample(bad_schema, 7, 1000000000, 1000005000, timing);
  EXPECT_EQ(SampleStatus::BadSchema, decision.status);
}

TEST(SampleBufferTest, StoresSamplesDetectsDuplicateAndSeqGap) {
  SampleBuffer buffer;
  const auto first = makeEnvelope(1, 1);
  auto result = buffer.add(first, 100);
  EXPECT_TRUE(result.inserted);
  EXPECT_EQ(SampleStatus::Fresh, result.status);
  ASSERT_TRUE(buffer.find("uav2", "solution", 1).has_value());
  ASSERT_TRUE(buffer.getLastGood("uav2", "solution").has_value());

  result = buffer.add(first, 110);
  EXPECT_FALSE(result.inserted);
  EXPECT_TRUE(result.duplicate);
  EXPECT_EQ(SampleStatus::Duplicate, result.status);

  const auto fourth = makeEnvelope(2, 4);
  result = buffer.add(fourth, 200);
  EXPECT_TRUE(result.inserted);
  EXPECT_EQ(2u, result.seq_gap);
  EXPECT_EQ(2u, buffer.seqGapCount("uav2", "solution"));
  EXPECT_EQ(2u, buffer.getLastGood("uav2", "solution")->envelope.target_cycle);
}

TEST(SnapshotBuilderTest, BuildsFreshMissingAndBadStats) {
  SampleBuffer buffer;
  ASSERT_TRUE(buffer.add(makeEnvelope(10, 1), 1000010000).inserted);

  auto bad_clock = makeEnvelope(10, 1);
  bad_clock.sender_id = "uav3";
  bad_clock.clock_ok = false;
  ASSERT_TRUE(buffer.add(bad_clock, 1000010000).inserted);

  ChannelTiming timing;
  timing.payload_type = "bytes";
  timing.schema_id = "solution.v1";
  timing.receive_cutoff_ns = 20000000;
  timing.ttl_ns = 40000000;

  SnapshotBuildRequest request;
  request.session_id = "session-a";
  request.task_id = "formation";
  request.self_id = "uav1";
  request.cycle_id = 10;
  request.cycle_start_ns = 1000000000;
  request.period_ns = 50000000;
  request.build_time_ns = 1000010000;
  request.local_clock = ClockState{true, 0, 0, ClockQuality::OK, "test"};
  request.local_runtime.running = true;
  request.expected_samples = {
      ExpectedSample{"uav2", "solution", true, timing},
      ExpectedSample{"uav3", "solution", true, timing},
      ExpectedSample{"uav4", "solution", false, timing},
  };

  SnapshotBuilder builder;
  const auto snapshot = builder.build(request, buffer);
  ASSERT_EQ(3u, snapshot.samples.size());
  EXPECT_EQ(SampleStatus::Fresh, snapshot.samples[0].status);
  EXPECT_EQ(SampleStatus::SenderClockBad, snapshot.samples[1].status);
  EXPECT_EQ(SampleStatus::Missing, snapshot.samples[2].status);
  EXPECT_EQ(1u, snapshot.stats.fresh_count);
  EXPECT_EQ(1u, snapshot.stats.missing_count);
  EXPECT_EQ(1u, snapshot.stats.bad_count);
  EXPECT_FALSE(snapshot.all_required_fresh);
  EXPECT_FALSE(snapshot.all_required_usable);
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
