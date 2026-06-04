# 01. Requirements Analysis

## Goal

Provide a ROS1 package surface for a ROS-free synchronized task communication
runtime. Each vehicle runs the runtime locally. The ground station coordinates
session start and health monitoring. Zenoh carries task-semantic samples across
the wireless network.

## Core Principles

- Do not bridge the complete ROS1 graph.
- Communicate task channels, not ROS topic names.
- Keep payloads opaque.
- Let algorithms own fallback, prediction, and safety policy.
- Make timing evidence visible through messages and statistics.

## Roles

Vehicle runtime:

- Generate local synchronized cycle events.
- Export configured ROS1 topics into task channels.
- Import peer task samples back into configured ROS1 topics.
- Publish cycle, snapshot, health, and statistics messages.
- Expose session control and status services.

Ground station:

- Manage session configuration and shared start epoch.
- Check participant readiness and clock health.
- Monitor runtime health and timing statistics.
- Record experiment evidence.

## Functional Requirements

F1 Session management:

- Create, start, stop, and inspect sessions.
- Configure participants, channels, deadlines, TTL, QoS, epoch, and period.
- Support ready and ACK behavior.

F2 Synchronized cycles:

- Support 0.5 Hz to 50 Hz.
- Use local synchronized clocks rather than a per-cycle ground-station tick.
- Include expected time, actual time, jitter, and clock quality.
- Do not replay missed historical cycles.

F3 Zenoh task communication:

- Publish by session, task, channel, sender, and target cycle.
- Subscribe by task key expression.
- Support best-effort drop profiles for realtime samples.
- Support reliable profiles for config and commands.

F4 Sample envelope:

- Include schema id, sequence, source, target cycle, timestamps, clock status,
  and payload checksum.
- Validate envelope version, schema id, checksum, and timing window.

F5 Cycle snapshot:

- Report one status entry per expected peer and channel.
- Count fresh, missing, late, wrong-cycle, bad-schema, and bad-payload samples.
- Preserve enough evidence for offline replay.

F6 ROS1 adapter:

- Use explicit export and import allowlists.
- Rate limit exported topics.
- Never bridge /tf, /rosout, image, point cloud, or debug topics by default.

F7 Clock health:

- Read chrony or PTP health.
- Reject start or mark degraded when clock quality is insufficient.
- Mark outgoing and incoming samples when sender clock quality is bad.
- Treat the ground station as the default LAN time authority.
- Gate preflight session start on selected source, offset, uncertainty, and
  leap status.
- Disallow in-flight system-time step in Runtime Sync; only observe and mark
  degraded when clock health worsens.
- Keep local control-period `dt` on monotonic or steady clocks; use
  synchronized ROS/system time only for cross-machine stamps and future
  execution timestamps.

F8 Observability:

- Publish runtime health, cycle jitter, drop reasons, sequence gaps, late
  samples, and memory or CPU usage.

## Non-Functional Requirements

- Low onboard overhead.
- ROS-free core implementation.
- Replaceable ROS1 adapter layer.
- Testable Zenoh transport and timing logic.
- Extensible participants, tasks, channels, rates, and offsets.

## Out of Scope

- MPC model design.
- Optimizer wrappers.
- Trajectory format ownership.
- Flight-control command execution.
- Safety fallback policy.
- Full ROS1 multi-machine networking.
- Executing `chronyc makestep`, `ntpdate`, or any other system-time correction
  command from this package.
