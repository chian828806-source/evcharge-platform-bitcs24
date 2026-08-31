# API 规范

本文档定义 Web / ML REST API 设计，以及 Qt Socket 业务消息与 REST API 共用的认证、错误码、分页、幂等和变更规则。具体接口清单将在 SRS 冻结后继续细化。

## 1. 基本原则

1. Linux Qt 用户端和 Qt 管理端通过 TCP Socket 访问核心业务数据。
2. Web 大屏和 Python ML 模块通过 REST API 访问后端数据。
3. 设备通信不放入本业务接口规范，独立见 `docs/07-DEVICE-PROTOCOL.md`。
4. Socket 业务消息和 REST API 都属于公共契约，一旦进入联调不得私自修改。
5. 所有 Socket 业务消息或 REST API 必须能追踪到 SRS 需求编号。

## 2. Qt Socket 业务消息规范

Qt 业务客户端不得直接在页面中操作 `QTcpSocket`，必须统一封装 `SocketClient` / `NetworkClient`。

V1 推荐采用 JSON Lines 格式，一条 JSON 消息以 `\n` 结束。

请求结构建议：

```json
{
  "requestId": "REQ-20260831-000001",
  "type": "USER_LOGIN",
  "token": null,
  "payload": {
    "phone": "13800138000"
  }
}
```

响应结构建议：

```json
{
  "requestId": "REQ-20260831-000001",
  "code": 200,
  "message": "success",
  "data": {}
}
```

Socket 消息类型应按业务域命名，例如：

```text
USER_LOGIN
USER_PROFILE_GET
USER_RECHARGE
STATION_LIST_NEARBY
STATION_DETAIL_GET
ORDER_CREATE
ORDER_START
ORDER_STOP
ORDER_SETTLE
ADMIN_LOGIN
ADMIN_REVENUE_SUMMARY
ADMIN_USER_FREEZE
CHARGER_RESTART
```

## 3. REST 路径规范

统一前缀：

```text
/api
```

推荐资源路径：

```text
/api/auth
/api/users
/api/stations
/api/chargers
/api/orders
/api/admin
/api/statistics
/api/predictions
/api/devices
/api/files
```

## 4. REST 命名规范

查询列表：

```http
GET /api/stations
```

查询单个：

```http
GET /api/stations/{id}
```

新增：

```http
POST /api/stations
```

修改：

```http
PUT /api/stations/{id}
```

删除：

```http
DELETE /api/stations/{id}
```

业务动作可以使用：

```http
POST /api/users/{id}/recharge
POST /api/orders/{id}/start
POST /api/orders/{id}/stop
POST /api/orders/{id}/settle
POST /api/chargers/{id}/restart
POST /api/users/{id}/freeze
POST /api/users/{id}/unfreeze
```

禁止：

```text
/getStation
/getAllStation
/addStation
/doDeleteStation
/changeStationInformation
```

## 5. JSON 命名

Socket 业务消息和 REST API 的 JSON 字段统一使用 camelCase。

示例：

```json
{
  "stationId": 1,
  "stationName": "中关村充电站",
  "availableCount": 6,
  "totalCount": 10,
  "pricePerKwh": 1.5
}
```

禁止混用：

```text
station_id
stationId
StationID
stationID
```

## 6. 统一返回结构

Socket 响应和 REST API 响应统一使用以下语义结构：

```json
{
  "code": 200,
  "message": "success",
  "data": {}
}
```

成功示例：

```json
{
  "code": 200,
  "message": "success",
  "data": {
    "userId": 1,
    "nickname": "用户1234"
  }
}
```

失败示例：

```json
{
  "code": 4001,
  "message": "用户存在未完成订单",
  "data": null
}
```

后端统一实现：

```java
Result<T>
```

## 7. 认证与授权

V1 推荐轻量 JWT。

角色至少包括：

```text
USER
ADMIN
```

Qt 用户端 Socket 登录消息：

```text
USER_LOGIN
```

Qt 管理端 Socket 登录消息：

```text
ADMIN_LOGIN
```

REST 登录接口仅在 Web 或调试需要时保留：

```http
POST /api/auth/user-login
POST /api/auth/admin-login
```

需要登录的 REST API 应携带：

```http
Authorization: Bearer <token>
```

安全要求：

- 管理员密码不得明文存储。
- 冻结用户不得登录或执行业务操作。
- 管理端 Socket 消息和管理类 REST API 必须校验 ADMIN 权限。

## 8. 错误码建议

| 范围 | 含义 |
| --- | --- |
| 200 | 成功 |
| 4000-4099 | 用户与认证错误 |
| 4100-4199 | 订单与充电业务错误 |
| 4200-4299 | 站点和电桩错误 |
| 4300-4399 | 管理端错误 |
| 4400-4499 | 设备通信错误 |
| 4500-4599 | ML 和预测错误 |
| 5000-5999 | 系统错误 |

示例：

```text
4001 手机号格式非法
4002 用户已被冻结
4101 用户存在未完成订单
4102 电桩不可用
4103 钱包余额不足
4401 设备离线
4402 设备命令超时
4501 预测结果不存在
```

## 9. 分页规范

列表接口如用户、电桩、订单应支持分页。

请求参数：

```text
page
pageSize
keyword
```

响应建议：

```json
{
  "code": 200,
  "message": "success",
  "data": {
    "records": [],
    "total": 100,
    "page": 1,
    "pageSize": 10
  }
}
```

## 10. 幂等与并发

必须重点保护：

- 用户自动注册
- 创建充电订单
- 开始充电
- 停止充电
- 钱包结算
- 远程重启命令

规则：

- 手机号必须唯一。
- 用户同一时刻最多 1 个活动订单。
- 充电桩同一时刻最多服务 1 个订单。
- 结算接口重复调用不得重复扣款。

## 11. 文件上传接口

头像上传建议：

```http
POST /api/files/avatar
```

要求：

- 限制图片类型。
- 限制文件大小。
- 返回可访问的 `avatarUrl`。
- 禁止任意路径写入。

## 12. 地图接口

推荐由后端代理腾讯地图地理编码：

```http
GET /api/map/geocode?address=...
```

Qt 导航页面可使用 `QWebEngineView` 加载腾讯地图路线页面。

## 13. 接口文档格式

每个 Socket 业务消息或 REST API 至少描述：

```text
需求编号
功能
Message Type 或 URL
Transport
Method，REST API 需要
权限
请求参数
响应参数
成功示例
失败情况
```

示例：

```text
Requirement:
FR-U-001

功能：
手机号登录/自动注册

Transport:
TCP Socket

Message Type:
USER_LOGIN

Request:
{
  "phone": "13800138000"
}

Response:
{
  "code": 200,
  "message": "success",
  "data": {
    "token": "...",
    "user": {
      "userId": 1,
      "nickname": "用户8000",
      "balance": 0.00
    }
  }
}
```

SpringDoc Swagger 用于 REST API 实时接口测试。

`docs/03-API.md` 用于团队确定 Qt Socket 业务消息和 REST API 的正式接口协议。

## 14. 接口修改流程

一旦 Socket 消息或 REST API 进入联调阶段，任何人不得直接修改：

```text
Message Type
URL
HTTP Method
请求字段
响应字段
状态含义
错误码
权限规则
```

确需修改时：

```text
提出修改
↓
确认影响模块
↓
组长确认
↓
修改接口文档
↓
修改代码
↓
通知相关成员
```

必须做到：

```text
先改文档，再改代码。
```
