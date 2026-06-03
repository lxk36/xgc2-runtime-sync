# 02. 通道等级与 Zenoh QoS 设计

## 通道分类

中间件必须把不同数据流拆成不同 `ChannelProfile`。不要让实时样本、健康、配置、日志共用同一策略。

| Profile | 内容 | 目标 | 默认策略 |
|---|---|---|---|
| `realtime_sample` | 周期算法样本 | 新鲜优先，允许丢 | best_effort + drop + high priority |
| `health` | clock/runtime/link 状态 | 可观测，低频 | best_effort + drop |
| `command_config` | session/config/start/stop | 一致性 | reliable + block，可选 readiness |
| `bulk_debug` | debug/log/map/image 等非实时数据 | 不能挤占实时链路 | background + rate limit + drop |

## Zenoh 原生 QoS 映射

Zenoh API 提供 QoS primitive，例如：

```text
reliability
priority
congestion_control
express
```

实际枚举名按所选 Zenoh binding 版本落地。配置层统一使用小写字符串，在 adapter 中映射为 binding-specific enum。

```yaml
channel_profiles:
  realtime_sample:
    zenoh:
      reliability: best_effort
      congestion_control: drop
      priority: real_time
      express: true
    runtime:
      max_queue_cycles: 1
      drop_policy: drop_oldest_before_send
      allow_ack: false

  health:
    zenoh:
      reliability: best_effort
      congestion_control: drop
      priority: data_low
      express: false
    runtime:
      max_hz: 2
      allow_ack: false

  command_config:
    zenoh:
      reliability: reliable
      congestion_control: block
      priority: interactive_high
      express: true
    runtime:
      realtime_path: false
      readiness_required: true

  bulk_debug:
    zenoh:
      reliability: best_effort
      congestion_control: drop
      priority: background
      express: false
    runtime:
      max_bandwidth_kbps: 50
      disable_when_state_at_least: degraded
```

## 设计原则

```text
实时数据：宁可丢，不要堵。
配置数据：可以慢，但必须一致。
健康数据：低频统计，不追求逐条必达。
调试数据：默认受限，弱网状态下首先关闭。
```

## Subagent 落点

```text
include/swarm_sync_core/weaknet/channel_profile.hpp
config/weaknet_policy.example.yaml
specs/channel_profiles.yaml
```

## 验收

```text
- 每个 channel 必须显式绑定 profile。
- realtime_sample 禁止 reliable+block。
- bulk_debug 在 DEGRADED 及以上状态自动进入限速或禁用建议。
- QoS 映射错误时启动失败，而不是静默使用默认值。
```
