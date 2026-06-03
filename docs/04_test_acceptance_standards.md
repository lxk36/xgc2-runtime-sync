# 04. Test And Acceptance Standards

## Acceptance Goal

Acceptance proves that the middleware can provide synchronized cycle events,
deadline-aware sample communication, structured cycle snapshots, clock and
network health, ROS1 adapter behavior, ground-station session control, and
explainable drop or status statistics under weak links.

## Test Environment

Minimum:

- One ground station.
- Two vehicles or Linux hosts.
- Same LAN.
- chrony or a mocked clock monitor.
- Zenoh router.
- ROS Noetic.

Recommended:

- One ground station.
- Three to five vehicles or Linux hosts.
- Wireless link or controllable network.
- `tc netem` fault injection.
- Log collection or dashboarding.

## Unit Tests

Cycle scheduler:

- Frequency/period validation accepts 0.5 Hz through 50 Hz and rejects values outside that range.
- Future epoch does not trigger early.
- cycle_id starts at 0 when epoch is reached.
- 20 Hz for 100 cycles increments continuously.
- Delayed callbacks do not replay historical cycles.
- StopSession stops new cycles.

Envelope codec:

- Normal payload round trips.
- Bad checksum reports BadPayload.
- Wrong schema id reports BadSchema.
- Unsupported envelope version is rejected.
- Empty payload follows channel policy.

Transport and task-semantic keys:

- Sample keys are task-semantic and include session, task, channel, and schema fields.
- Empty key fields and ROS graph-like names with slash separators are rejected.
- In-memory transport dispatches only to subscribers on the exact same key.
- Different task keys remain isolated.
- Unsubscribe stops further delivery for the removed subscriber.

Deadline checker:

- Target cycle before cutoff is Fresh.
- Arrival after cutoff is Late.
- Old target cycle is WrongCycle or Expired.
- Future target cycle is buffered.
- TTL expiry is marked unusable.

Sample buffer:

- One peer, channel, and cycle stores successfully.
- Duplicate sequence is marked Duplicate.
- Sequence gaps are counted.
- Multiple peers and channels remain isolated.
- Last-good lookup returns the latest usable sample.

Snapshot builder:

- All required peers fresh sets all_required_fresh.
- Missing required peer is reported Missing.
- Bad sender clock is reported SenderClockBad.
- Bad schema and late samples are counted.

## Integration Tests

Single-machine loopback:

- Start runtime.
- Publish a local sample.
- Receive it through loopback.
- Generate snapshot.

Two-machine Zenoh router:

- Ground station runs zenohd.
- Two runtimes connect to zenohd.
- Each side publishes channel `solution`.
- Each snapshot sees the other peer as Fresh.

Ground-station session start:

- Ground station distributes config.
- Vehicles ACK.
- Ground station sends StartSession with a shared epoch.
- Vehicles enter RUNNING at the same epoch and cycle id.

ROS1 adapter:

- Local algorithm publishes `/local_solution`.
- Runtime exports to Zenoh.
- Remote runtime imports to `/remote_solution/<peer>`.
- Unconfigured topics are not forwarded.

## Weak-Network Tests

Use `tools/netem_profiles.sh` to inject delay, loss, reorder, duplicate, and
corrupt conditions. Acceptance requires that late, missing, wrong-cycle,
duplicate, bad-payload, and sequence-gap counters remain explainable and that
old samples do not block new cycles.

## Time-Sync Tests

- Good chrony state allows session start.
- Unsynchronized chrony rejects start when required.
- Bad sender clock marks outgoing and incoming samples.
- High clock uncertainty produces degraded or bad health.

## Performance Tests

Run 0.5, 2, 10, 20, and 50 Hz profiles for at least 10 minutes per profile.
Record CPU, memory, jitter p95 and p99, drops, sequence gaps, late samples, and
snapshot build cost. Initial acceptance expects no memory growth and no queue
backlog at 50 Hz.

## ROS1 Package Acceptance

After build metadata and source integration:

- `catkin_make` or `catkin build` passes.
- `roslaunch periodic_sync swarm_runtime.launch` starts.
- `/swarm_sync/cycle` publishes.
- `/swarm_sync/get_runtime_status` responds.
- The configured allowlist controls all topic forwarding.
