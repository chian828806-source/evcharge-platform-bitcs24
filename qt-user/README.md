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
| U01 | 手机号登录 | 已完成 UI、格式校验和真实 `USER_LOGIN` Socket 请求 |
| U02 | 首页/附近充电站 | 已完成 Mock 定位区、站点卡片和推荐标识 |
| U03 | 充电站详情 | 已完成 Mock 站点摘要、电桩状态和选择交互 |
| U04 | 当前充电/订单处理 | 已完成 Mock `CREATED`、`CHARGING`、`PENDING_PAYMENT` 等展示状态 |
| U05 | 我的 | 已完成 Mock 资料、头像选择、钱包、充值和订单列表 |
| U06 | 地图导航 | 已完成导航 UI 和地图容器占位；未接入地图 Web View |

User UI V1 已完成六个主要页面。当前仅 `USER_LOGIN` 接入真实 `SocketClient`；其余页面暂使用 Mock 数据。

未连接 `qt-server` 时，程序进入明确标注的演示/Mock 模式，便于独立完成 UI 评审。已经连接真实 Server 时，未接入的充值、资料更新和订单操作会明确报出待接入接口，绝不以本地 Mock 数据伪造成功；站点请求也不会在真实请求失败后伪造响应。

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

## 后续联调 TODO

- `USER_PROFILE_GET`、`USER_PROFILE_UPDATE`、`USER_AVATAR_UPLOAD`、`USER_RECHARGE` 和 `USER_ORDER_LIST`；
- `STATION_LIST_NEARBY`、`STATION_DETAIL_GET` 和 `MAP_GEOCODE`；
- `ORDER_ACTIVE_CHECK`、`ORDER_CREATE`、`ORDER_CANCEL`、`ORDER_START`、`ORDER_STOP` 和 `ORDER_SETTLE`；
- `QWebEngineView` + 腾讯地图 Web API、地址解析和路线展示。


## 页面调用原则

页面只调用 `SocketClient::sendRequest`，不直接操作 `QTcpSocket`。页面保存 `sendRequest` 返回的 `requestId`，并在 `responseReceived` 信号中按 `requestId` 匹配响应。

登录成功后，页面所属的Session对象应保存服务端返回的sessionId；之后的受保护请求都传入该值。
