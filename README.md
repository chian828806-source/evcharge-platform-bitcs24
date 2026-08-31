# EV Charging Service Platform

新能源汽车充电服务与智能运营平台

## 1. 项目简介

本项目面向新能源汽车充电服务场景，设计并实现一套集用户充电服务、充电设备管理、运营数据分析、大数据可视化与机器学习预测于一体的综合软件系统。

系统采用分层、模块化的设计思路，主要包括：

- Qt 充电用户端
- Qt PC 管理端
- 后端服务层
- MySQL 数据库
- Web 大数据可视化大屏
- Python 机器学习智能分析模块

项目目标不仅是实现各模块的独立功能，还需要建立完整的数据与业务闭环，使用户充电行为能够同步影响后台管理、数据统计、可视化展示以及机器学习分析结果。

---

## 2. 当前核心功能范围

### 2.1 Qt 用户端

面向新能源汽车车主，主要包括：

- 手机号登录与自动注册
- 用户信息维护
- 钱包充值
- 附近充电站查询
- 腾讯地图定位与导航
- 充电站及充电桩详情查看
- 充电桩选择
- 充电订单创建
- 充电过程模拟
- 计费与结算
- 未完成订单检查

### 2.2 Qt 管理端

面向充电平台运营管理人员，主要包括：

- 管理员登录
- 营收统计
- 近 7 日 / 30 日营收趋势
- 充电桩状态统计
- 充电桩管理
- 充电站管理
- 用户管理
- 用户冻结 / 解冻
- 电桩远程重启模拟

### 2.3 数据库

数据库负责存储和维护平台核心业务数据，包括但不限于：

- 用户信息
- 管理员信息
- 充电站信息
- 充电桩信息
- 充电订单
- 用户充值记录
- 设备运行记录
- 机器学习预测结果

具体数据库表结构将在项目设计书确认后统一设计并冻结。

### 2.4 Web 数据可视化大屏

使用 Web 技术与 ECharts 构建运营数据大屏，用于展示：

- 用户规模
- 充电订单
- 充电量
- 营收数据
- 充电桩状态
- 充电站运行情况
- 负荷预测结果
- 其他运营指标

### 2.5 机器学习智能分析

使用 Python 完成数据处理与机器学习分析，当前核心目标为：

- 历史充电数据预处理
- 充电负荷预测
- 空闲充电桩数量预测
- 高峰充电时段预测
- 预测结果存储及系统展示

后续可根据项目实际进度增加故障预测、智能推荐等扩展功能。

---

## 3. 系统总体架构

当前计划采用前后端分离的分层架构：

```text
Qt 用户端 ─────┐
               │
Qt 管理端 ─────┼──── REST API ──── 后端服务 ──── MySQL
               │
Web 大屏 ──────┘                         │
                                         │
                                  Python ML 模块
```

各客户端原则上通过统一后端 API 访问业务数据，不直接修改核心数据库。

机器学习模块可根据实际需要读取历史数据，并将预测结果写回数据库或通过后端接口提供给其他模块。

---

## 4. 当前技术栈

### Qt 客户端

- C++
- Qt
- Qt Widgets
- Qt Network
- Qt WebEngine
- Qt Charts

地图功能计划使用：

- 腾讯地图 Web API

### 后端

当前计划采用：

- Java
- Spring Boot
- Maven
- MyBatis-Plus
- RESTful API
- OpenAPI / Swagger

具体 JDK、Spring Boot 及相关依赖版本将在后端基础工程创建时统一确定并冻结。

### 数据库

- MySQL 8.x

### Web 大屏

- HTML
- CSS
- JavaScript
- ECharts

### 机器学习

- Python
- NumPy
- pandas
- scikit-learn

根据模型需求可进一步增加：

- XGBoost
- PyTorch

---

## 5. 仓库结构

本项目采用单 Git 仓库管理。

```text
ev-charging-platform/

├── backend/
│   └── Spring Boot 后端
│
├── qt-user/
│   └── Qt 用户端
│
├── qt-admin/
│   └── Qt 管理端
│
├── web-dashboard/
│   └── Web 数据可视化大屏
│
├── ml/
│   └── 机器学习模块
│
├── database/
│   ├── schema.sql
│   ├── init_data.sql
│   └── README.md
│
├── docs/
│   ├── 01-ARCHITECTURE.md
│   ├── 02-DEVELOPMENT-GUIDE.md
│   ├── 03-API.md
│   ├── 04-DATABASE.md
│   ├── 05-GIT-WORKFLOW.md
│   └── 06-AGENT-GUIDE.md
│
├── README.md
└── .gitignore
```

---

## 6. 开发方式

项目计划采用简化敏捷开发方式。

核心原则：

1. 优先保证完整业务闭环。
2. 每天进行短周期迭代。
3. 尽早联调，不在项目最后一天集中集成。
4. 每天结束时尽量保证 `develop` 分支存在可运行版本。
5. 所有成员遵循统一数据库、API、Git 与代码规范。
6. Agent 可参与编码与调试，但不得自行改变系统架构和公共契约。

---

## 7. Git 工作流

长期分支：

```text
main
develop
```

开发分支统一采用：

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

基本流程：

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

原则上禁止直接在 `main` 分支开发业务功能。

---

## 8. Commit 规范

统一采用：

```text
<type>: <description>
```

常用类型：

```text
feat:
fix:
refactor:
docs:
test:
style:
chore:
```

示例：

```text
feat: add user phone login API

feat: implement nearby station page

fix: prevent duplicate charging orders

docs: update API documentation
```

---

## 9. 公共开发约束

数据库结构、API 接口及公共状态定义属于项目公共契约。

未经讨论不得自行修改：

- 数据库表名
- 数据库字段
- API URL
- HTTP Method
- 请求字段
- 响应字段
- 公共状态值

如确需修改，应先确认影响范围，再同步修改相关文档与代码。

---

## 10. Agent 使用原则

本项目允许使用 Agent 辅助：

- 编写代码
- 修改 Bug
- 补充测试
- 分析现有代码
- 完善文档
- 优化局部实现

但 Agent 不得自行：

- 修改系统总体架构
- 修改数据库公共结构
- 修改既有 API 契约
- 引入未经确认的新框架
- 大规模重构无关模块
- 删除现有有效功能

所有 Agent 修改均应遵循项目现有文档和目录结构。

---

## 11. 当前项目状态

当前阶段：

```text
项目初始化
```

已初步确定：

- 单 Git 仓库
- 前后端分离
- Qt 双客户端
- Spring Boot 后端方向
- MySQL 数据库
- ECharts Web 大屏
- Python 机器学习模块
- REST API 数据交互
- 敏捷迭代开发
- Agent 协作开发

等待进一步确认：

- 正式项目设计书
- 最终需求范围
- 数据库详细设计
- API 详细设计
- 5 人具体分工
- 后端最终版本
- 认证方案具体实现
- 一周 Sprint 任务拆分

---

## 12. 后续开发流程

项目设计书确认后，按照以下顺序推进：

```text
需求分析
    ↓
需求冻结 V1
    ↓
数据库设计
    ↓
API 设计
    ↓
系统模块拆分
    ↓
5 人任务分工
    ↓
基础工程创建
    ↓
并行开发
    ↓
每日集成
    ↓
系统联调
    ↓
测试与优化
    ↓
项目答辩
```

---

## 13. 项目目标

最终系统应形成完整业务数据闭环：

```text
用户在 Qt 用户端产生充电行为
            ↓
后端处理业务逻辑
            ↓
MySQL 保存业务数据
            ↓
Qt 管理端获取最新运营数据
            ↓
Web 大屏展示统计指标
            ↓
机器学习模块使用历史数据进行分析
            ↓
预测结果重新反馈到系统
```

项目最终评价重点不仅是单个功能是否实现，更重要的是各模块能否通过统一的数据与接口形成完整、稳定、可演示的软件系统。