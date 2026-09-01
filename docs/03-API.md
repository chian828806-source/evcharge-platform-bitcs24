# Socket 消息与数据交换规范

本文档定义 Qt 用户端、Qt 管理端与 Qt/C++ 服务端之间的 Socket 应用层协议、Web 大屏 WebSocket 消息，以及 ML 的数据交换方式。

## 1. 基本原则

1. 主业务通信使用 TCP Socket。
2. Qt 用户端和 Qt 管理端不得绕过 Qt/C++ 服务端直接访问 SQLite。
3. Web 大屏通过 WebSocket 连接 Qt/C++ 服务端，不直接读取数据库。
4. ML 不得直接访问 SQLite。Qt/C++ 服务端导出 CSV/JSON 训练数据，ML 输出预测 JSON，由服务端校验并导入 SQLite。
5. 消息类型、字段、错误码属于公共契约。

## 2. Socket 封装

用户端统一封装：

```text
SocketClient / NetworkClient
```

Qt/C++ 服务端统一封装：

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

消息方向约定：

- `USER_*`、`STATION_*`、`ORDER_*`、`PREDICTION_*` 主要由 Qt 用户端发送，Qt/C++ 服务端处理；
- `ADMIN_*` 由 Qt 管理端发送，Qt/C++ 服务端处理；
- `DASHBOARD_*` 由 Web 大屏通过 WebSocket 发送或接收。

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

ML 数据交换是服务端与 Python 进程之间的受控文件契约，不属于 Qt 用户端或 Qt 管理端可调用的 Socket 消息。ML 进程不得获得 SQLite 文件路径、数据库连接或写库权限。

### 14.1 服务端导出

Qt/C++ 服务端在定时任务、管理员触发或演示流程中生成一个带 `batchId` 的训练数据批次：

```text
ml/data/<batchId>/history.csv
ml/data/<batchId>/orders.csv
```

导出字段、来源和统计口径以 `docs/04-DATABASE.md` 第 9.5 节为准；服务端只导出模型所需的字段，不导出用户手机号、昵称、头像、余额等个人信息。

### 14.2 ML 输出

```text
ml/output/<batchId>/predictions.json
```

输出 JSON 必须采用以下结构：

```json
{
  "schemaVersion": "1.0",
  "batchId": "20260901T120000Z-demo",
  "predictions": [
    {
      "stationId": 1,
      "predictionTime": "2026-09-01T13:00:00+08:00",
      "horizon": "1h",
      "predictedLoad": 0.65,
      "predictedAvailableCount": 3,
      "peakLevel": "HIGH",
      "modelName": "baseline-v1"
    }
  ]
}
```

`horizon` 只能是 `1h`、`6h` 或 `24h`；`predictedLoad` 必须是 0 到 1 的有限数值；`predictedAvailableCount` 必须为非负整数；`peakLevel` 只能是 `LOW`、`MEDIUM` 或 `HIGH`。

### 14.3 服务端导入

Qt/C++ 服务端读取 ML 输出后，必须校验 `schemaVersion`、`batchId`、必填字段、站点存在性和上述范围约束。一个批次中任一记录不合法时，服务端拒绝整个批次且不得写入 `prediction` 表；全部合法时，以一个数据库事务写入。导入成功后，服务端向用户端、管理端和 WebSocket 大屏提供最新预测结果。

## 15. 接口修改流程

一旦 Socket 或 WebSocket 消息进入联调，任何人不得直接修改：

- Message Type；
- 请求字段；
- 响应字段；
- 推送字段；
- 错误码；
- 认证规则。

确需修改时，先改本文档，再改代码并通知相关成员。

## 16. 通信层代码边界

通信层负责“消息如何进入和出去”，不负责“业务是否允许执行”。

~~~text
Qt用户端 / Qt管理端页面
  → SocketClient
  → TCP JSON Lines
  → SocketServer / ClientSession
  → MessageDispatcher
  → Handler
  → Service
  → Repository / SQLite
~~~

各层职责：

| 层 | 负责 | 禁止 |
| --- | --- | --- |
| SocketClient | 连接、发送请求、接收响应、断线提示 | 直接操作数据库或执行业务规则 |
| JsonLineCodec | 半包、粘包、按换行分帧 | 理解订单、用户等业务 |
| ClientSession | 每连接缓冲、JSON解析、统一响应 | 写复杂业务和SQL |
| SessionManager | Session生成、查询和角色区分 | 校验密码、冻结状态 |
| MessageDispatcher | type路由、公共鉴权、调用Handler | 决定订单状态转换 |
| Handler | 校验本消息payload、调用Service、映射响应 | 直接拼接SQL |
| Service | 服务端业务规则、状态变化和事务意图 | 操作Socket或依赖管理端UI |
| Repository | 参数化SQL、事务和对象映射 | 拼装网络JSON |

当前代码目录：

~~~text
shared/protocol/                 公共消息、错误码、JSON Lines
qt-user/network/                 Qt用户端SocketClient
qt-admin/network/                Qt管理端SocketClient
qt-server/network/               TCP服务、Session、Dispatcher、WebSocket
tests/network/                   通信层自动化测试
~~~

## 17. JSON Lines实现规范

当前V1代码选择“紧凑JSON + LF换行”作为实现方案。进入联调后，不能在某一端单独改为长度前缀。

实现要求：

1. 文本编码固定为UTF-8。
2. 一条消息使用QJsonDocument::Compact编码，末尾追加一个LF。
3. 接收方必须为每个TCP连接保存独立缓冲区。
4. 未收到LF时继续等待，不得把半条JSON判为格式错误。
5. 一次收到多个LF分隔帧时必须逐条处理。
6. 兼容CRLF，解析前移除行尾CR。
7. 空行忽略，不进入Dispatcher。
8. 当前单连接未完成帧缓存上限为2 MiB；超限返回4401并断开。
9. JSON可以解析但根节点不是对象时，按4401处理。
10. 无法提取requestId时，协议错误响应允许requestId为空字符串。

业务参数错误与Socket格式错误必须区分。例如pileId不存在属于4202，不是4401；payload不是对象才属于4401。

## 18. Qt 客户端设计规范

Qt 用户端和 Qt 管理端都属于 Socket 客户端。每个客户端内部所有页面共享一个 SocketClient，或由统一 NetworkClient 管理其实例。

页面不得：

- new自己的QTcpSocket；
- 直接拼接JSON字符串；
- 自己处理半包和粘包；
- 根据message文本判断成功或失败；
- 直接访问 SQLite；
- 在客户端自行决定订单、冻结、重启等服务端业务状态。

页面应该：

1. 调用sendRequest发送type、sessionId和payload；
2. 保存返回的requestId；
3. 在responseReceived中按requestId找到原请求；
4. 只根据code做程序判断；
5. 在4003时清理本地Session并返回登录流程；
6. 在disconnected或socketError时停止发起新的写操作。

两个 Qt 客户端都必须增加待处理请求表：

~~~text
requestId → 页面/回调、发送时间、消息type
~~~

普通请求建议5秒超时，地图请求可单独设置更长时间。超时后页面收到统一错误，不允许永久等待。重试同一业务操作时应复用原requestId，减少充值、下单和结算重复执行风险。

Qt 管理端可以发送 `ADMIN_*` 消息并展示 QChart，但图表数据必须来自服务端返回或推送，不得由管理端直接执行 SQL。

## 19. Qt/C++ 服务端设计规范

### 19.1 连接生命周期

每个QTcpSocket对应一个ClientSession和一个JsonLineCodec。连接断开后清除残留半帧并释放ClientSession。

Socket读取回调只完成：

1. readAll；
2. 分帧；
3. JSON与公共外壳校验；
4. 投递Dispatcher；
5. 编码并写回响应。

不得在Socket读取回调中执行耗时地图请求、ML任务或长事务。

### 19.2 Handler注册

业务模块通过MessageDispatcher::registerHandler注册：

- 消息type；
- 访问级别Public、User、Admin或AnyAuthenticated；
- Handler回调。

未在docs/03-API.md登记的type返回4401。已经登记但尚未接入业务Handler的消息返回5002，不能返回伪造成功数据。

登录Handler是公开路由。它在Service验证用户或管理员后调用SessionManager创建Session，并把sessionId放入成功响应。其他受保护Handler只能使用Dispatcher提供的可信principalId，不能相信payload中自报的userId或adminId。

服务端不包含管理界面，不直接绘制 QChart。管理端图表由 Qt 管理端根据服务端返回数据绘制。

### 19.3 响应规则

每个请求必须只返回一个标准响应：

~~~json
{
  "requestId": "原请求ID",
  "code": 200,
  "message": "success",
  "data": {}
}
~~~

- requestId必须原样返回。
- code用于程序判断。
- message用于日志和界面提示，不作为逻辑分支依据。
- data始终为JSON对象；没有数据时返回空对象。
- Handler不得直接向Socket写第二条响应。

## 20. Session设计规范

1. Session必须使用不可预测的随机值生成。
2. Session记录主体ID和角色USER或ADMIN。
3. 普通用户Session不能调用ADMIN消息，管理员Session不能冒充普通用户。
4. Service读取当前用户身份时使用SessionContext，不接受客户端传入的userId作为身份依据。
5. 服务端退出时内存Session自然失效，客户端收到4003后重新登录。
6. Session有效期、同账号多端登录和主动注销策略尚未冻结，进入完整登录开发前由客户端与业务负责人共同确认。

冻结用户是业务状态，不等同于Session格式无效。冻结规则由UserService依据SRS处理。

## 21. WebSocket实现规范

WebSocket与TCP业务Socket相互独立：

- WebSocket只服务大屏展示；
- 不承载创建订单、充值、结算等核心写业务；
- 大屏不得读取SQLite；
- Qt/C++ 服务端从Service或统计模块取得结果后调用publish；
- WebSocket模块不得自己执行统计SQL。

连接建立后，客户端必须先发送DASHBOARD_SUBSCRIBE。服务端只接受summary、pileStatus、revenueTrend和prediction四个topic；订阅集合按连接独立保存。

一次推送只包含一个topic：

~~~json
{
  "type": "DASHBOARD_UPDATE",
  "topic": "pileStatus",
  "data": {}
}
~~~

浏览器断线后，服务端删除其订阅。浏览器重连后必须重新订阅，不能假定旧订阅仍存在。

## 22. 并发与线程边界

Qt Socket对象具有线程归属，必须在其所属线程读取和写入。后续引入Business Worker或Database Worker时：

1. 网络线程解析完成后，通过Qt信号槽或线程安全队列投递任务；
2. Worker返回普通数据对象或ResponseMessage；
3. 最终Socket写操作回到Socket所属线程；
4. 不跨线程直接调用QTcpSocket；
5. 不跨线程共享QSqlDatabase；
6. SessionManager等共享容器必须使用锁或限制在单线程。

## 23. 联调与验收标准

通信层进入团队联调前至少验证：

1. 一条JSON拆成多次发送，只在收到LF后处理；
2. 多条JSON一次发送，分别得到对应响应；
3. 非法JSON返回4401且服务端不崩溃；
4. 未知type返回4401；
5. 缺少或错误Session返回4003；
6. 用户与管理员角色不能混用；
7. requestId在响应中原样返回；
8. 客户端断线时页面能够收到通知；
9. WebSocket非法topic被拒绝；
10. WebSocket只向订阅该topic的连接推送；
11. 至少完成一次真实 Qt 用户端 SocketClient 到 SocketServer 的登录闭环；
12. 至少完成一次真实 Qt 管理端 SocketClient 到 SocketServer 的管理员登录闭环；
13. 充值、创建订单和结算的重复请求不会重复写数据库。

当前自动化测试已覆盖公共分帧、消息外壳、Session角色和消息登记。真实业务Handler、两个 Qt 客户端的超时重连、WebSocket端到端及数据库幂等仍需在对应模块实现后补测。

## 24. 协作修改边界

网络负责人可以独立修改：

- SocketClient、SocketServer和ClientSession内部实现；
- JsonLineCodec实现与测试；
- SessionManager和Dispatcher内部结构；
- 不改变公共消息字段的性能、日志和异常修复。

以下内容必须与相关负责人共同评审：

| 变更 | 必须参与 |
| --- | --- |
| 新增或删除Message Type | 客户端、网络、业务 |
| 修改payload或data字段 | 调用端、业务、数据库 |
| 修改错误码含义 | 网络、业务、测试 |
| 修改Session规则 | 客户端、网络、业务 |
| 修改大屏topic和字段 | Web、网络、统计/ML |
| 修改订单或电桩状态 | 业务、数据库、网络 |
| 修改数据库字段 | 数据库、业务、网络 |

文档未定义的业务字段不得由网络层猜测。应先形成评审结论，再按“文档→代码→测试”的顺序修改。
