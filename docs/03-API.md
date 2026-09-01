# Socket 消息与数据交换规范

本文档定义 Qt 用户端与 Qt/C++ PC 服务端之间的 Socket 应用层协议、Web 大屏 WebSocket 消息，以及 ML 的数据交换方式。

## 1. 基本原则

1. 主业务通信使用 TCP Socket。
2. 用户端不得绕过 PC 服务端直接访问 SQLite。
3. Web 大屏通过 WebSocket 连接 PC 服务端，不直接读取数据库。
4. ML 可读取 SQLite、CSV 或 JSON，输出结果写回 SQLite 或导出 JSON。
5. 消息类型、字段、错误码属于公共契约。

## 2. Socket 封装

用户端统一封装：

```text
SocketClient / NetworkClient
```

服务端统一封装：

```text
SocketServer / ClientSession
```

业务页面不得直接操作 `QTcpSocket`。

## 3. 消息帧格式

V1 推荐 JSON Lines：

```text
一条 JSON 消息 + \n
```

如果出现粘包、拆包处理困难，可改为长度前缀：

```text
uint32 length + JSON body
```

格式一旦进入联调不得私自修改。

## 4. 请求结构

```json
{
  "requestId": "REQ-20260901-000001",
  "type": "USER_LOGIN",
  "sessionId": null,
  "payload": {
    "phone": "13800138000"
  }
}
```

## 5. 响应结构

```json
{
  "requestId": "REQ-20260901-000001",
  "code": 200,
  "message": "success",
  "data": {}
}
```

## 6. 核心消息类型

用户：

```text
USER_LOGIN
USER_PROFILE_GET
USER_PROFILE_UPDATE
USER_AVATAR_UPLOAD
USER_RECHARGE
USER_ORDER_LIST
```

站点与导航：

```text
STATION_LIST_NEARBY
STATION_DETAIL_GET
MAP_GEOCODE
```

充电订单：

```text
ORDER_ACTIVE_CHECK
ORDER_CREATE
ORDER_CANCEL
ORDER_START
ORDER_STOP
ORDER_SETTLE
```

管理：

```text
ADMIN_LOGIN
ADMIN_REVENUE_SUMMARY
ADMIN_REVENUE_TREND
ADMIN_PILE_STATUS_SUMMARY
ADMIN_PILE_LIST
ADMIN_PILE_RESTART
ADMIN_STATION_LIST
ADMIN_STATION_CREATE
ADMIN_USER_LIST
ADMIN_USER_FREEZE
ADMIN_USER_UNFREEZE
```

ML 展示：

```text
PREDICTION_LIST
PREDICTION_RECOMMENDATION
PREDICTION_WARNING
```

## 7. 认证

登录成功后返回随机 `sessionId`。受保护消息必须携带 `sessionId`。

管理员和普通用户 session 必须区分权限。

## 8. 错误码

| code | 含义 |
| --- | --- |
| 200 | 成功 |
| 4001 | 手机号格式非法 |
| 4002 | 用户已被冻结 |
| 4003 | 未登录或 session 无效 |
| 4101 | 用户存在未完成订单 |
| 4102 | 电桩不可用 |
| 4103 | 钱包余额不足 |
| 4104 | 非法订单状态 |
| 4105 | 订单不可取消 |
| 4201 | 站点不存在 |
| 4202 | 电桩不存在 |
| 4301 | 管理员账号或密码错误 |
| 4401 | Socket 消息格式错误 |
| 4402 | Socket 请求超时 |
| 4501 | 预测结果不存在 |
| 5001 | 数据库错误 |
| 5002 | 系统内部错误 |

## 9. 示例：用户登录

Request:

```json
{
  "requestId": "REQ-001",
  "type": "USER_LOGIN",
  "sessionId": null,
  "payload": {
    "phone": "13800138000"
  }
}
```

Response:

```json
{
  "requestId": "REQ-001",
  "code": 200,
  "message": "success",
  "data": {
    "sessionId": "S-8f4a9c",
    "user": {
      "userId": 1,
      "nickname": "用户8000",
      "balanceFen": 0
    }
  }
}
```

## 10. 示例：创建订单

```json
{
  "requestId": "REQ-002",
  "type": "ORDER_CREATE",
  "sessionId": "S-8f4a9c",
  "payload": {
    "pileId": 1
  }
}
```

## 11. 示例：结算订单

```json
{
  "requestId": "REQ-003",
  "type": "ORDER_SETTLE",
  "sessionId": "S-8f4a9c",
  "payload": {
    "orderId": 1001
  }
}
```

## 12. 示例：取消未开始订单

```json
{
  "requestId": "REQ-004",
  "type": "ORDER_CANCEL",
  "sessionId": "S-8f4a9c",
  "payload": {
    "orderId": 1001
  }
}
```

取消仅允许作用于 `CREATED` 状态订单。取消成功后订单进入 `CANCELLED`，对应电桩由 `RESERVED` 恢复为 `AVAILABLE`。

## 13. Web 大屏 WebSocket

连接地址：

```text
ws://<server-host>:<port>/dashboard
```

大屏请求：

```json
{
  "requestId": "DASH-001",
  "type": "DASHBOARD_SUBSCRIBE",
  "payload": {
    "topics": ["summary", "pileStatus", "revenueTrend", "prediction"]
  }
}
```

服务端推送：

```json
{
  "type": "DASHBOARD_UPDATE",
  "topic": "summary",
  "data": {
    "todayEnergyKwh": 128.5,
    "todayRevenueFen": 93600,
    "stationLoad": 0.62
  }
}
```

`stationLoad` 使用 SRS 中统一负荷口径，取值范围为 0 到 1。

## 14. ML 数据交换

输入：

```text
ml/data/history.csv
ml/data/orders.csv
```

输出：

```text
ml/output/predictions.json
```

PC 服务端负责将预测结果导入 SQLite，并通过 WebSocket 提供给 Web 大屏。

## 15. 接口修改流程

一旦 Socket 或 WebSocket 消息进入联调，任何人不得直接修改：

- Message Type；
- 请求字段；
- 响应字段；
- 推送字段；
- 错误码；
- 认证规则。

确需修改时，先改本文档，再改代码并通知相关成员。
