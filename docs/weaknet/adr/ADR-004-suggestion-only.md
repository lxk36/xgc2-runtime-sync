# ADR-004: Recommendation Only

## Decision

中间件可以建议降频、增大 target_cycle_offset、关闭通道或减小 payload，但不主动修改算法模式。

## Consequence

coordinator 或算法层需要显式接受建议并发布新的 session config。
