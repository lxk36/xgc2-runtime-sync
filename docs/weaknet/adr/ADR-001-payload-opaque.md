# ADR-001: Payload Opaque

## Decision

中间件不理解 payload 业务语义。payload 可以是轨迹、控制量、局部地图、梯度、约束、代价函数参数或其他任意数据。

## Consequence

中间件只能检查：

```text
schema_id
encoding
payload_size
crc
cycle/deadline metadata
```

算法负责解释 payload 和 fallback 策略。
