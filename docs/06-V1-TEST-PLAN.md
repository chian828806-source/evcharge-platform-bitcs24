# EVCharge Platform V1 测试缺口与后续测试计划

## 1. 文档目的

本文档用于记录 EVCharge Platform 第一阶段网络基础设施与数据库基础设施合并进入 `develop` 后仍需要补充的测试内容。

当前阶段的目标是建立稳定的开发基础，而不是完成最终系统验收。

因此：

- 网络模块允许在核心协议和基础结构完成后先进入 `develop`；
- 数据库模块允许在 Schema、Constraint 和 Initialization Data 完成后先进入 `develop`；
- 本文档记录当前暂未完成但后续必须补充的自动化测试和集成测试；
- 缺少本文档中的测试不代表当前基础模块不能进入 `develop`；
- 在 V1 最终验收前，本文档中的核心测试必须完成。

---

# 2. 测试层级

项目后续测试划分为四个层级：

1. Unit Test
2. Module Integration Test
3. Cross-Module Integration Test
4. System Acceptance Test

---

# 3. 网络模块现有基础测试

当前网络模块已经具有部分协议级测试，包括：

- JSON Lines 半包处理；
- JSON Lines 粘包处理；
- 超大未完成数据帧处理；
- 基础请求格式验证；
- 基础响应格式验证；
- Session 与 Dispatcher 边界；
- 已注册消息类型识别；
- 用户端请求超时；
- 管理端请求超时。

这些测试作为网络基础设施第一版的最低回归测试保留。

---

# 4. 网络模块待补测试

## 4.1 TCP 非法 JSON 测试

### 目的

验证真实 TCP 连接收到非法 JSON 数据时，服务端不会崩溃，并能够按照 API 契约返回统一错误。

### 测试场景

客户端发送：

```text
{"type":
```

或其他完整但非法的 JSON Line。

### 验证内容

- 服务端连接保持行为符合设计；
- 返回统一错误响应；
- error code 符合 API 文档；
- 不进入业务 Handler；
- 服务端进程不崩溃。

---

## 4.2 Unknown Message Type 测试

### 目的

验证客户端请求未注册消息类型时，MessageDispatcher 正确拒绝请求。

### 示例

```json
{
  "type": "unknown.test",
  "requestId": "req-001",
  "data": {}
}
```

### 验证内容

- 服务端返回失败；
- requestId 保持一致；
- 错误码符合协议；
- 不调用任何业务 Service。

---

# 5. requestId 关联测试

## 5.1 Response Echo

验证：

```text
Request.requestId
=
Response.requestId
```

### 场景

同时连续发送多个请求：

```text
req-001
req-002
req-003
```

即使响应时间不同，客户端也必须能够通过 requestId 匹配正确请求。

---

## 5.2 并发 Pending Request

验证同一个 SocketClient 中同时存在多个未完成请求时：

- 不发生覆盖；
- 不发生错误回调；
- timeout 能精确对应 requestId；
- response 到达后只清除对应请求。

---

# 6. TCP Disconnect 测试

## 6.1 服务端主动断开

验证：

- SocketClient 能检测断线；
- UI 可收到连接状态变化；
- pending request 被正确处理；
- 不出现永久等待。

## 6.2 网络异常断开

模拟：

- 服务端退出；
- 网络中断；
- socket error。

验证客户端不会崩溃。

---

# 7. Reconnect 测试

验证客户端断开后：

1. 能重新连接；
2. 新连接可以正常发送请求；
3. 旧 Session 不会污染新连接；
4. 登录状态按照系统设计重新建立；
5. 不重复注册 signal；
6. 不产生重复消息。

用户端和管理端均需要覆盖。

---

# 8. Session 测试

## 8.1 未登录访问

未建立用户 Session 时访问需要认证的 Handler。

期望：

- 请求被拒绝；
- 不进入 Service。

## 8.2 用户访问管理员 API

普通用户 Session 请求管理员消息。

期望：

- 权限拒绝。

## 8.3 管理员访问管理员 API

管理员 Session 请求合法管理员消息。

期望：

- Dispatcher 正确放行到 Handler。

## 8.4 Logout

Logout 后 Session 必须失效。

---

# 9. WebSocket 测试

## 9.1 Dashboard Path

仅允许文档规定的 WebSocket Endpoint。

例如：

```text
/dashboard
```

测试其他路径的处理行为。

---

## 9.2 Topic Subscribe

客户端订阅合法 topic。

验证：

- Subscription 成功；
- 后续事件能够收到。

---

## 9.3 Invalid Topic

客户端订阅不存在或禁止的 topic。

验证：

- 服务端明确拒绝；
- 不创建无效订阅；
- 不影响其他客户端。

---

## 9.4 Subscriber Isolation

建立两个 WebSocket Client：

Client A：

```text
subscribe station
```

Client B：

```text
subscribe order
```

发送 station event。

期望：

- Client A 收到；
- Client B 不收到。

---

## 9.5 Disconnect Cleanup

WebSocket Client 断开后：

- Subscription 自动释放；
- 不保留失效连接；
- 后续 publish 不访问无效 socket。

---

# 10. 数据库 Schema 测试

建议建立：

```text
tests/database/
```

或者建立独立验证脚本。

---

## 10.1 Table Existence

验证以下表全部存在：

- user
- admin
- charging_station
- charging_pile
- charging_order
- recharge_record
- prediction
- operation_log

---

## 10.2 Index Existence

检查数据库文档规定的：

- UNIQUE INDEX；
- Query INDEX；

全部存在。

---

# 11. Database Constraint 测试

主动插入非法数据，确认数据库拒绝。

至少包括：

### User

- duplicate phone；
- negative balance。

### Station

- duplicate station_no；
- negative price；
- negative service fee。

### Pile

- duplicate `(station_id, pile_no)`；
- invalid power_kw；
- invalid status。

### Order

- duplicate order_no；
- negative amount；
- negative energy；
- invalid status。

### Recharge

- duplicate recharge_no；
- negative amount。

### Prediction

- predicted_load < 0；
- predicted_load > 1；
- predicted_available_count < 0；
- invalid horizon；
- invalid peak_level。

---

# 12. Foreign Key 测试

执行：

```sql
PRAGMA foreign_key_check;
```

期望：

无返回记录。

同时测试：

- 删除被订单引用的用户；
- 删除被充电桩引用的充电站；
- 删除被订单引用的充电桩。

确认行为符合 Schema 设计。

---

# 13. Schema Reinitialization 测试

数据库存在完整初始化数据后再次执行 Schema 初始化流程。

验证：

- DROP 顺序正确；
- 不因为 Foreign Key 导致初始化失败；
- Schema 可以重新建立；
- Init Data 可以重新导入。

---

# 14. Initialization Data 测试

初始化数据至少需要覆盖：

## User

- 正常用户；
- 冻结用户；
- 低余额用户。

## Charging Pile

至少覆盖：

- AVAILABLE；
- RESERVED；
- CHARGING；
- FAULT；
- OFFLINE。

## Charging Order

至少覆盖：

- CREATED；
- CHARGING；
- PENDING_PAYMENT；
- COMPLETED；
- CANCELLED。

## Prediction

每个测试站点应具有合理的：

- 1h；
- 6h；
- 24h；

预测数据。

---

# 15. 数据一致性测试

## 15.1 Charging Pile 与 Order

当 Pile：

```text
status = RESERVED
```

或：

```text
status = CHARGING
```

时，检查：

```text
current_order_id
```

是否指向合理订单。

---

## 15.2 Completed Order

检查：

- energy_kwh；
- price_fen_per_kwh；
- service_fee_fen_per_kwh；
- amount_fen；

之间的数据关系合理。

统一公式为：

```text
amountFen =
round(
    energyKwh *
    (priceFenPerKwh + serviceFeeFenPerKwh)
)
```

允许由于初始化测试数据需要产生合理的取整误差，但不得出现明显矛盾。

---

# 16. 业务层加入后的集成测试

以下测试当前暂不属于网络模块或数据库模块单独负责，必须等：

```text
Handler
Service
Repository
```

实现后再进行。

---

## 16.1 Login End-to-End

### Qt User

```text
Qt User
→ SocketClient
→ TCP
→ Handler
→ UserService
→ UserRepository
→ SQLite
```

验证真实登录。

### Qt Admin

```text
Qt Admin
→ AdminSocketClient
→ TCP
→ Handler
→ AdminService
→ AdminRepository
→ SQLite
```

验证真实管理员登录。

---

# 17. 幂等测试

以下写操作必须重点验证：

- create order；
- recharge；
- stop charging / settlement；
- remote restart。

同一个业务请求被重复发送时，不得产生重复数据库记录或重复扣款。

requestId 本身不一定等同于最终业务幂等 Key。

业务层必须根据对应业务定义实现幂等。

---

# 18. 订单状态机测试

后续 OrderService 完成后测试至少覆盖：

```text
CREATED
→ CHARGING
→ COMPLETED
```

以及：

```text
CREATED
→ CANCELLED
```

和：

```text
CHARGING
→ PENDING_PAYMENT
```

等项目文档允许的转换。

非法状态转换必须被 Service 层拒绝。

---

# 19. 并发测试

后续业务层完成后重点测试：

两个用户同时尝试预约同一个 AVAILABLE Pile。

必须确保：

只有一个请求成功。

该测试需要验证：

- Service；
- Transaction；
- Repository；
- Database；

之间的并发保护机制。

---

# 20. Web Dashboard 数据闭环测试

后续业务层完成后验证：

```text
Database
→ Service
→ WebSocket Publisher
→ Web Dashboard
```

内容至少包括：

- Station Status；
- Pile Status；
- Order Update；
- Prediction Update。

---

# 21. ML 数据闭环测试

后续 ML 模块完成后验证：

```text
Server
→ Export Data
→ ML
→ Prediction Result
→ Server
→ Database
→ WebSocket
→ Dashboard
```

验证：

- 时间范围；
- station_id；
- horizon；
- prediction value；
- model_version；
- generated_at；

能够完整传递。

---

# 22. 测试完成阶段划分

## Phase 1

当前：

- Network Infrastructure；
- Database Infrastructure；

允许进入 `develop`。

## Phase 2

业务 Service / Repository 接入后：

完成核心 Integration Test。

## Phase 3

Qt / Web / ML 接入后：

完成 End-to-End Test。

## Phase 4

V1 Release 前：

完成全部 Acceptance Test。

---

# 23. 当前结论

当前网络与数据库模块的第一版目标是：

**提供稳定的基础设施和清晰的开发契约。**

暂未实现完整测试体系是当前阶段已知并接受的技术债务。

但在 V1 Release 或最终课程验收之前：

本文档标记的核心网络、数据库、业务和系统集成测试必须完成。
