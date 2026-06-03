# 01. 弱网优化目标模型

## 目标

弱网优化不是让所有消息可靠送达，而是让同步周期可判定、可控、可复盘。

### 主要目标

```text
1. 新鲜优先：当前周期数据优先于历史数据。
2. 周期正确：target_cycle 不匹配的数据不得进入本周期 Fresh 集合。
3. 迟到可见：late sample 必须进入 Late Pocket 并被统计，但不能被误用。
4. 不积压：realtime_sample 的队列年龄不得超过 1 个周期。
5. 可建议：中间件根据链路事实给出降频、增大 target_cycle_offset、关闭通道等建议。
6. 可定位：按 peer/channel 统计，能判断是链路、发送端、接收端、payload 还是拓扑问题。
7. 可追溯：所有 drop、late、state transition、stagger offset 都有 reason code。
```

### 非目标

```text
1. 不保证每个实时 sample 都送达。
2. 不做实时 sample 的 ACK 或重传闭环。
3. 不决定算法 fallback。
4. 不把业务 payload 固定为任何语义。
```

## 核心指标

| 指标 | 定义 | 目的 |
|---|---|---|
| `cycle_usability_ratio` | 可被算法使用的周期比例 | 总体验收指标 |
| `fresh_ratio` | 本周期按时收到且可用的 sample 比例 | 新鲜度 |
| `late_ratio` | 迟到但被识别的 sample 比例 | 弱网延迟观测 |
| `missing_ratio` | cutoff 时未收到的 sample 比例 | 丢失/拥塞观测 |
| `wrong_cycle_acceptance` | 错周期样本被当作 fresh 的次数 | 必须为 0 |
| `late_as_fresh` | late 样本被当作 fresh 的次数 | 必须为 0 |
| `max_queue_age_cycles` | 实时队列中最老样本年龄 | 必须 <= 1 |
| `rx_latency_p99` | 接收延迟 p99 | 推荐 offset 和频率 |
| `payload_size_p95` | payload 大小 p95 | 预算与 guard |
| `stagger_spread_observed` | 实际发送错峰分布 | 验证无线 burst 控制 |

## 验收底线

```text
wrong_cycle_acceptance == 0
late_as_fresh == 0
max_realtime_queue_age_cycles <= 1
state_transition_has_reason == true
all_metrics_per_peer_channel == true
```
