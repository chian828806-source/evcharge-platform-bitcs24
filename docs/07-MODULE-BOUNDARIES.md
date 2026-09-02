# EVCharge Platform V1 模块职责与开发边界

## 1. 文档目的

本文档定义 EVCharge Platform V1 各主要模块的职责、调用关系和禁止事项。

目标是保证多人并行开发过程中：

- 模块职责清晰；
- 接口稳定；
- 减少代码冲突；
- 避免重复实现；
- 防止跨层访问；
- 降低后期集成成本。

本文档适用于当前 `develop` 分支之后的所有 V1 开发。

---

# 2. 总体架构

系统主调用链：

```text
Qt User
    ↓
SocketClient
    ↓
TCP
    ↓
SocketServer
    ↓
ClientSession
    ↓
MessageDispatcher
    ↓
Handler
    ↓
Service
    ↓
Repository
    ↓
SQLite
```

管理端：

```text
Qt Admin
    ↓
AdminSocketClient
    ↓
TCP
    ↓
Server
    ↓
Handler
    ↓
Service
    ↓
Repository
    ↓
SQLite
```

Web：

```text
Service / Event
    ↓
WebSocket Publisher
    ↓
Web Dashboard
```

ML：

```text
Server
    ↓
Export CSV / JSON
    ↓
ML

ML
    ↓
Prediction JSON
    ↓
Server
    ↓
Service / Repository
    ↓
SQLite
```

---

# 3. Network Infrastructure

当前主要来源：

```text
feature/server-admin-split
```

网络模块负责：

## 3.1 TCP Server

负责：

- listen；
- accept；
- connection lifecycle；
- socket read/write。

不负责：

- 用户业务；
- 订单业务；
- SQL；
- 计费；
- 数据库事务。

---

## 3.2 JSON Lines

网络协议统一采用：

```text
Compact JSON + LF
```

网络层负责：

- 数据缓存；
- 半包；
- 粘包；
- CRLF/LF；
- 最大未完成 Frame；
- JSON Parse。

业务 Handler 不应该再次自行处理 TCP 分帧。

---

# 4. ClientSession

ClientSession 负责：

- 当前连接身份；
- user/admin role；
- authenticated state；
- userId/adminId；
- connection-scoped information。

ClientSession 不负责：

- 查询用户；
- 验证密码；
- 修改数据库；
- 创建订单。

登录 Handler / Service 验证身份成功后，可以更新 Session。

---

# 5. MessageDispatcher

Dispatcher 负责：

```text
message type
→ corresponding handler
```

例如：

```text
user.login
→ UserHandler

order.create
→ OrderHandler
```

Dispatcher 可以负责：

- Handler Registry；
- 基础消息路由；
- 必要的 Session / Role Gate。

Dispatcher 不负责：

- SQL；
- 订单状态计算；
- 余额计算；
- 计费；
- 复杂业务逻辑。

---

# 6. SocketClient / AdminSocketClient

Qt User 和 Qt Admin 必须分别通过统一网络 Client 使用服务端 API。

页面禁止直接：

```cpp
new QTcpSocket(...)
```

页面应该：

```text
Page
→ SocketClient
→ request
```

SocketClient 负责：

- connection；
- reconnect infrastructure；
- requestId；
- pending request；
- timeout；
- JSON serialization；
- response dispatch；
- network state signal。

SocketClient 不负责：

- 页面展示逻辑；
- SQL；
- 本地数据库；
- 服务端业务规则。

---

# 7. Handler Layer

Handler 是网络协议与业务 Service 之间的适配层。

职责：

```text
JSON Request
↓
Validate Required Fields
↓
Call Service
↓
Convert Result
↓
JSON Response
```

Handler 允许负责：

- API 字段解析；
- 基础类型检查；
- Session 读取；
- DTO 转换；
- Error → API Response 映射。

Handler 不应该负责：

- SQL；
- Transaction；
- 核心业务规则；
- 大量业务判断。

错误示例：

```text
OrderHandler
直接 UPDATE charging_order
```

禁止。

---

# 8. Service Layer

Service 是系统业务规则的核心。

例如：

```text
UserService
AdminService
ChargingService
OrderService
RechargeService
PredictionService
```

Service 负责：

- 业务状态机；
- 权限规则；
- 金额计算；
- 业务校验；
- Transaction Boundary；
- 多 Repository 协调；
- 业务幂等；
- 业务事件。

例如创建订单：

```text
OrderService
↓
检查 User
↓
检查 Pile
↓
检查 Pile AVAILABLE
↓
建立 Transaction
↓
创建 Order
↓
更新 Pile
↓
Commit
```

这是 Service 的职责。

---

# 9. Service 禁止事项

Service 不应该：

- 直接处理 TCP buffer；
- 自己解析 JSON Lines；
- 持有 QTcpSocket；
- 操作 QWidget；
- 操作 Qt 页面；
- 编写 Web 页面代码。

Service 可以返回结构化业务结果，由 Handler 决定 API Response。

---

# 10. Repository Layer

Repository 是 Service 与数据库之间的唯一主要访问边界。

建议例如：

```text
UserRepository
AdminRepository
StationRepository
PileRepository
OrderRepository
RechargeRepository
PredictionRepository
OperationLogRepository
```

职责：

- SQL；
- insert；
- update；
- select；
- database row ↔ model conversion。

Repository 不负责：

- 网络；
- UI；
- WebSocket；
- 业务流程决策。

---

# 11. Repository 禁止业务规则

错误示例：

```text
OrderRepository::createOrder()
内部判断余额够不够
```

余额是否足够属于：

```text
OrderService / ChargingService
```

而不是 Repository。

Repository 应保持：

**数据访问层。**

---

# 12. Database Infrastructure

当前主要来源：

```text
feature/database-v1-schema
```

数据库模块负责：

- SQLite Schema；
- Table；
- Foreign Key；
- Index；
- Constraint；
- Init Data；
- 数据存储契约。

数据库模块不负责：

- TCP；
- Qt UI；
- WebSocket；
- Order Workflow；
- 登录流程。

---

# 13. 数据库访问边界

禁止：

```text
Qt User → SQLite
```

禁止：

```text
Qt Admin → SQLite
```

禁止：

```text
Web Dashboard → SQLite
```

禁止：

```text
ML → SQLite
```

正常路径必须是：

```text
Client
→ Server
→ Service
→ Repository
→ Database
```

---

# 14. QtSql 与线程边界

QSqlDatabase 必须遵守 Qt 数据库线程规则。

原则：

**数据库连接在哪个线程创建，就在哪个线程使用。**

禁止把一个线程中的 QSqlDatabase 连接直接交给另一个线程。

如果后续实现 Database Worker：

```text
Worker Thread
→ Own QSqlDatabase Connection
```

其他线程使用 queued signal / invokeMethod 与其通信。

---

# 15. Socket 与线程边界

QTcpSocket / QWebSocket 具有 QObject Thread Affinity。

禁止：

```text
Worker Thread
直接操作属于 Socket Thread 的 QTcpSocket
```

正确方式：

```text
Worker
→ signal
→ Socket owning thread
→ socket.write()
```

---

# 16. 数据库 Schema 与 Service 的关系

数据库 CHECK 是：

**数据完整性的最后一道防线。**

Service 仍然必须进行业务校验。

例如数据库：

```text
balance_fen >= 0
```

并不意味着 Service 可以直接尝试扣成负数。

正确逻辑：

```text
Service 先检查余额
↓
执行 Transaction
↓
Repository UPDATE
↓
Database Constraint 最终保护
```

---

# 17. Charging Order 状态机边界

Order Status 的状态转换属于：

```text
Service Layer
```

Database 只负责保存合法枚举。

例如：

```text
CREATED
→ CHARGING
```

是否允许转换：

由 Service 判断。

数据库不承担完整业务状态机。

---

# 18. 统一计费规则

V1 统一：

```text
amountFen =
round(
    energyKwh *
    (
        priceFenPerKwh
        +
        serviceFeeFenPerKwh
    )
)
```

创建订单时，应保存：

- price snapshot；
- service fee snapshot。

后续即使充电站修改价格：

历史订单仍按照订单创建时保存的价格处理。

最终金额计算逻辑属于：

```text
Service Layer
```

数据库负责保存结果。

---

# 19. WebSocket 边界

WebSocket 模块负责：

- client connection；
- topic subscription；
- event publish；
- connection cleanup。

它不应该自己查询和计算复杂业务数据。

正确方式：

```text
Service completes business action
↓
generate event
↓
WebSocket Publisher
↓
Dashboard
```

例如：

```text
OrderService
完成订单
↓
OrderUpdated Event
↓
WebSocket
```

---

# 20. Web Dashboard 边界

Web Dashboard 负责：

- ECharts；
- 页面显示；
- 用户交互；
- WebSocket event consumption。

Web Dashboard 不负责：

- SQLite；
- 服务端业务；
- 订单金额重新计算。

---

# 21. ML 模块边界

ML 模块主要负责：

```text
Input Dataset
↓
Model
↓
Prediction Result
```

ML 不直接修改主数据库。

正确方式：

```text
ML
→ prediction JSON
→ Server
→ PredictionService
→ PredictionRepository
→ SQLite
```

这样：

- 数据校验；
- model version；
- station；
- horizon；
- timestamp；

均由服务端统一控制。

---

# 22. Operation Log

业务操作日志推荐由 Service 产生。

例如：

```text
AdminService::freezeUser()
↓
UserRepository
↓
OperationLogRepository
```

Handler 不直接生成核心业务审计日志。

---

# 23. 模块依赖方向

推荐依赖：

```text
UI
↓
Client Network
↓
Server Network
↓
Handler
↓
Service
↓
Repository
↓
Database
```

依赖方向原则：

**只能向下依赖。**

禁止：

```text
Repository → Service
Service → Handler
Database → Socket
```

---

# 24. 当前两个基础模块的状态

进入 `develop` 第一版后：

## Network Module

定义为：

```text
Network Infrastructure V1
```

已提供：

- TCP；
- WebSocket；
- JSON Lines；
- Session；
- Dispatcher；
- Client abstraction。

不代表：

```text
Server Business Complete
```

---

## Database Module

定义为：

```text
Database Infrastructure V1
```

已提供：

- Schema；
- Constraint；
- Index；
- Initialization Data。

不代表：

```text
Repository Complete
```

或：

```text
Business Persistence Complete
```

---

# 25. 后续开发推荐顺序

基础模块进入 develop 后，推荐：

```text
Step 1
Model / DTO

Step 2
Repository

Step 3
Service

Step 4
Handler

Step 5
Qt User / Admin Integration

Step 6
WebSocket Business Events

Step 7
ML Integration

Step 8
Integration Tests

Step 9
System Acceptance
```

---

# 26. Code Review 必查项

以后每次 Pull Request 至少检查：

- 是否跨层访问；
- UI 是否直接 SQL；
- UI 是否直接创建业务 Socket；
- Handler 是否写 SQL；
- Repository 是否写业务规则；
- Service 是否操作 Socket；
- Worker 是否跨线程操作 QSqlDatabase；
- Worker 是否跨线程操作 Socket；
- 是否违反 API；
- 是否违反 Database Contract；
- 是否破坏现有 requestId；
- 是否绕过 Session；
- 是否添加重复基础设施。

---

# 27. 最终原则

V1 的核心原则为：

> Network 负责通信，
> Handler 负责协议适配，
> Service 负责业务，
> Repository 负责数据访问，
> Database 负责数据完整性，
> UI/Web 负责展示，
> ML 负责预测。

任何模块都不应因为“实现方便”而跨越上述边界。

如果某项功能需要多个模块协作：

优先通过明确接口进行集成，

而不是直接访问其他模块内部实现。
