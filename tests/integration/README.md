# system integration tests

这里验证的是单元测试无法覆盖的服务端装配边界：测试启动真实的
`evcharge-qt-server` 进程，并经由 `QTcpSocket` 发送 JSON Lines 请求。这样可以
覆盖 SocketServer、ClientSession、Dispatcher、各 Registry、Handler、Service、Repository
以及 SQLite 的完整链路，而不是直接调用 Handler 或 Dispatcher。

三个典型用例分别是：

1. **Unified server startup / composition smoke**：以由 `schema.sql` 和
   `init_data.sql` 初始化的专属 SQLite 启动服务端，确认监听、连接和退出清理。
2. **User/Admin routing boundary**：真实 User Session 调用 `ADMIN_STATION_LIST`
   必须得到协议既有的 `4003`；真实 Admin Session 对同一路由得到成功响应。
3. **Shared database cross-module**：Admin 通过 `ADMIN_STATION_CREATE` 写入站点，
   User 再经 `STATION_LIST_NEARBY` 读取该站点；不会用直接 SQL 查询替代用户 API。

## 构建和运行

先由顶层工程构建服务端和测试工程，或分别构建：

```bash
qmake6 evcharge-platform.pro
make -j2
./system-integration-tests
```

测试会自动寻找同一次构建生成的 `evcharge-qt-server`。如果构建布局不同，可显式设置
`EVCHARGE_SERVER_BINARY` 为该可执行文件的绝对路径后运行测试。

每项测试会新建 `QTemporaryDir`，在其中执行仓库的 `database/schema.sql` 与
`database/init_data.sql`，并向服务端传递临时数据库、头像目录和两个临时端口。测试结束
会停止子进程并由临时目录清除数据库，因此绝不会读写仓库的 `database/evcharge.db`。
