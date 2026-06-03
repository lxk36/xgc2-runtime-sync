# 04. 自适应建议引擎

## 关键边界

中间件只负责给建议，不主动替算法降级。

```text
允许：建议 frequency_hz 从 50 降到 20。
允许：建议 target_cycle_offset 从 1 增加到 2。
允许：建议关闭 bulk_debug 或降低 payload 大小。
禁止：直接修改算法 fallback、控制策略、预测策略。
禁止：未经过 coordinator/session config 就修改全局周期。
```

## 输入指标

```text
rx_latency_p95/p99
fresh_ratio
late_ratio
missing_ratio
payload_size_p95
queue_age_cycles
clock_uncertainty_ns
publish_deadline_miss_ratio
state_machine_state
```

## 推荐 target_cycle_offset

推荐公式：

```text
required_time = producer_compute_p99
              + network_latency_p99
              + receiver_jitter_p99
              + clock_uncertainty
              + safety_margin

recommended_target_cycle_offset = ceil(required_time / period)
```

结果必须被限制在配置范围：

```yaml
adaptive_recommendation:
  target_cycle_offset:
    min: 1
    max: 4
```

## 推荐频率

如果持续窗口内 Fresh 率不足：

```yaml
if:
  fresh_ratio_below: 0.95
  window_cycles: 100
recommend:
  lower_frequency_one_step: true
```

如果稳定恢复：

```yaml
if:
  fresh_ratio_above: 0.995
  window_cycles: 500
recommend:
  higher_frequency_one_step: true
```

## 输出结构

```cpp
struct RuntimeRecommendation {
  uint64_t cycle_id;
  RecommendationLevel level;  // INFO / WARN / CRITICAL
  RecommendationType type;    // ChangeFrequency / ChangeOffset / DisableChannel / ReducePayload / ChangeTopology
  std::string channel;
  std::string peer;
  double confidence;
  std::string reason;
  std::map<std::string, double> evidence;
};
```

## 验收

```text
- 建议必须包含 reason 和 evidence。
- 建议不得直接修改算法状态。
- 建议切换存在 hysteresis，避免 GOOD/DEGRADED 之间抖动。
- Ground station 能显示所有建议及其触发指标。
```
