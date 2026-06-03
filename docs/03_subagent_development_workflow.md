# 03. Development Workflow

This document keeps the design bundle work split into bounded ownership areas.
For the current first phase, only ROS1 resources are staged: messages, services,
config, launch files, helper tools, and documentation.

## Work Areas

Specification:

- Keep requirements, architecture, acceptance standards, and ADRs consistent.
- Track compatibility with the existing `periodic_sync` package surface.

Core runtime:

- Implement the ROS-free session, scheduler, buffer, deadline, envelope, clock,
  and health components.

ROS1 adapter:

- Wire the runtime into ROS1 nodes.
- Publish cycle, snapshot, health, and statistics topics.
- Expose start, stop, and status services.
- Implement explicit topic export and import allowlists.

Transport:

- Implement Zenoh connection, sample key handling, QoS mapping, and command
  channels.

Validation:

- Cover unit, integration, weak-network, time-sync, and ROS package acceptance.
- Keep test evidence tied to cycle id, peer id, channel, and drop reason.

## Phase 1 Deliverables

- Add new runtime message definitions.
- Add new session service definitions.
- Add config examples for session, channels, ROS1 adapters, and QoS.
- Add launch skeletons for the runtime and ground station nodes.
- Add chrony and netem helper scripts.
- Add ASCII documentation.
- Keep legacy `Sync*` messages in place.

## Follow-Up Integration

The next phase must update CMakeLists.txt and package.xml to register the new
messages, services, dependencies, installed config, launch files, scripts, and
tools. Source and include files are not staged in this phase.
