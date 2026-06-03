#include <gtest/gtest.h>

#include <ros/ros.h>

#include "periodic_sync/CycleSnapshot.h"
#include "periodic_sync/GetRuntimeStatus.h"
#include "periodic_sync/PeerSampleStatus.h"
#include "periodic_sync/SyncedCycle.h"

namespace {

periodic_sync::SyncedCycle last_cycle;
periodic_sync::CycleSnapshot last_snapshot;
bool received_cycle = false;
bool received_snapshot = false;

void cycleCallback(const periodic_sync::SyncedCycle::ConstPtr& msg) {
  last_cycle = *msg;
  received_cycle = true;
}

void snapshotCallback(const periodic_sync::CycleSnapshot::ConstPtr& msg) {
  last_snapshot = *msg;
  received_snapshot = true;
}

}  // namespace

TEST(RuntimeNodeSmokeTest, PublishesCycleAndServesStatus) {
  ros::NodeHandle nh;
  const auto cycle_sub = nh.subscribe("/swarm_sync/cycle", 4, cycleCallback);
  const auto snapshot_sub = nh.subscribe("/cycle_snapshot", 4, snapshotCallback);

  const ros::Time deadline = ros::Time::now() + ros::Duration(5.0);
  ros::Rate rate(100.0);
  while (ros::ok() &&
         !(received_cycle && received_snapshot && last_cycle.cycle_id == last_snapshot.cycle_id) &&
         ros::Time::now() < deadline) {
    ros::spinOnce();
    rate.sleep();
  }

  ASSERT_TRUE(received_cycle);
  ASSERT_TRUE(received_snapshot);
  EXPECT_EQ(last_cycle.cycle_id, last_snapshot.cycle_id);
  EXPECT_EQ("test_team", last_cycle.team_id);
  EXPECT_EQ("smoke_session", last_cycle.session_id);
  EXPECT_EQ("smoke_task", last_cycle.task_id);
  EXPECT_EQ("uav_test", last_cycle.self_id);
  EXPECT_TRUE(last_cycle.clock_ok);
  EXPECT_TRUE(last_cycle.session_running);

  EXPECT_EQ("test_team", last_snapshot.team_id);
  EXPECT_EQ("smoke_session", last_snapshot.session_id);
  EXPECT_EQ("smoke_task", last_snapshot.task_id);
  EXPECT_EQ("uav_test", last_snapshot.self_id);
  EXPECT_TRUE(last_snapshot.local_clock_ok);
  ASSERT_EQ(1u, last_snapshot.samples.size());
  EXPECT_EQ("uav_test", last_snapshot.samples.front().peer_id);
  EXPECT_EQ("runtime_state", last_snapshot.samples.front().channel);
  EXPECT_EQ(periodic_sync::PeerSampleStatus::STATUS_MISSING, last_snapshot.samples.front().status);
  EXPECT_EQ(last_snapshot.cycle_id, last_snapshot.samples.front().expected_target_cycle);
  EXPECT_EQ(1u, last_snapshot.missing_count);
  EXPECT_FALSE(last_snapshot.all_required_fresh);
  EXPECT_FALSE(last_snapshot.all_required_usable);

  ros::ServiceClient client =
      nh.serviceClient<periodic_sync::GetRuntimeStatus>("/swarm_sync/get_runtime_status");
  ASSERT_TRUE(client.waitForExistence(ros::Duration(3.0)));

  periodic_sync::GetRuntimeStatus request;
  request.request.session_id = "smoke_session";
  ASSERT_TRUE(client.call(request));
  EXPECT_TRUE(request.response.running);
  EXPECT_EQ("RUNNING", request.response.state);
  EXPECT_TRUE(request.response.clock_ok);
  EXPECT_GE(request.response.current_cycle, last_cycle.cycle_id);
  EXPECT_NE(std::string::npos, request.response.reason.find("adapters=1/1"));
}

int main(int argc, char** argv) {
  ros::init(argc, argv, "runtime_node_smoke_test");
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
