# 06. Per Peer/Channel LinkHealth

## 问题

只知道 `uav03 online` 不够。中间件必须知道：

```text
uav01 -> uav03 的 solution channel 是否健康？
uav02 -> uav03 的 health channel 是否健康？
是全局网络差，还是某条边差，还是某个 channel 过大？
```

## 指标结构

```cpp
struct PeerChannelHealth {
  std::string peer_id;
  std::string channel;

  double fresh_ratio;
  double late_ratio;
  double missing_ratio;
  double duplicate_ratio;
  double wrong_cycle_ratio;
  double bad_payload_ratio;

  int64_t rx_latency_p50_ns;
  int64_t rx_latency_p95_ns;
  int64_t rx_latency_p99_ns;

  uint64_t seq_gap_count;
  uint64_t late_pocket_count;
  uint64_t dropped_oversized_count;

  LinkState state;  // GOOD / DEGRADED / CONGESTED / PARTITIONED / RECOVERING
  std::string primary_reason;
};
```

## 统计窗口

建议同时维护：

```text
short_window: 20-100 cycles，用于快速告警。
long_window: 500-2000 cycles，用于稳定建议。
```

## 聚合层级

```text
peer/channel -> peer aggregate -> channel aggregate -> node aggregate -> global session
```

## 验收

```text
- 所有实时 channel 都有 peer/channel 级统计。
- Fresh/Late/Missing 三者可解释，不能只给丢包率。
- latency p50/p95/p99 可导出。
- Ground station 能按边显示状态。
```
