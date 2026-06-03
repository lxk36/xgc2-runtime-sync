# Acceptance Checklist

## P0

- [ ] realtime_sample 无 ACK。
- [ ] Late sample 不进入 Fresh。
- [ ] WrongCycle sample 不进入 Fresh。
- [ ] realtime queue age <= 1 cycle。
- [ ] CycleSnapshot 包含每 peer/channel 状态。

## P1

- [ ] LinkHealth 按 peer/channel 输出。
- [ ] WeaknetStateMachine 可迁移且有 reason/evidence。
- [ ] Ground Station 可显示 per-edge 指标。

## P2

- [ ] TrafficBudget 启动前报告。
- [ ] PayloadGuard 保护 oversized realtime payload。
- [ ] SendStagger offset 可复现，可记录。

## P3

- [ ] Recommendation 只输出建议，不主动降级。
- [ ] Query Side Path 不阻塞实时周期。
- [ ] Topology Profile 可切换。

## P4

- [ ] ROS1 package 可编译。
- [ ] launch 可启动 runtime 和 ground referee。
- [ ] netem 矩阵生成验收报告。
