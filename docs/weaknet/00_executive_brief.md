# 00. 执行摘要

本弱网优化包把系统目标压缩为一句话：

> 在无线多机同步计算场景中，中间件不保证每个包都到，但必须保证每个周期的数据状态被准确判定、统计、记录和暴露给算法。

## 核心判断

弱网中最危险的问题不是丢包，而是：

```text
- 旧数据堵住新数据；
- 迟到数据被误用为本周期 fresh 数据；
- 同步周期边界触发无线 burst；
- payload 或 fanout 超预算导致全局拥塞；
- 算法无法区分“没发、迟到、发送方超时、接收端拥塞、时钟不可信”；
- 没有统一状态机，导致开发和调度混乱。
```

因此中间件的目标函数应从 `packet delivery rate` 转为：

```text
cycle_usability_ratio
fresh_ratio
late_ratio
missing_ratio
wrong_cycle_acceptance = 0
max_queue_age_cycles <= 1
state_transition_traceability
startup_budget_validity
```

## 顶层架构

```text
Ground Station
  ├─ chrony/NTP server
  ├─ zenohd router
  ├─ session coordinator
  ├─ weaknet referee / monitor
  └─ recorder

Each UAV
  ├─ swarm_sync_core       # ROS-free
  │   ├─ cycle scheduler
  │   ├─ channel QoS manager
  │   ├─ receive window + late pocket
  │   ├─ send stagger
  │   ├─ link health metrics
  │   ├─ weaknet state machine
  │   ├─ traffic budget / payload guard
  │   └─ recommendation engine
  └─ swarm_sync_ros1       # ROS1 adapter only
      ├─ /swarm_sync/cycle
      ├─ /swarm_sync/snapshot
      ├─ /swarm_sync/health
      └─ configurable topic import/export
```

## 中间件输出给算法的内容

算法每周期获得 `CycleSnapshot`：

```text
cycle_id
local_clock_quality
runtime_state
per peer/channel sample status:
  Fresh / Missing / Late / WrongCycle / Duplicate / BadPayload / SenderClockBad / ProducerReportedFail
statistics:
  fresh_count / late_count / missing_count / p95 latency / queue_age / state
history access:
  get_last_good(peer, channel)
```

中间件不输出：

```text
UsePrevious / UsePrediction / UseBackup / InflateObstacle / Hover / Land
```

这些属于算法层。
