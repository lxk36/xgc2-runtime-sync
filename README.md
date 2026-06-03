# XGC2 Runtime Sync

ROS1 runtime synchronization messages and coordinator for XGC2 multi-agent systems.

This repository currently provides the `periodic_sync` ROS package. It coordinates synchronized periodic process triggering with participant readiness, trigger sequence IDs, timeout detection, cycle snapshots, and health statistics.

The weak-network layer is implemented under `swarm_sync_core/weaknet` and remains payload opaque. It provides channel QoS profiles, receive cutoff plus late-pocket classification, payload and traffic budget guards, per-peer/channel link health, deterministic send staggering, communication-only recommendations, and a GOOD/DEGRADED/CONGESTED/PARTITIONED/RECOVERING state machine. It does not apply algorithm fallback and it does not add per-sample ACKs to realtime traffic.

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

## Runtime

```bash
roslaunch periodic_sync swarm_runtime.launch
roslaunch periodic_sync ground_station.launch participant_count:=3 frequency_hz:=20.0
```

Useful weak-network topics:

```text
/budget_report
/peer_link_health
/recommendation
/weaknet_state
```
