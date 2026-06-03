# 09. 拓扑 Profile 与 Query Side Path

## 拓扑 Profile

### 1. Ground Star

```text
UAV client -> ground zenohd router
```

适用于实验室、地面站 AP、中小规模实验。优点是监控和记录简单，缺点是所有数据可能经过地面站/AP。

### 2. Mesh / Multi-router

```text
UAV group router -> local peers
regional router -> ground gateway
```

适用于多跳、地面站链路不稳定、局部集群自治。

### 3. Ground-loss Degraded

地面站断开后，飞机本地周期不应立即停止。行为由 session policy 控制：

```yaml
ground_station_loss:
  continue_session_if_clock_ok: true
  freeze_config_version: true
  disable_bulk_debug: true
  max_ground_loss_s: 10
```

## Query Side Path

Query 只用于慢速旁路：

```text
- 查询 last health；
- 查询 config version；
- 查询 late pocket 摘要；
- 查询历史 sample 元信息；
- 调试和恢复。
```

禁止：

```text
cycle k 开始没收到 sample，于是阻塞 query 补拉后再给算法。
```

实时周期只用 pub/sub + cutoff 判定。

## 验收

```text
- 每个 profile 有显式 Zenoh endpoint 和 keyspace 配置。
- query 不能阻塞 CycleSnapshot 生成。
- 地面站断开测试中，本地 cycle 仍可继续产生，状态进入 PARTITIONED 或 DEGRADED。
```
