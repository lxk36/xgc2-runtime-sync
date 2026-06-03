# ADR-002: No ACK for Realtime Samples

## Decision

实时周期 sample 不做逐条 ACK。

## Rationale

ACK 会造成额外 O(N²) 控制流量，并可能在弱网下把实时路径变成阻塞路径。

## Consequence

链路质量由 seq gap、Fresh/Late/Missing、latency、Late Pocket 和地面站统计来证明。
