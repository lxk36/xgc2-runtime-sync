# 11. Ground Station Referee

## 地面站职责

地面站是：

```text
Time Authority
Zenoh Router
Session Coordinator
Weaknet Referee
Recorder / Dashboard
```

不是：

```text
per-cycle tick broadcaster
algorithm fallback controller
payload interpreter
```

## 核心模块

```text
ground_time_service
  chrony/NTP server，系统级服务。

ground_zenoh_router
  zenohd 或多 router profile。

ground_session_coordinator
  下发 session config、epoch、channel profiles、participants。

ground_weaknet_referee
  聚合每机、每边、每 channel 的指标。

ground_recorder
  记录 CycleSnapshot、Late Pocket 摘要、state transitions、recommendations。
```

## Dashboard 必须回答的问题

```text
1. 当前 session 是否 ready？
2. 哪些节点 clock_ok？
3. 每个周期 Fresh/Late/Missing 比例是多少？
4. 哪条 peer/channel 链路正在退化？
5. 是 payload 过大、burst、拓扑问题，还是计算发布时间太晚？
6. 当前 state machine 状态是什么？为什么迁移？
7. 中间件建议是什么？证据是什么？
8. 错峰是否生效？发送分布是否仍然聚集？
```

## 地面站输出

```text
/swarm_sync/session_status
/swarm_sync/global_link_health
/swarm_sync/recommendations
/swarm_sync/budget_report
/swarm_sync/state_transitions
```

## 验收

```text
- ground station 可显示 per peer/channel 指标。
- 可导出 session report。
- 可追溯任意状态迁移。
- 可对比错峰前后 burst 指标。
```
