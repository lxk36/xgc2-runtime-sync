# 15. 参考来源

本开发包只引用底层工具的公开能力，不把其 API 细节写死到业务层。实现时请按实际锁定版本检查枚举名和 API 签名。

## Zenoh QoS

- Zenoh Rust API docs: `zenoh::qos` module provides QoS primitives including reliability, priority and congestion_control, set via publisher builder methods.
- URL: https://docs.rs/zenoh/latest/zenoh/qos/index.html

## Zenoh 概念与部署

- Zenoh 官方网站：pub/sub/query/store 等基础抽象。
- URL: https://zenoh.io/

## Linux tc netem

- Linux manual page: `tc-netem(8)` supports delay, loss, corrupt, duplication, reordering, rate and slot options.
- URL: https://man7.org/linux/man-pages/man8/tc-netem.8.html
