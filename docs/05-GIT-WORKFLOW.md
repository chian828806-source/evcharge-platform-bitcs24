# Git 工作流规范

## 1. 仓库策略

本项目采用单 Git 仓库管理。

原则上禁止直接在 `main` 分支开发业务功能。

## 2. 长期分支

长期分支：

```text
main
develop
```

`main` 只存放稳定、可以演示的版本。

`develop` 用于每天集成开发成果，并尽量保持可运行。

## 3. 开发分支

个人开发统一使用：

```text
feature/*
```

例如：

```text
feature/user-login
feature/station-list
feature/admin-dashboard
feature/load-prediction
```

Bug 修复使用：

```text
fix/*
```

例如：

```text
fix/order-settlement
```

禁止使用以下无意义分支名：

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
合并 develop
   ↓
集成测试
   ↓
main
```

或简化表示为：

```text
feature/*
    ↓
develop
    ↓
测试
    ↓
main
```

## 5. 合并规范

任何人禁止直接在 `main` 上开发业务功能。

`main` 合并由组长负责。

每天至少一次 `develop` 集成。

发生冲突时，优先由代码原作者解决，不允许为了快速合并直接删除另一方代码。

## 6. Commit 规范

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
feat: add user phone login API
feat: implement nearby station page
fix: prevent duplicate unfinished orders
refactor: extract Qt API client
docs: update charging API documentation
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

## 7. 每日集成要求

每天结束前进行一次集成。

标准：

```text
每天结束时 develop 必须处于能够运行的状态。
```

一个功能达到 Done 后，应完成提交并合并到 `develop`。
