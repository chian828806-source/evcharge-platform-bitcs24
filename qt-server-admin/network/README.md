# network — 过渡服务端通信层

本目录负责“请求如何进入、响应如何出去”，不实现业务规则和 SQL。

当前目录属于 `qt-server-admin/` 历史合并结构。目标结构下，这些服务端网络组件应迁移到 `qt-server/network/`；Qt 管理端应放在 `qt-admin/` 并作为 Socket 客户端接入。

## 文件说明

| 文件 | 功能 |
| --- | --- |
| socketserver.h/.cpp | 监听TCP端口并接受新连接 |
| clientsession.h/.cpp | 为每个连接保存独立缓冲区，解析请求并写回响应 |
| sessionmanager.h/.cpp | 创建随机Session并保存用户/管理员身份 |
| messagedispatcher.h/.cpp | 根据type查找Handler并检查访问角色 |
| dashboardwebsocketserver.h/.cpp | 接受大屏订阅并按topic推送JSON |
| network.pri | 把上述源码加入过渡服务端qmake工程 |

## 调用顺序

~~~text
QTcpServer
  → ClientSession
  → JsonLineCodec
  → RequestMessage
  → MessageDispatcher
  → 业务Handler / Service
  → ResponseMessage
  → QTcpSocket
~~~

## 线程边界

当前代码是网络骨架，所有QObject先运行在创建它们的线程中。后续引入Business Worker时，应通过Qt信号槽或队列投递业务任务，不能从Socket线程直接执行耗时SQL。

每个TCP连接拥有自己的 ClientSession 和 JsonLineCodec，因此不同客户端的半包缓存不会混在一起。
