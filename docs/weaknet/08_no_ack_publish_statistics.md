# 08. No ACK 实时发布与统计闭环

## 原则

实时样本是单向发布路径，不做逐条 ACK。

原因：

```text
1. ACK 会引入 O(N²) 控制流量。
2. ACK 会让弱网下的实时路径变成阻塞/重传路径。
3. 对同步周期算法，迟到的可靠送达通常没有业务价值。
4. 中间件需要的是统计证据，不是逐包确认。
```

## 替代机制

```text
seq gap 检测
Fresh/Late/Missing 统计
per peer/channel latency
Late Pocket
state machine
Ground station referee
```

发送端只发布：

```text
SampleEnvelope + payload
```

接收端统计：

```text
expected seq
received seq
seq gap
wrong cycle
late
missing
```

地面站通过低频 health/summary 汇总得到链路状态。

## 命令配置路径

本弱网包的 No ACK 约束主要针对 realtime sample。Session/config/start 的一致性可以使用以下非实时机制之一：

```text
1. readiness state publication；
2. 低频 config_version echo；
3. command_config profile 的可靠路径；
4. ground station 等待所有 required participants 进入 READY。
```

这些不进入周期实时数据路径。

## 验收

```text
- realtime_sample 不得生成 per-sample ACK。
- 统计能重建每个 peer/channel 的缺失与迟到情况。
- command/config 的 readiness 机制不会与 realtime path 共用队列。
```
