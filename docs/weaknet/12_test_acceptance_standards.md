# 12. 测试与验收标准

## 测试层级

```text
Unit Test
  ChannelProfile 映射、CycleWindow 判定、Late Pocket、BudgetCalc、Stagger。

Integration Test
  多进程本机 Zenoh、模拟多个 UAV、周期同步、统计聚合。

Netem Test
  delay/loss/jitter/reorder/rate/slot 弱网矩阵。

Ground Station Test
  dashboard、state transition、recommendation、report。

ROS1 Adapter Test
  topic import/export、msg 发布、snapshot 一致性。
```

## 必须通过的验收项

### A. 周期正确性

```text
A1. 同一 epoch/period 下，多节点 cycle_id 一致。
A2. target_cycle 不等于当前 cycle 的 sample 不得进入 Fresh。
A3. 迟到 sample 必须进入 Late Pocket。
A4. late_as_fresh == 0。
A5. wrong_cycle_acceptance == 0。
```

### B. 队列与实时性

```text
B1. realtime_sample max_queue_age_cycles <= 1。
B2. 弱网下旧样本被 drop_oldest，而不是阻塞当前周期。
B3. realtime_sample 不产生 per-sample ACK。
```

### C. Zenoh QoS 与通道隔离

```text
C1. 每个 channel 显式绑定 profile。
C2. realtime_sample 使用 best_effort/drop 类策略。
C3. bulk_debug 在 DEGRADED/CONGESTED 时被限速或给出关闭建议。
C4. command_config 与 realtime path 队列隔离。
```

### D. 错峰

```text
D1. 开启 deterministic_micro_slot 后，多节点发送分布被打散。
D2. offset 可复现。
D3. offset 被记录到统计和 envelope metadata。
D4. 错峰不改变 target_cycle。
```

### E. 预算与 Payload Guard

```text
E1. session 启动前生成 BudgetReport。
E2. 超预算时按配置 warn 或 reject。
E3. oversized realtime payload 不得静默发送。
E4. Payload Guard 不检查业务语义，只检查大小、schema、encoding、CRC。
```

### F. LinkHealth 与状态机

```text
F1. per peer/channel fresh/late/missing/latency/seq_gap 可用。
F2. GOOD/DEGRADED/CONGESTED/PARTITIONED/RECOVERING 可按阈值迁移。
F3. 每次迁移有 reason 和 evidence。
F4. 状态机只给通信建议，不触发算法 fallback。
```

### G. 时钟 gate 与飞行阶段

```text
G1. 起飞前所有节点必须选中地面站 chrony source。
G2. 默认 offset <= 2 ms 且 uncertainty <= 2 ms 才允许 start。
G3. leap status 非 Normal 时拒绝 start。
G4. in_flight phase 不执行 step，只发布 degraded/bad 诊断。
G5. 同步事件使用未来 T_exec，不在事件前临时重新校时。
G6. 控制周期 dt 使用 monotonic/steady clock，跨机时间戳使用同步 system/ROS time。
```

### H. Netem 弱网矩阵

最低矩阵：

```text
clean
mild_wifi
bad_wifi
burst_loss
bandwidth_limited
partition_recover
slot_burst
```

每个场景输出：

```text
fresh_ratio
late_ratio
missing_ratio
rx_latency_p50/p95/p99
queue_age_max
state transitions
recommendations
budget utilization
stagger distribution
```

## 最终验收报告

最终 CI/人工验收应生成：

```text
reports/{session_id}/
  acceptance_summary.md
  metrics.csv
  state_transitions.csv
  link_health_by_peer_channel.csv
  recommendations.jsonl
  stagger_distribution.csv
  budget_report.yaml
```
