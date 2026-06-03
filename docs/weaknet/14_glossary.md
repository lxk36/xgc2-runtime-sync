# 14. 术语表

| 术语 | 含义 |
|---|---|
| `Cycle` | 全局同步周期，由 epoch 和 period 定义。 |
| `target_cycle` | 发送方希望该 sample 被哪个周期使用。 |
| `Fresh` | 在 cutoff 前收到、target_cycle 匹配、payload/clock/schema 合格。 |
| `Late` | target_cycle 匹配，但超过 cutoff 才到。 |
| `Late Pocket` | 保存迟到样本的旁路缓存，用于统计、查询、复盘。 |
| `WrongCycle` | sample 的 target_cycle 与当前周期不匹配。 |
| `ChannelProfile` | 一类通道的 QoS、队列、限速和弱网策略。 |
| `Traffic Budget` | 启动前根据节点数、频率、payload 大小、fanout 估算通信量。 |
| `Payload Guard` | 运行时检查 payload 大小、schema、encoding、CRC 的保护器。 |
| `LinkHealth` | per peer/channel 的链路健康统计。 |
| `Send Stagger` | 确定性微错峰，主动打散同步发送 burst。 |
| `Recommendation` | 中间件给出的频率、offset、通道裁剪等建议，不是算法决策。 |
| `Weaknet State` | GOOD / DEGRADED / CONGESTED / PARTITIONED / RECOVERING。 |
