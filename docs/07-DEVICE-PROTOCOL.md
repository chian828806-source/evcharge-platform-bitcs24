# 远程重启与设备扩展规范

## 1. 定位

任务书明确要求管理端能够模拟发送远程重启指令。V1 必做范围是远程重启模拟，不要求完整设备网关、心跳、遥测或串口协议。

完整设备接入可作为 OPTIONAL 扩展，不能替代 Qt 用户端与 Qt/C++ PC 服务端之间的 Socket 主业务通信。

## 2. V1 必做范围

远程重启流程：

```text
管理员选择电桩
        ↓
点击远程重启
        ↓
PC 服务与管理端记录操作
        ↓
电桩状态变为 RESTARTING
        ↓
模拟处理完成
        ↓
返回 AVAILABLE 或原可用状态
```

## 3. 状态

```text
AVAILABLE
RESERVED
CHARGING
FAULT
OFFLINE
RESTARTING
```

远程重启只允许作用于存在的电桩。对于 `RESERVED` 或 `CHARGING` 状态电桩，V1 可拒绝重启并提示“电桩正在使用中”。

## 4. Socket 主业务消息

远程重启作为管理端主业务消息，定义在 `docs/03-API.md`：

```text
ADMIN_PILE_RESTART
```

示例：

```json
{
  "requestId": "REQ-RESTART-001",
  "type": "ADMIN_PILE_RESTART",
  "sessionId": "ADMIN-S-001",
  "payload": {
    "pileId": 1
  }
}
```

## 5. 日志

远程重启必须记录：

- 管理员；
- 电桩；
- 操作时间；
- 操作前状态；
- 操作后状态；
- 结果消息。

## 6. OPTIONAL 完整设备接入

如时间允许，可扩展：

- Device Simulator；
- TCP Socket 或 Serial；
- HELLO；
- HEARTBEAT；
- STATUS；
- TELEMETRY；
- RESTART；
- ACK。

扩展协议进入开发前必须单独评审，不得影响 V1 必做功能。
