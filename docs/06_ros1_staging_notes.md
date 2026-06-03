# 06. ROS1 Staging Notes

This directory contains first-phase resources imported from the design bundle
into the existing `periodic_sync` package. The package metadata and source tree
were intentionally not modified in this phase.

## Preserved Compatibility

The legacy messages below remain in `msg/`:

- SyncReady
- SyncTrigger
- SyncAck
- SyncParticipantStats
- SyncStatistics
- SyncEvent

## Newly Staged Messages

- SyncedCycle
- PeerSampleStatus
- CycleSnapshot
- RuntimeHealth
- SampleStats

## Newly Staged Services

- StartSession
- StopSession
- GetRuntimeStatus

## Required Build Integration

CMakeLists.txt must later add the new message files, add the service files,
call `generate_messages(DEPENDENCIES std_msgs)`, add `std_msgs` to
`find_package(catkin REQUIRED COMPONENTS ...)`, and install config, launch,
script, and tool resources.

package.xml must later add `std_msgs` build and exec dependencies. The existing
`message_generation` and `message_runtime` dependencies remain required.
