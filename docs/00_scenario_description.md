# 00. Scenario Description

This package stages the ROS1-facing resources for a synchronized swarm
communication runtime. The runtime is intended for heterogeneous UAV or robot
swarms that still run local ROS1 sensor, estimator, control, and planning
nodes, while cross-vehicle data exchange is carried by Zenoh with task-level
semantics.

The runtime should not bridge the full ROS graph over the wireless link. It
exports only configured task channels, wraps each payload in a timing-aware
sample envelope, and gives local algorithms a structured view of which peer
data was fresh, late, missing, malformed, or affected by bad sender clock
quality.

## Typical Cycle

At the start of cycle k, each vehicle generates the cycle event locally from a
synchronized system clock. The runtime freezes the receive buffer from the
previous window and builds a CycleSnapshot for the algorithm.

During cycle k, the local algorithm computes and hands an opaque business
payload to the runtime. The runtime wraps it as a SampleEnvelope and publishes
it through Zenoh. Other peers receive samples asynchronously and classify them
against the target cycle and channel deadlines.

At the start of cycle k+1, the received samples are exposed through the next
snapshot. The runtime reports facts; the algorithm owns fallback, prediction,
and safety policy decisions.

## Ground Station

The ground station provides the chrony or PTP reference, a Zenoh router when
needed, session coordination, runtime health monitoring, and experiment
recording. It starts a session by distributing config, waiting for readiness,
checking clock health, and issuing a shared epoch.

## Constraints

- Cross-machine keys use team, session, task, channel, peer, and target cycle.
- Payload bytes are opaque to the runtime.
- Old or late data must not block newer cycles.
- Timing evidence is first-class output.
- ROS1 integration is an adapter layer, not the runtime core.
