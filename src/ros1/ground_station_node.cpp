#include <algorithm>
#include <string>

#include <ros/ros.h>

#include "periodic_sync/BudgetReport.h"
#include "periodic_sync/WeakNetState.h"
#include "swarm_sync_core/weaknet/weaknet.hpp"

int main(int argc, char** argv) {
  ros::init(argc, argv, "swarm_ground_station_node");
  ros::NodeHandle nh;
  ros::NodeHandle pnh("~");
  std::string session_config;
  std::string session_id;
  std::string node_id;
  int participant_count = 1;
  int payload_bytes_p95 = 600;
  double frequency_hz = 20.0;
  double max_realtime_bps = 2000000.0;
  double safety_factor = 1.35;
  pnh.param<std::string>("session_config", session_config, "");
  pnh.param<std::string>("session_id", session_id, "ground_session");
  pnh.param<std::string>("node_id", node_id, "ground_station");
  pnh.param<int>("participant_count", participant_count, 1);
  pnh.param<int>("payload_bytes_p95", payload_bytes_p95, 600);
  pnh.param<double>("frequency_hz", frequency_hz, 20.0);
  pnh.param<double>("max_realtime_bps", max_realtime_bps, 2000000.0);
  pnh.param<double>("safety_factor", safety_factor, 1.35);

  swarm_sync::weaknet::ChannelBudgetInput input;
  input.channel = "realtime_sample";
  input.publishers = static_cast<uint32_t>(std::max(1, participant_count));
  input.effective_fanout = static_cast<uint32_t>(std::max(0, participant_count - 1));
  input.frequency_hz = frequency_hz;
  input.payload_bytes_p95 = static_cast<uint32_t>(std::max(0, payload_bytes_p95));
  const auto budget = swarm_sync::weaknet::computeTrafficBudget({input},
                                                                max_realtime_bps,
                                                                safety_factor);

  auto budget_pub = nh.advertise<periodic_sync::BudgetReport>("budget_report", 1, true);
  auto state_pub = nh.advertise<periodic_sync::WeakNetState>("weaknet_state", 1, true);

  periodic_sync::BudgetReport budget_msg;
  budget_msg.header.stamp = ros::Time::now();
  budget_msg.session_id = session_id;
  budget_msg.accepted = budget.accepted;
  budget_msg.total_realtime_bps = budget.total_realtime_bps;
  budget_msg.max_realtime_bps = budget.max_realtime_bps;
  budget_msg.utilization_ratio = budget.utilization_ratio;
  budget_msg.report_json = "{\"reason\":\"" + budget.reason + "\"}";
  budget_pub.publish(budget_msg);

  periodic_sync::WeakNetState state_msg;
  state_msg.header.stamp = budget_msg.header.stamp;
  state_msg.node_id = node_id;
  state_msg.previous_state = static_cast<uint8_t>(swarm_sync::weaknet::LinkState::Good);
  state_msg.current_state = budget.accepted
                                ? static_cast<uint8_t>(swarm_sync::weaknet::LinkState::Good)
                                : static_cast<uint8_t>(swarm_sync::weaknet::LinkState::Congested);
  state_msg.reason = budget.reason;
  state_msg.evidence_json = "{\"budget_utilization_ratio\":" +
                            std::to_string(budget.utilization_ratio) + "}";
  state_pub.publish(state_msg);

  ROS_INFO_STREAM("swarm_ground_station_node ready"
                  << (session_config.empty() ? "" : " session_config=" + session_config)
                  << " budget_ratio=" << budget.utilization_ratio
                  << " accepted=" << budget.accepted);
  ros::spin();
  return 0;
}
