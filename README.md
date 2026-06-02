# XGC2 Runtime Sync

ROS1 runtime synchronization messages and coordinator for XGC2 multi-agent systems.

This repository currently provides the `periodic_sync` ROS package. It coordinates synchronized periodic process triggering with participant readiness, trigger sequence IDs, acknowledgments, timeout detection, and health statistics.

## Install

```bash
sudo apt update
sudo apt install ros-noetic-xgc2-runtime-sync
```

## Smoke Test

```bash
rospack find periodic_sync
rosrun periodic_sync sync_coordinator _num_uavs:=1
```
