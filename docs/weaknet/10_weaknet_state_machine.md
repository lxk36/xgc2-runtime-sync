# 10. Weaknet State Machine

## 为什么必须有状态机

没有状态机，调度会变成大量 if/else，开发不可扩展，地面站也无法解释系统当前处于什么网络状态。

状态机只描述通信与运行时状态，不决定算法策略。

## 状态定义

```text
GOOD
  Fresh 率高，latency 和 queue 正常。

DEGRADED
  出现轻微 late/missing，实时周期仍基本可用。

CONGESTED
  queue、payload、latency 或 missing 明显恶化，建议降频/增大 offset/关闭 bulk。

PARTITIONED
  一个或多个 required peer 长时间不可达，或 ground station 断连超过阈值。

RECOVERING
  链路从坏状态恢复，但需要稳定观察窗口，避免立即恢复高负载。
```

## 状态迁移示例

```yaml
transitions:
  GOOD_to_DEGRADED:
    if_any:
      - late_ratio_gt: 0.02
      - missing_ratio_gt: 0.01
    window_cycles: 100

  DEGRADED_to_CONGESTED:
    if_any:
      - fresh_ratio_lt: 0.90
      - rx_latency_p99_gt_period_ratio: 0.8
      - max_queue_age_cycles_gt: 1
    window_cycles: 100

  CONGESTED_to_PARTITIONED:
    if_any:
      - fresh_ratio_eq: 0.0
      - no_sample_cycles_gt: 10
    window_cycles: 10

  PARTITIONED_to_RECOVERING:
    if_all:
      - fresh_ratio_gt: 0.50
      - peer_seen_recently: true
    window_cycles: 20

  RECOVERING_to_GOOD:
    if_all:
      - fresh_ratio_gt: 0.995
      - late_ratio_lt: 0.005
      - missing_ratio_lt: 0.001
    window_cycles: 500
```

## 状态行为

状态机可以触发通信层行为和建议：

```text
DEGRADED:
  recommend disable bulk_debug

CONGESTED:
  recommend lower frequency
  recommend larger target_cycle_offset
  enforce payload guard

PARTITIONED:
  mark peer unavailable
  continue local cycles if policy allows

RECOVERING:
  recommend gradual recovery, do not immediately restore all channels
```

但状态机不得触发算法 fallback。

## 验收

```text
- 每次状态迁移有 previous_state、next_state、reason、metrics_snapshot。
- 状态机有 hysteresis，避免抖动。
- 状态行为只影响通信建议和非实时通道控制，不替算法决策。
```
