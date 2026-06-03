# 13. 开发里程碑

## P0：实时路径不堵、不误判

交付：

```text
ChannelProfile
CycleWindow
LatePocket
NoAckStats
CycleSnapshot status
```

验收：

```text
late_as_fresh = 0
wrong_cycle_acceptance = 0
max_queue_age_cycles <= 1
```

## P1：可观测与状态机

交付：

```text
PeerChannelHealth
WeaknetStateMachine
GroundStationReferee 初版
state transition logs
```

验收：

```text
per peer/channel 指标可见
状态迁移有 reason/evidence
```

## P2：预算、Payload Guard、错峰

交付：

```text
TrafficBudgetCalculator
PayloadGuard
DeterministicSendStagger
```

验收：

```text
启动前预算报告
oversized 保护
发送分布可追溯
```

## P3：建议引擎和拓扑 profile

交付：

```text
RecommendationEngine
TopologyProfile
QuerySidePath
```

验收：

```text
建议有证据
query 不阻塞实时周期
profile 可切换
```

## P4：ROS1 集成与系统验收

交付：

```text
ROS1 msg/srv/topic adapter
launch files
netem test scripts
acceptance report generator
```

验收：

```text
ROS1 package 可编译
仿真多节点可运行
弱网矩阵通过
```
