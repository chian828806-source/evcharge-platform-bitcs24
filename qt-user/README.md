# qt-user — Qt 用户端

本目录承载车主侧 Qt 界面，并提供可供页面复用的 SocketClient 与地图导航页组件。

用户端是业务客户端，只连接 `qt-server`，不连接 `qt-admin`，不直接访问 SQLite。

## 文件说明

| 路径 | 功能 |
| --- | --- |
| network/ | TCP连接、请求发送和响应接收 |
| qt-user-network.pro | qmake静态库工程 |
| ui/ | 用户端页面、页面导航和交互状态 |
| resources/ | 用户端统一 QSS 样式 |
| qt-user.pro | 可运行的 Qt Widgets 用户端工程 |
| map/mapnavigationpage.h/.cpp | U06 地图导航组件；接收起终点与服务端提供的导航 URL，使用 QWebEngineView 展示路线 |
| qt-user-map.pro | 地图导航组件独立静态库工程（供单独编译或复用） |

## UI V1 页面

| 编号 | 页面 | 当前状态 |
| --- | --- | --- |
| U01 | 手机号登录 | 已接入 `USER_LOGIN`、格式校验和 Session 保存 |
| U02 | 首页/附近充电站 | 已接入附近站点和智能推荐列表 |
| U03 | 充电站详情 | 已接入站点详情、电桩状态和选桩下单 |
| U04 | 当前充电/订单处理 | 已接入活动订单轮询、开始、停止、取消和结算 |
| U05 | 我的 | 已接入资料、昵称、头像上传、钱包充值和订单列表 |
| U06 | 地图导航 | 已接入 `QWebEngineView`、驾车/步行切换和服务端路线规划 |

未连接 `qt-server` 时，程序使用明确标注的 Mock 数据，便于独立完成 UI 评审。
Mock 数据不改变公共 Socket 协议，也不作为真实业务结果。

## 构建运行

在安装 Qt 6.2 或更高版本的环境中：

```bash
qmake qt-user/qt-user.pro
make
./evcharge-user
```

Windows 使用对应编译套件的 `mingw32-make` 或 `nmake`。

## 后端联调状态

冻结登录、头像读取、充电秒数和站点综合单价的 Socket 契约已在
`docs/03-API.md` 冻结。U06 的路线展示页已接入：页面发送 `MAP_ROUTE_PLAN`，服务端
`MapAdapter` 调用腾讯路线规划后返回距离、时长和解压后的路线折线；其余页面只以真实
Socket 响应更新业务状态，未连接服务端时才进入明确标注的 Mock 模式。

地图 Key 仅由服务端配置；U06 页面不保存 Key。页面通过 `setRoutePlan()` 在
`QWebEngineView` 中展示服务端返回的路线预览。

## 页面调用原则

页面只调用 `SocketClient::sendRequest`，不直接操作 `QTcpSocket`。页面保存 `sendRequest` 返回的 `requestId`，并在 `responseReceived` 信号中按 `requestId` 匹配响应。

登录成功后，页面所属的Session对象应保存服务端返回的sessionId；之后的受保护请求都传入该值。

## 地图页接入

首页 U02 或站点详情 U03 创建 `MapRoute` 并调用 `MapNavigationPage::setRoute()`；
页面通过 `retryRequested(route, mode)` 向上层发送 `MAP_ROUTE_PLAN`，再调用
`setRoutePlan()` 显示服务端确认的路线数据。地图 Key 只放在服务端配置，不得放入
Qt 用户端源码或资源文件。
