# qt-server — Qt/C++ 服务端

本目录是目标架构中的独立服务端目录。

服务端负责业务服务和数据访问，不包含管理界面。

## 职责

- 使用 `QTcpServer` 接收 Qt 用户端和 Qt 管理端连接；
- 解析 Socket JSON Lines 消息；
- 执行登录、站点、电桩、订单、充值、结算、管理统计和远程重启业务；
- 通过 QtSql `QSQLITE` 访问 SQLite；
- 提供 WebSocket 大屏数据服务；
- 导入或读取 Python ML 预测结果；
- 维护业务线程、数据库线程和充电计时任务。

## 禁止

- 不绘制管理端 QChart；
- 不包含管理端页面；
- 不让 Qt 用户端或 Qt 管理端直接访问 SQLite；
- 不把业务规则写在 Socket 读取回调中。

## 目标目录

```text
qt-server/
├── main.cpp
├── network/
├── service/
├── repository/
├── model/
├── worker/
└── util/
```

`zly` 分支当前的 `qt-server-admin/network` 已实现部分服务端通信骨架，后续应迁移到本目录。
