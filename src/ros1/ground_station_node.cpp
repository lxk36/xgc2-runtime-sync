#include <ros/ros.h>

int main(int argc, char** argv) {
  ros::init(argc, argv, "swarm_ground_station_node");
  ros::NodeHandle pnh("~");
  std::string session_config;
  pnh.param<std::string>("session_config", session_config, "");
  ROS_INFO_STREAM("swarm_ground_station_node ready"
                  << (session_config.empty() ? "" : " session_config=" + session_config));
  ros::spin();
  return 0;
}
