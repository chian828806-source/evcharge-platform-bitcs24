# 设备接入与协议规范

## 1. 定位

设备接入与物联网控制子系统用于模拟充电桩设备接入，支撑以下目标：

- 满足项目要求书中的 Socket 编程和多线程要求。
- 让远程重启不只是数据库状态修改，而是形成控制链路。
- 为管理端和 Web 大屏提供设备状态、心跳和充电数据来源。

本模块是 V1 的 SHOULD / P1 模块。必须服从核心业务闭环，不得影响用户充电、管理端、数据库、API 和 ML 的 MUST 需求交付。

## 2. 范围

V1 包含：

- Device Simulator
- Device Gateway
- TCP Socket 通信
- 设备上线
- 心跳
- 状态上报
- 充电数据上报
- 远程重启
- ACK 响应
- 离线判定

V1 暂不包含：

- OCPP
- MQTT
- Kafka
- 复杂设备证书体系
- 多网关集群
- Kubernetes 部署

## 3. 推荐运行架构

```text
Qt Admin
   ↓ REST
Spring Boot
   ↓ command
Device Gateway
   ↓ TCP Socket
Device Simulator
   ↑ status / heartbeat / telemetry
Device Gateway
   ↓ persist
MySQL
   ↓ query
Qt Admin / Web Dashboard
```

## 4. 传输协议

V1 优先使用 TCP Socket。

串口通信可作为扩展，不作为 V1 必须交付内容。

默认配置：

```text
host: 127.0.0.1
port: 19090
encoding: UTF-8
frame: one JSON object per line
```

每条消息以换行符 `\n` 结束。

## 5. 消息通用结构

```json
{
  "messageId": "MSG-20260831-000001",
  "deviceId": "PILE-001",
  "type": "HEARTBEAT",
  "timestamp": "2026-08-31 15:00:00",
  "payload": {}
}
```

字段说明：

| 字段 | 必填 | 说明 |
| --- | --- | --- |
| `messageId` | 是 | 消息唯一 ID，用于 ACK 和排查问题 |
| `deviceId` | 是 | 设备编号，对应电桩编号 |
| `type` | 是 | 消息类型 |
| `timestamp` | 是 | 设备发送时间 |
| `payload` | 是 | 消息数据 |

## 6. 消息类型

### 6.1 HELLO

设备上线注册。

```json
{
  "messageId": "MSG-001",
  "deviceId": "PILE-001",
  "type": "HELLO",
  "timestamp": "2026-08-31 15:00:00",
  "payload": {
    "stationId": 1,
    "pileNo": "PILE-001",
    "pileType": "FAST",
    "powerKw": 60.0
  }
}
```

### 6.2 HEARTBEAT

设备心跳。

```json
{
  "messageId": "MSG-002",
  "deviceId": "PILE-001",
  "type": "HEARTBEAT",
  "timestamp": "2026-08-31 15:00:10",
  "payload": {
    "status": "AVAILABLE"
  }
}
```

### 6.3 STATUS

设备状态上报。

```json
{
  "messageId": "MSG-003",
  "deviceId": "PILE-001",
  "type": "STATUS",
  "timestamp": "2026-08-31 15:00:20",
  "payload": {
    "status": "CHARGING"
  }
}
```

### 6.4 TELEMETRY

充电过程数据上报。

```json
{
  "messageId": "MSG-004",
  "deviceId": "PILE-001",
  "type": "TELEMETRY",
  "timestamp": "2026-08-31 15:01:00",
  "payload": {
    "orderId": 1001,
    "status": "CHARGING",
    "powerKw": 60.2,
    "energyKwh": 12.6
  }
}
```

### 6.5 START

后端或网关发送开始充电指令。

```json
{
  "messageId": "CMD-001",
  "deviceId": "PILE-001",
  "type": "START",
  "timestamp": "2026-08-31 15:01:00",
  "payload": {
    "orderId": 1001
  }
}
```

### 6.6 STOP

后端或网关发送停止充电指令。

```json
{
  "messageId": "CMD-002",
  "deviceId": "PILE-001",
  "type": "STOP",
  "timestamp": "2026-08-31 15:20:00",
  "payload": {
    "orderId": 1001
  }
}
```

### 6.7 RESTART

后端或网关发送远程重启指令。

```json
{
  "messageId": "CMD-003",
  "deviceId": "PILE-001",
  "type": "RESTART",
  "timestamp": "2026-08-31 15:30:00",
  "payload": {
    "reason": "admin_request"
  }
}
```

### 6.8 ACK

设备或网关对命令进行确认。

```json
{
  "messageId": "ACK-003",
  "deviceId": "PILE-001",
  "type": "ACK",
  "timestamp": "2026-08-31 15:30:01",
  "payload": {
    "ackMessageId": "CMD-003",
    "success": true,
    "message": "restart accepted"
  }
}
```

## 7. 状态枚举

设备侧电桩状态统一使用：

```text
AVAILABLE
CHARGING
FAULT
OFFLINE
RESTARTING
```

设备连接状态统一使用：

```text
ONLINE
OFFLINE
FAULT
RESTARTING
```

不得在协议中混用中文状态、数字状态和未定义字符串。

## 8. 心跳与离线规则

默认心跳间隔：

```text
10 seconds
```

默认离线判定：

```text
超过 30 seconds 未收到 HEARTBEAT，则标记 OFFLINE
```

离线后：

- 管理端显示设备离线。
- Web 大屏状态统计同步变化。
- 用户端不能选择该电桩开始新订单。

## 9. 超时与重试

命令发送后默认等待 ACK：

```text
5 seconds
```

超时处理：

- 标记命令失败。
- 写入设备日志。
- 返回管理端明确提示。

V1 不强制自动重试；如实现重试，最多重试 2 次。

## 10. 多线程要求

Device Gateway 至少应避免阻塞主流程。

推荐线程结构：

```text
Main Thread
├── Socket Accept / Connect Worker
├── Message Receive Worker
├── Heartbeat Monitor
└── Command Dispatcher
```

Qt 管理端发起重启时，不得阻塞 UI 线程。

## 11. 日志要求

以下事件必须记录：

- 设备上线
- 设备离线
- 心跳超时
- 状态变化
- 远程重启请求
- 命令 ACK
- 命令超时
- 设备故障

日志可写入：

- `device_log`
- 应用日志文件

正式数据库设计阶段再冻结字段。

## 12. 与业务系统关系

设备协议只负责模拟充电桩与设备接入层之间的设备侧通信，不替代 Qt Socket 业务协议或 Web / ML REST API。

Qt 用户端和 Qt 管理端通过 TCP Socket 业务协议完成：

- 用户登录
- 站点查询
- 订单创建
- 充电开始和停止
- 钱包结算
- 管理端查询

Web 大屏和 Python ML 模块通过 REST API 完成数据查询和预测结果交互。

设备通信负责：

- 状态上报
- 心跳
- 充电遥测
- 远程控制指令

## 13. 验收标准

V1 设备模块完成标准：

1. 能启动 Device Gateway。
2. 能启动至少 1 个 Device Simulator。
3. Simulator 能通过 TCP Socket 发送 `HELLO` 和 `HEARTBEAT`。
4. Gateway 能识别设备在线和离线。
5. 管理端远程重启能触发 `RESTART` 指令。
6. Simulator 返回 `ACK`。
7. 设备状态能进入数据库或通过后端接口被管理端读取。
