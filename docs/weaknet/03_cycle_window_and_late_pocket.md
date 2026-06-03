# 03. Receive Cutoff 与 Late Pocket

## 为什么需要 Receive Cutoff

同步周期场景中，接收端不能只问“消息到了吗”，必须问：

```text
消息是否在本周期判定窗口之前到达？
消息 target_cycle 是否等于当前 cycle？
消息是否来自时钟可信的发送方？
```

因此每个 channel 需要定义：

```text
receive_cutoff_ns
late_record_window_ns
ttl_ns
```

## 周期窗口定义

对于周期 `k`：

```text
T_k = epoch + k * period
cutoff_k = T_k + receive_cutoff_offset
late_end_k = cutoff_k + late_record_window
```

判定：

```text
arrive_time <= cutoff_k 且 target_cycle == k  -> Fresh
arrive_time > cutoff_k 且 arrive_time <= late_end_k 且 target_cycle == k -> Late
arrive_time > late_end_k -> ExpiredLate / Dropped
目标周期 != k -> WrongCycle
```

## Late Pocket

Late Pocket 是一个旁路缓存，保存迟到样本的元信息和 payload 引用。它的目的不是补进当前周期，而是：

```text
1. 统计 late ratio；
2. 供算法或调试工具查询历史；
3. 判断是否需要建议增加 target_cycle_offset；
4. 判断某条链路是迟到还是缺失；
5. 为地面站裁判提供证据。
```

## 禁止行为

```text
- 禁止把 Late sample 自动改标为 Fresh。
- 禁止在周期开始后等待 Late Pocket 补齐。
- 禁止把上一周期 Late sample 偷偷塞进当前周期输入。
```

## 状态枚举

```cpp
enum class SampleStatus {
  Fresh,
  Missing,
  Late,
  ExpiredLate,
  WrongCycle,
  Duplicate,
  BadPayload,
  BadSchema,
  SenderClockBad,
  ProducerReportedFail,
  TransportError
};
```

## 验收

```text
- 构造延迟超过 cutoff 的样本，必须进入 Late，不得进入 Fresh。
- 构造 target_cycle 错误样本，必须进入 WrongCycle。
- Late Pocket 可按 peer/channel/cycle 查询。
- late_as_fresh 指标必须恒为 0。
```
