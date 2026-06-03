# Netem 弱网矩阵

| Scenario | delay | jitter | loss | reorder | rate | slot |
|---|---:|---:|---:|---:|---:|---|
| clean | 2ms | 1ms | 0 | 0 | none | none |
| mild_wifi | 10ms | 5ms | 0.5% | 0 | none | none |
| bad_wifi | 30ms | 20ms | 3% | 1% | none | none |
| burst_loss | 20ms | 10ms | gemodel | 0 | none | none |
| bandwidth_limited | 20ms | 10ms | 0 | 0 | 1mbit | none |
| partition_recover | down/up | - | 100% during partition | - | - | - |
| slot_burst | 5-20ms slot | - | 0 | 0 | optional | slot |

每个场景必须输出：

```text
fresh_ratio
late_ratio
missing_ratio
wrong_cycle_acceptance
late_as_fresh
queue_age_max
state_transitions
recommendations
stagger_distribution
```
