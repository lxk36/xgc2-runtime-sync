#include <gtest/gtest.h>

#include <ros/ros.h>

#include "periodic_sync/GetRuntimeStatus.h"
#include "periodic_sync/SyncedCycle.h"

namespace {

periodic_sync::SyncedCycle last_cycle;
bool received_cycle = false;

void cycleCallback(const periodic_sync::SyncedCycle::ConstPtr& msg) {
  last_cycle = *msg;
  received_cycle = true;
}

}  // namespace

TEST(RuntimeNodeSmokeTest, PublishesCycleAndServesStatus) {
  ros::NodeHandle nh;
  const auto sub = nh.subscribe("/swarm_sync/cycle", 4, cycleCallback);

  const ros::Time deadline = ros::Time::now() + ros::Duration(5.0);
  ros::Rate rate(100.0);
  while (ros::ok() && !received_cycle && ros::Time::now() < deadline) {
    ros::spinOnce();
    rate.sleep();
  }

  ASSERT_TRUE(received_cycle);
  EXPECT_EQ("test_team", last_cycle.team_id);
  EXPECT_EQ("smoke_session", last_cycle.session_id);
  EXPECT_EQ("smoke_task", last_cycle.task_id);
  EXPECT_EQ("uav_test", last_cycle.self_id);
  EXPECT_TRUE(last_cycle.clock_ok);
  EXPECT_TRUE(last_cycle.session_running);

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
}

int main(int argc, char** argv) {
  ros::init(argc, argv, "runtime_node_smoke_test");
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
