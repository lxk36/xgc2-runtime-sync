# 07. Deterministic Send Stagger：主动错峰

## 目的

同步计算会天然产生同步发送高峰。不能依赖“各机计算时间不稳定”来被动打散 burst，因为这种副作用不可控、不可复盘。

中间件应提供主动、确定性、可追溯的微错峰。

## 基本模式

```yaml
send_stagger:
  enabled: true
  mode: deterministic_micro_slot
  max_spread_ms: 5
  per_channel:
    realtime_sample:
      enabled: true
      max_spread_ms: 5
    health:
      enabled: true
      max_spread_ms: 500
```

## 计算方法

错峰 offset 必须可复现：

```text
slot_count = ceil(max_spread_ns / slot_width_ns)
slot_index = stable_hash(session_id, channel, sender_id, cycle_id) % slot_count
send_offset_ns = slot_index * slot_width_ns
```

也可以在 session 启动时由 ground station 分配静态 slot：

```text
uav01: slot 0
uav02: slot 1
uav03: slot 2
```

## 记录要求

每条发送样本的 envelope 或统计中记录：

```text
stagger_enabled
stagger_slot
stagger_offset_ns
stagger_reason
intended_publish_time_ns
actual_publish_time_ns
```

## 重要边界

```text
- 错峰不改变 target_cycle。
- 错峰时间必须计入 publish_deadline。
- 如果算法明确要求立即发送，可对该 channel 关闭 stagger。
- ground station 必须能画出每周期发送分布，证明 burst 被打散。
```

## 验收

```text
- N 个节点同周期发送时，actual publish time 分布覆盖配置的 spread 窗口。
- 同一 session/config 下 offset 可复现。
- 关闭 stagger 后恢复 immediate 模式。
- 所有 stagger 决策可追溯。
```
