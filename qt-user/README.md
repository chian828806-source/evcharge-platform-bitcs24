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
| U01 | 手机号登录 | 已完成 UI、格式校验和真实登录请求入口 |
| U02 | 首页/附近充电站 | 已完成定位区、站点卡片和推荐标识 |
| U03 | 充电站详情 | 已完成站点摘要、电桩状态和选择交互 |
| U04 | 当前充电/订单处理 | 已完成 CREATED、CHARGING、PENDING_PAYMENT 等展示状态 |
| U05 | 我的 | 已完成资料、头像选择、钱包、充值和订单列表 |
| U06 | 地图导航 | 已完成路线模式和 QWebEngineView 接入占位区 |

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

## 待后端接口确认

- 冻结用户登录和活动订单收尾的响应字段；
- 头像内容的下载或返回方式；
- 充电进度的时间精度和刷新方式；
- 地址解析与腾讯地图 Key 的职责边界。


## 页面调用原则

页面只调用 `SocketClient::sendRequest`，不直接操作 `QTcpSocket`。页面保存 `sendRequest` 返回的 `requestId`，并在 `responseReceived` 信号中按 `requestId` 匹配响应。

登录成功后，页面所属的Session对象应保存服务端返回的sessionId；之后的受保护请求都传入该值。
