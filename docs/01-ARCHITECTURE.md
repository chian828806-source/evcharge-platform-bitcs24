# 系统架构规范

## 1. 总体原则

本项目采用前后端分离、模块化、敏捷迭代的开发方式。

系统主要包括：

- Qt 用户端
- Qt 管理端
- Spring Boot 后端
- MySQL 数据库
- Web 数据可视化大屏
- Python 机器学习模块

所有模块围绕统一数据库结构和 RESTful API 协作。

核心原则：

1. 优先保证完整业务闭环，再增加附加功能。
2. 禁止各客户端绕过后端直接修改核心业务数据。
3. 所有公共数据结构、数据库字段和 API 接口必须统一。
4. 已确定的接口不得由个人随意修改。
5. 每天必须产生至少一个可运行、可集成版本。
6. Agent 生成代码必须符合项目规范后才能提交。
7. 不进行与项目目标无关的过度设计。

## 2. 系统数据闭环

最终系统必须形成真实数据闭环：

```text
Qt 用户端产生业务行为
        ↓
Spring Boot 处理业务
        ↓
MySQL 更新数据
        ↓
Qt 管理端读取变化
        ↓
Web 大屏读取统计数据
        ↓
ML 使用历史数据预测
        ↓
预测结果重新进入系统
```

项目评价标准不是单个页面数量，而是整个系统是否真正协同运行。

## 3. 模块职责

### 3.1 Qt 用户端

面向新能源汽车车主，负责用户充电业务流程，包括登录、附近充电站查询、地图导航、充电桩选择、订单创建、充电模拟、计费结算和未完成订单检查。

Qt 用户端不得直接访问 MySQL，必须通过后端 API 获取和修改业务数据。

### 3.2 Qt 管理端

面向平台运营管理人员，负责管理员登录、营收统计、充电桩状态统计、充电桩管理、充电站管理、用户管理、用户冻结/解冻和电桩远程重启模拟。

Qt 管理端不得直接访问 MySQL，必须通过后端 API 操作业务数据。

### 3.3 后端服务

后端采用 Spring Boot，负责统一业务逻辑、数据校验、事务处理、数据库访问封装和 REST API 输出。

所有客户端原则上通过统一后端 API 访问业务数据，不直接修改核心数据库。

### 3.4 MySQL 数据库

数据库负责存储平台核心业务数据，包括用户、管理员、充电站、充电桩、充电订单、充值记录、设备运行记录和机器学习预测结果。

数据库结构由 `database/schema.sql` 作为唯一正式版本。

### 3.5 Web 数据可视化大屏

Web 大屏使用 HTML、CSS、JavaScript 和 ECharts 展示用户规模、充电订单、充电量、营收数据、充电桩状态、充电站运行情况和负荷预测结果。

Web 大屏必须通过 HTTP API 获取数据，禁止直接连接 MySQL。

### 3.6 Python 机器学习模块

机器学习模块负责历史充电数据预处理、充电负荷预测、空闲充电桩数量预测、高峰充电时段预测，并将预测结果写入 `prediction` 表或通过后端接口提供给其他模块。

第一阶段优先实现：

```text
数据库 → 数据预处理 → 模型训练 → 预测 → prediction 表
```

不强制第一阶段部署独立在线推理服务。

## 4. 后端分层架构

Spring Boot 后端统一采用：

```text
Controller
    ↓
Service
    ↓
Mapper
    ↓
MySQL
```

推荐目录：

```text
backend/src/main/java/.../

├── common/
├── config/
├── controller/
├── service/
│   └── impl/
├── mapper/
├── entity/
├── dto/
├── vo/
├── exception/
└── util/
```

## 5. 技术栈

### Qt 客户端

- C++
- Qt
- Qt Widgets
- Qt Network
- Qt WebEngine
- Qt Charts
- 腾讯地图 Web API

HTTP 请求统一使用 `QNetworkAccessManager`。

JSON 数据统一使用：

- `QJsonDocument`
- `QJsonObject`
- `QJsonArray`

### 后端

- Java
- Spring Boot
- Maven
- MyBatis-Plus
- RESTful API
- OpenAPI / Swagger

推荐基础组件：

- Spring Web
- MyBatis-Plus
- MySQL Driver
- Lombok
- Validation
- SpringDoc OpenAPI

一周开发周期内暂不引入：

- Spring Cloud
- Redis
- Kafka
- RabbitMQ
- Nacos
- Elasticsearch
- 微服务网关
- 复杂权限框架

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
- XGBoost
- matplotlib

如后续需要深度学习，可根据实际情况增加 PyTorch。

## 6. 项目目录

本项目采用单 Git 仓库管理：

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

禁止以下无法明确含义的目录进入正式仓库：

```text
final/
final2/
new/
new2/
测试/
临时/
最新版/
真的最终版/
```
