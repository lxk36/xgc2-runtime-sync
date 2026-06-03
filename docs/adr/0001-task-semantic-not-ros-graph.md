# ADR 0001: Task Semantics Instead Of ROS Graph Bridging

## Status

Accepted for staging.

## Decision

The runtime communicates task-semantic channels over Zenoh instead of bridging
the complete ROS1 graph.

## Rationale

Full graph bridging can push unrelated topics such as `/tf`, `/rosout`, image,
point-cloud, debug, and diagnostic streams over the wireless link. It also
makes per-task deadlines and QoS difficult to enforce. Task channels keep the
wireless surface explicit and allow each channel to define schema, rate,
deadline, TTL, and QoS.

## Consequences

- Every exported or imported topic must be configured explicitly.
- Cross-machine keys are not ROS topic names.
- Algorithms must bind their local ROS topics to task channels through adapter
  config.
