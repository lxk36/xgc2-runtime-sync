# 05. Traffic Budget 与 Payload Guard

## 目的

弱网下最有效的优化是：少发、发小、发准。Traffic Budget 是启动前旁路功能，Payload Guard 是运行时保护功能。

## 启动前预算

对每个 realtime channel 估算：

```text
estimated_bps = publishers
              * effective_fanout
              * frequency_hz
              * (payload_bytes_p95 + envelope_overhead_bytes + transport_overhead_bytes)
              * safety_factor
```

全局预算：

```text
total_realtime_bps = sum(channel.estimated_bps)
```

配置示例：

```yaml
traffic_budget:
  max_realtime_air_bps: 2000000
  safety_factor: 1.35
  reject_if_exceeded: true
  warn_if_above_ratio: 0.75
```

## Payload Guard

中间件不理解 payload 内容，但必须管理工程属性：

```text
payload_size_bytes
schema_id
encoding
crc32c
fragmentation_risk
compression_flag
```

规则示例：

```yaml
payload_guard:
  realtime:
    warn_bytes: 800
    reject_bytes: 1200
    allow_compression: true
    compression_min_bytes: 512
  bulk_debug:
    max_bandwidth_kbps: 50
```

## 超预算行为

中间件只做启动/运行时保护和建议：

```text
启动前：reject / warn。
运行中：标记 oversized、drop 或按配置拒绝发布，同时给出 recommendation。
算法层：自行决定减小 payload、降频或改变业务策略。
```

## 验收

```text
- session 启动前产生 BudgetReport。
- 超预算时按照配置 warn 或 reject。
- realtime payload 超 reject_bytes 不得静默发送。
- 所有超预算事件必须带 channel、size、limit、reason。
```
