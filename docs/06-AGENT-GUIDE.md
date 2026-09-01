# Agent 协作规范

本文档定义团队使用 AI Agent 的输入上下文、允许范围、禁止事项和验收要求。

## 1. 基本原则

Agent 可以参与编写代码、修改 Bug、补充测试、分析现有代码、完善文档和优化局部实现。

Agent 不具有自行修改项目架构和公共契约的权限。

公共契约包括：

- SRS 需求编号和需求含义；
- Socket Message Type / 请求字段 / 响应字段；
- WebSocket 大屏 Message Type / 请求字段 / 推送字段；
- SQLite 表和字段；
- 状态枚举；
- 统计口径；
- 模块边界：`qt-user`、`qt-admin`、`qt-server`、`shared`、`web-dashboard`、`ml`、`database`；
- 技术栈和依赖。

## 2. Contract Read-First

任何 Agent 开始编码前，必须先阅读：

- `docs/00-SRS-V1.0.md`
- `docs/01-ARCHITECTURE.md`
- `docs/02-DEVELOPMENT-GUIDE.md`
- `docs/03-API.md`
- `docs/04-DATABASE.md`
- 当前模块 README 或已有代码

涉及远程重启扩展时，再阅读 `docs/07-DEVICE-PROTOCOL.md`。

## 3. 给 Agent 的必要上下文

每次任务至少提供：

1. 需求编号。
2. 当前技术栈：Qt/C++、Socket、SQLite、QtSql。
3. 涉及 Socket 消息。
4. 是否涉及 WebSocket 大屏消息。
5. 涉及模块边界。
6. 涉及 SQLite 表。
7. 涉及状态枚举。
8. 本次任务边界。
9. 禁止修改的文件或契约。
10. 验收条件。
11. 可运行或测试命令。

推荐 Prompt：

```text
本次任务对应 SRS: FR-C-007。
只实现订单结算逻辑。
技术栈为 Qt/C++ + QtSql(SQLite) + Socket。
不得引入 Spring Boot、MySQL 或 REST 主接口。
不得修改既有 Socket 消息类型。
不得修改数据库字段。
不得把管理端界面逻辑写入服务端，或让管理端直接访问SQLite。
不得把 `qt-server` 与 `qt-admin` 重新合并为一个大模块。
不得重构无关模块。
完成后说明修改文件和验证结果。
```

## 4. 禁止事项

Agent 不得自行：

- 引入 Spring Boot、MySQL 或 REST 主业务接口；
- 修改系统总体架构；
- 修改 SRS 已冻结需求；
- 修改 Socket 契约；
- 修改 SQLite 公共结构；
- 修改公共状态枚举；
- 大规模重构无关模块；
- 删除现有有效功能；
- 绕过 Qt/C++ 服务端让用户端或管理端直接操作 SQLite；
- 把 `qt-server` 与 `qt-admin` 重新合并为一个大模块；
- 把敏感信息写入仓库。

## 5. 输出要求

Agent 完成任务后必须说明：

- 修改了哪些文件；
- 对应哪些需求编号；
- 是否修改公共契约；
- 执行了哪些测试或运行命令；
- 仍存在什么风险。

## 6. Diff Review

提交前必须检查：

- 是否只改了本任务相关文件；
- 是否误删文件；
- 是否改动敏感配置；
- 是否引入临时文件；
- 是否包含无关格式化；
- 是否包含调试输出。

## 7. Linux 虚拟机协作所需信息

进入编码阶段前，请提供：

- GitHub 仓库地址；
- 默认分支和开发分支策略；
- 虚拟机 IP；
- SSH 端口；
- SSH 用户名；
- SSH 登录方式；
- Ubuntu 版本；
- Qt Creator / Qt 版本；
- C++ 编译器版本；
- 项目在虚拟机中的目标目录；
- 是否允许安装 Qt 相关依赖、创建目录、运行程序和执行测试；
- 腾讯地图 Key 的配置方式；
- 每次任务的验收条件。

不再需要 JDK、Maven、MySQL 作为主环境信息。
