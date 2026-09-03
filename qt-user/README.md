# qt-user — Qt 用户端

本目录承载车主侧 Qt 界面，并提供可供页面复用的 SocketClient。

用户端是业务客户端，只连接 `qt-server`，不连接 `qt-admin`，不直接访问 SQLite。

## 文件说明

| 路径 | 功能 |
| --- | --- |
| network/ | TCP连接、请求发送和响应接收 |
| qt-user-network.pro | qmake静态库工程 |
| ui/ | 用户端页面、页面导航和交互状态 |
| resources/ | 用户端统一 QSS 样式 |
| qt-user.pro | 可运行的 Qt Widgets 用户端工程 |

## UI V1 页面

| 编号 | 页面 | 当前状态 |
| --- | --- | --- |
| U01 | 手机号登录 | 已接入 `USER_LOGIN`、格式校验和 Session 保存 |
| U02 | 首页/附近充电站 | 已接入附近站点和智能推荐列表 |
| U03 | 充电站详情 | 已接入站点详情、电桩状态和选桩下单 |
| U04 | 当前充电/订单处理 | 已接入活动订单轮询、开始、停止、取消和结算 |
| U05 | 我的 | 已接入资料、昵称、头像、钱包充值和订单列表 |
| U06 | 地图导航 | 已完成导航 UI 和地图容器占位；尚未接入 QWebEngineView 和腾讯地图 API |

User UI V1 已完成六个主要页面，并已按用户后端最新 03/09 文档加入接口适配。未连接服务端时仍可使用 Mock 数据完成 UI 评审。

未连接 `qt-server` 时，程序进入明确标注的演示/Mock 模式。连接真实 Server 后，页面发送真实请求并只使用服务端响应更新业务状态，不以本地 Mock 数据伪造成功。

最新 `develop` 已具备 User、Station、Order 等后端接口及公共协议。下一阶段应严格按 [`docs/03-API.md`](../docs/03-API.md) 和 `shared/protocol/MessageTypes` 完成 UI 与现有后端的端到端联调，不在 `qt-user` 重新定义协议。

地图当前为 UI/容器占位，`QWebEngineView` + 腾讯地图 Web API 尚未正式接入。

## 构建运行

在安装 Qt 6.2 或更高版本的环境中：

```bash
qmake qt-user/qt-user.pro
make
./evcharge-user
```

Windows 使用对应编译套件的 `mingw32-make` 或 `nmake`。

## 当前接口边界

- 已按 `feature/user-backend` 的 03/09 文档接入 16 个已实现用户接口；
- 充电中每秒调用 `ORDER_ACTIVE_CHECK`，金额与进度仅展示服务端结果；
- `MAP_GEOCODE` 的真实 Adapter、腾讯地图 Key 和真实地图页面仍由后端/地图模块补齐；
- `PREDICTION_LIST` 没有 Handler，也不是当前用户端必需接口。


## 页面调用原则

页面只调用 `SocketClient::sendRequest`，不直接操作 `QTcpSocket`。页面保存 `sendRequest` 返回的 `requestId`，并在 `responseReceived` 信号中按 `requestId` 匹配响应。

登录成功后，页面所属的Session对象应保存服务端返回的sessionId；之后的受保护请求都传入该值。
