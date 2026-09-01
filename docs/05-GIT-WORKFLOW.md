# Git 工作流规范

本文档定义一周敏捷开发中的分支、提交、Review、合并和集成要求。

## 1. 仓库策略

本项目采用单 Git 仓库。

建议将 `evcharge-platform` 单独初始化为 Git 仓库，不要把桌面或其他无关目录纳入版本管理。

## 2. 长期分支

长期分支：

```text
main
develop
```

`main` 只存放稳定、可以演示的版本。

`develop` 用于每天集成开发成果，并尽量保持可运行。

## 3. 开发分支

功能开发：

```text
feature/<short-name>
```

Bug 修复：

```text
fix/<short-name>
```

文档任务：

```text
docs/<short-name>
```

示例：

```text
feature/user-login
feature/station-list
feature/admin-dashboard
feature/load-prediction
fix/order-settlement
docs/srs-v1
```

禁止：

```text
miao
test
new
final
abc
```

## 4. 基本流程

```text
develop
   ↓
feature/xxx
   ↓
开发与自测
   ↓
同步 develop
   ↓
Pull Request / Review
   ↓
合并 develop
   ↓
集成测试
   ↓
main
```

原则上禁止直接在 `main` 开发业务功能。

## 5. Commit 规范

统一格式：

```text
<type>: <description>
```

允许类型：

```text
feat:
fix:
refactor:
docs:
style:
test:
chore:
```

示例：

```text
feat: add user phone login message
feat: implement nearby station page
fix: prevent duplicate unfinished orders
refactor: extract socket network client
docs: update SRS v1
```

禁止：

```text
update
修改
123
final
改好了
aaa
```

## 6. Pull Request 规范

每个 PR 至少说明：

- 对应 SRS 需求编号。
- 本次完成内容。
- 涉及模块。
- 是否修改 Socket 业务消息 / WebSocket 大屏消息 / ML 数据输入输出 / 数据库 / 状态枚举 / 设备协议。
- 是否影响 `qt-user`、`qt-admin`、`qt-server`、`shared` 的模块边界。
- 已执行的测试。
- 未完成或需注意的问题。

若没有使用 GitHub PR，也必须在合并前按上述内容进行组内 Review。

## 7. Code Review 重点

Review 优先检查：

- 是否符合 SRS。
- 是否私自修改公共契约。
- 是否破坏现有功能。
- 是否有基本异常处理。
- 是否存在重复逻辑或明显过度设计。
- 是否能运行。
- 是否更新相关文档。
- 是否把服务端、管理端和用户端职责混写。

公共契约变更必须重点审查：

- Socket Message Type / 请求字段 / 响应字段
- WebSocket 大屏 Message Type / 请求字段 / 推送字段
- ML 输入输出文件或表字段
- 数据库表和字段
- 状态枚举
- 设备协议消息

## 8. 合并规范

`main` 合并由组长负责。

每天至少一次 `develop` 集成。

发生冲突时，优先由代码原作者解决，不允许为了快速合并直接删除另一方代码。

禁止把不能编译或不能启动的代码合并到 `develop`。

## 9. 集成窗口

建议每日固定两个集成窗口：

- 中午或下午：小范围接口联调。
- 晚上：合并到 `develop` 并保证可运行。

第 7 天不能作为第一次系统联调日。

## 10. Hotfix

若演示前 `main` 出现必须修复的问题：

```text
main
   ↓
fix/<issue>
   ↓
测试
   ↓
main
   ↓
develop
```

Hotfix 只修复演示阻塞问题，不做新功能。

## 11. Agent 生成代码提交

Agent 生成或修改的代码必须由人类成员 Review 后提交。

提交前必须检查：

- Diff 是否只包含本任务相关文件。
- 是否改动公共契约。
- 是否新增未经确认依赖。
- 是否通过基本运行或测试。
