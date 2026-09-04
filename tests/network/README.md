# network — 通信层测试

本目录使用一个轻量控制台测试程序验证通信层，不依赖Qt界面和真实数据库。

## 当前覆盖

- 一条消息分多次到达时等待完整换行。
- 多条消息一次到达时正确拆成多帧。
- RequestMessage公共字段解析。
- ResponseMessage公共响应格式。
- 缺少Session时拒绝受保护消息。
- 用户Session可以进入用户路由。
- 管理员Session不能冒充普通用户。
- 31种TCP消息和4个大屏topic全部登记。
- 用户端消息、管理端消息和服务端处理入口的角色边界需要在后续端到端测试中继续补齐。

## 运行

~~~bash
qmake6 /home/bit/workspace/evcharge-platform-bitcs24/tests/network/network-protocol-tests.pro
make -j2
./network-protocol-tests
~~~

程序输出每项PASS/FAIL，并以TOTAL_FAILURES汇总。任何失败都会返回非零退出码。
