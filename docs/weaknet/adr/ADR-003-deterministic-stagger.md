# ADR-003: Deterministic Send Stagger

## Decision

中间件提供确定性微错峰，主动打散周期边界无线 burst。

## Rationale

不能依赖计算时间不稳定带来的被动错峰。主动错峰必须可复现、可记录、可追溯。

## Consequence

错峰 offset 计入 publish deadline，并记录在 metrics/envelope metadata 中。
