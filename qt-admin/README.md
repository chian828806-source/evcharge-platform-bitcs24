# qt-admin — Qt 管理端

本目录是目标架构中的独立管理客户端目录。

管理端负责运营人员界面和图表展示，通过 TCP Socket 调用 Qt/C++ 服务端。

## 职责

- 管理员登录；
- 营收统计、近 7 日 / 30 日趋势和 QChart 展示；
- 充电站、电桩和用户管理界面；
- 冻结、解冻和手机号模糊查询；
- 发起远程重启请求；
- 展示负荷预测和运营预警；
- 处理服务端返回的错误码并给出界面提示。

## 禁止

- 不启动 `QTcpServer`；
- 不直接访问 SQLite；
- 不直接执行 SQL；
- 不自行决定订单、电桩、冻结、重启等业务状态；
- 不承载 WebSocket 大屏服务。

## 目标目录

```text
qt-admin/
├── main.cpp
├── network/
├── model/
├── ui/
├── chart/
└── util/
```

管理端网络访问应复用 `SocketClient` / `NetworkClient` 封装。
