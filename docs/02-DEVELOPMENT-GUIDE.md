# 开发规范

## 1. 通用原则

1. 实现必须能追踪到 SRS 需求编号。
2. 优先完成 Qt/C++、Socket、SQLite、多线程主线。
3. 不引入 Spring Boot、MySQL、REST 作为主架构。
4. 不修改公共契约，契约包括 Socket 消息、WebSocket 大屏消息、SQLite 表结构、状态枚举和统计口径。
5. 不重构无关模块。

## 2. Qt/C++ 目录建议

```text
qt-server/
├── main.cpp
├── network/
├── service/
├── repository/
├── model/
├── worker/
└── util/

qt-admin/
├── main.cpp
├── network/
├── model/
├── ui/
├── chart/
└── util/

qt-user/
├── main.cpp
├── network/
├── model/
├── ui/
└── util/
```

## 3. 分层规范

Qt/C++ 服务端建议采用：

```text
Socket Handler / WebSocket Publisher
  ↓
Service
  ↓
Repository
  ↓
QtSql / SQLite
```

Qt 管理端建议采用：

```text
UI / Chart
  ↓
SocketClient / NetworkClient
  ↓
Qt/C++ 服务端
```

Socket 消息处理入口只负责解析、校验、调用 Service 和返回响应，不直接写复杂业务逻辑。

## 4. 命名规范

类名使用 PascalCase：

```text
LoginWindow
StationPage
SocketServer
OrderService
UserRepository
```

函数和变量使用 camelCase：

```text
loadStations()
startCharging()
updateUserInfo()
```

成员变量使用 `m_` 前缀：

```text
m_userId
m_socket
m_database
```

## 5. Qt 网络规范

用户端统一封装：

```text
SocketClient / NetworkClient
```

Qt/C++ 服务端统一封装：

```text
SocketServer / ClientSession
```

Qt 用户端和 Qt 管理端业务页面不得直接操作 `QTcpSocket`。

Socket 消息统一使用 JSON Lines 或长度前缀格式，具体见 `docs/03-API.md`。

## 6. SQLite 与金额

数据库访问统一使用 QtSql 和 `QSQLITE`。

金额以“分”为整数保存和计算，例如：

```text
balance_fen INTEGER
amount_fen INTEGER
price_fen_per_kwh INTEGER
```

界面展示时转换为元。

## 7. 时间

时间字段使用 TEXT 保存：

```text
yyyy-MM-dd HH:mm:ss
```

所有模块统一使用本地时间，演示阶段不处理复杂时区。

## 8. 多线程规范

UI 线程不得执行阻塞 Socket、数据库写入、充电计时或长时间 ML 调用。

SQLite 多线程规则：

- 不跨线程共享同一个 `QSqlDatabase` 连接；
- 每个数据库线程创建自己的连接名；
- 写操作集中到 Database Worker 或通过队列串行执行；
- 失败时返回明确错误码。

## 9. 错误处理

必须覆盖：

| 错误情况 | 处理办法 |
| --- | --- |
| Socket 断线 | 用户端提示断线，允许重连 |
| 消息不完整 | 丢弃或等待完整帧，返回协议错误 |
| 数据库打开失败 | 服务端返回数据库错误；管理端提示并阻止继续提交 |
| 地图 API 失败 | 提示定位或导航失败 |
| 余额不足 | 订单保持 `PENDING_PAYMENT` |
| 重复结算 | 返回已结算或待结算状态，不重复扣款 |
| 非法状态转换 | 拒绝操作并记录日志 |

## 10. 头像与文件

头像由用户端选择图片并传给 Qt/C++ 服务端。服务端保存文件，SQLite 保存相对路径。必须限制文件类型、大小和保存目录。

## 11. Web 大屏

Web 大屏通过 WebSocket 连接 Qt/C++ 服务端：

```text
ws://<server-host>:<port>/dashboard
```

大屏页面不得直接访问 SQLite。WebSocket 消息只服务运营展示和预测展示，不承载用户充电核心业务。

## 12. ML 模块

ML 使用 Python，只读取固定演示数据或 Qt/C++ 服务端导出的 CSV/JSON；ML 输出预测 JSON，由服务端校验并导入 SQLite。ML 不得直接访问 SQLite。

不要求高精度模型；要求流程可运行、结果可展示。

## 13. Definition of Ready

任务开始前必须明确：

- SRS 需求编号；
- 涉及 Socket 消息；
- 涉及模块边界，确认属于 `qt-user`、`qt-admin`、`qt-server`、`shared`、`web-dashboard`、`ml` 或 `database`；
- 涉及 SQLite 表；
- 状态变化；
- 验收标准。

## 14. Definition of Done

功能完成至少满足：

- 本地可运行；
- Socket 消息联通；
- 用户端、管理端和服务端职责没有混写；
- SQLite 数据正确；
- 基本异常处理完成；
- 必要测试或手工验证通过；
- 文档同步更新；
- 已提交 Git。
