# 02. Architecture Design

## Overview

The staged design separates the ROS1 package surface from a ROS-free runtime
core.

```text
Ground station
  chrony or PTP reference
  zenohd router
  session coordinator
  health monitor
  recorder

Vehicle runtime
  swarm_sync_core
    session manager
    cycle scheduler
    zenoh transport
    envelope codec
    deadline checker
    sample buffer
    snapshot builder
    clock monitor
    health monitor

  swarm_sync_ros1
    runtime node
    topic export adapter
    topic import adapter
    cycle publisher
    snapshot publisher
    service API
```

In this repository the resources are staged under the existing ROS package
name `periodic_sync`. The original periodic sync messages remain present for
compatibility.

## Modules

SessionManager:

- Load static and ground-station session config.
- Validate config version.
- Manage IDLE, CONFIGURED, ARMED, RUNNING, STOPPED, and ERROR states.
- Handle start and stop requests.

CycleScheduler:

- Generate cycle events from epoch and period.
- Compute cycle id, expected time, actual time, and jitter.
- Skip missed historical cycles.

ZenohTransport:

- Connect to a router or peer topology.
- Publish and subscribe SampleEnvelope records.
- Publish health and consume config or command keys.
- Apply per-channel QoS.

EnvelopeCodec:

- Build and validate sample envelopes.
- Check schema id, version, checksum, target cycle, and sender clock fields.
- Keep business payload bytes opaque.

SampleBuffer:

- Store samples by peer, channel, and target cycle.
- Handle duplicate, out-of-order, late, future-cycle, and bad samples.
- Preserve recent samples and last-good samples for diagnostics.

DeadlineChecker:

- Classify publish deadline misses.
- Classify receive cutoff, TTL expiry, late samples, and wrong-cycle samples.

SnapshotBuilder:

- Freeze the receive buffer at cycle boundaries.
- Emit CycleSnapshot and PeerSampleStatus facts.
- Aggregate fresh, missing, late, wrong-cycle, and bad counts.

ClockMonitor:

- Read chrony, PTP, or mock clock status.
- Expose clock offset, uncertainty, and quality.

HealthMonitor:

- Aggregate runtime state, transport state, timing statistics, and drop counts.

## Key Scheme

```text
swarm/{team}/session/{session}/task/{task}/sample/{channel}/{sender}/{target_cycle}
swarm/{team}/session/{session}/uav/{uav}/health
swarm/{team}/session/{session}/uav/{uav}/clock
swarm/{team}/session/{session}/config
swarm/{team}/session/{session}/cmd/start
swarm/{team}/session/{session}/cmd/stop
swarm/{team}/session/{session}/ack/{uav}
```

## ROS1 Topics and Services

Expected topics:

- `/swarm_sync/cycle`
- `/swarm_sync/snapshot`
- `/swarm_sync/health`
- `/swarm_sync/sample_stats`
- Configured import topics such as `/remote_solution/<peer>`

Expected services:

- `/swarm_sync/start_session`
- `/swarm_sync/stop_session`
- `/swarm_sync/get_runtime_status`

The concrete node implementation and build wiring are intentionally left for
the later integration step.
