# qt-server — Qt/C++ 服务端

本目录是目标架构中的独立服务端目录。

服务端负责业务服务和数据访问，不包含管理界面。

## 职责

- 使用 `QTcpServer` 接收 Qt 用户端和 Qt 管理端连接；
- 解析 Socket JSON Lines 消息；
- 执行登录、资料、站点、电桩、订单、结算、管理统计和远程重启业务；
- 通过 QtSql `QSQLITE` 访问 SQLite；
- 提供 WebSocket 大屏数据服务；

## 禁止

- 不绘制管理端 QChart；
- 不包含管理端页面；
- 不让 Qt 用户端或 Qt 管理端直接访问 SQLite；
- 不把业务规则写在 Socket 读取回调中。

## 分层与隔离原则

服务端只有一个可执行程序和一个统一的 Socket 入口，但业务入口按调用角色隔离：

- 用户消息的 Handler、Service 放在 `handlers/user/`、`services/user/`；
- 管理消息的 Handler、Service 放在 `handlers/admin/`、`services/admin/`；
- Network、Repository、Database、Model、Worker 等基础能力由两边共用；
- 用户和管理员身份统一由 `SessionManager` 保存，并由 `MessageDispatcher`
  按 USER / ADMIN 访问级别鉴权；
- 不得为了隔离而复制两套数据库连接、实体模型或 Repository。

依赖方向固定为：

```text
network
  -> handlers/user | handlers/admin
  -> services/user | services/admin
  -> repositories
  -> database / QtSql / SQLite
```

## 目标目录

```text
qt-server/
├── qt-server.pro
├── main.cpp                         # 只负责配置读取、对象装配和服务启动
├── network/                         # 共用：TCP、Session、Dispatcher、WebSocket
│
├── handlers/                        # 消息入口：校验 payload、调用 Service、映射响应
│   ├── user/                        # USER/STATION/ORDER 用户消息
│   │   ├── registeruserhandlers.h/.cpp
│   │   └── user-handlers.pri
│   └── admin/                       # ADMIN 管理消息
│       ├── registeradminhandlers.h/.cpp
│       └── admin-handlers.pri
│
├── services/                        # 业务规则、状态机和事务意图
│   ├── user/                        # 登录、资料、查站、充电、结算
│   └── admin/                       # 管理登录、统计、冻结、建站、重启
│
├── repositories/                    # 共用：参数化 SQL 和数据库对象映射
├── database/                        # 共用：QSQLITE 连接与事务支持
├── models/                          # 共用：User、Station、Pile、Order 等领域数据
└── common/                          # 共用：密码哈希、ServiceResult 和少量通用类型
```

目录使用复数形式，后续统一采用 `handlers`、`services`、`repositories`、
`models`、`workers`，不再新增并行的 `service/`、`repository/`、`model/`
或 `worker/`。

## 协作边界

| 区域 | 用户后端负责人 | 管理后端负责人 | 共同评审 |
| --- | --- | --- | --- |
| `handlers/user/` | 主责 | 不修改 | Message Type、响应结构 |
| `services/user/` | 主责 | 不修改 | 共享业务规则 |
| `handlers/admin/` | 不修改 | 主责 | Message Type、权限 |
| `services/admin/` | 不修改 | 主责 | 共享业务规则 |
| `repositories/` | 按任务修改 | 按任务修改 | SQL、字段映射、查询口径 |
| `database/`, `models/`, `common/` | 按任务修改 | 按任务修改 | 公共接口和线程边界 |
| `network/`, `main.cpp`, `qt-server.pro` | 尽量不重复修改 | 尽量不重复修改 | 集成负责人统一装配 |

用户和管理员模块分别维护自己的 `.pri` 文件，主工程只执行 `include(...)`，
减少双方新增源码时同时修改 `qt-server.pro`。公共目录发生接口变化时必须通知
另一方并共同评审。

服务端实现仅放在本目录；用户界面放在 `qt-user/`，管理界面放在
`qt-admin/`。
