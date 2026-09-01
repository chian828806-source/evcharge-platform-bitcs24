# qt-admin — Qt 管理员端

`qt-admin` 是与 `qt-user`、`qt-server` 分离运行的管理员客户端应用目录。它负责管理员登录、运营统计、站点、电桩、用户和远程重启界面，不承载服务端监听、业务事务或 SQLite 访问。

| 路径 | 功能 |
| --- | --- |
| `network/` | 管理员端 TCP 连接、请求发送和响应接收 |
| `qt-admin-network.pro` | 供后续 Qt Widgets 管理界面链接的 qmake 静态库工程 |

构建网络库：

```bash
mkdir -p /tmp/evcharge-build/admin-network
cd /tmp/evcharge-build/admin-network
qmake6 /home/bit/workspace/evcharge-platform-bitcs24/qt-admin/qt-admin-network.pro
make -j2
```

后续管理界面应持有一个共享的 `AdminSocketClient`，以 `requestId` 关联请求与响应。界面不直接操作 `QTcpSocket`，也不直接读取数据库。
