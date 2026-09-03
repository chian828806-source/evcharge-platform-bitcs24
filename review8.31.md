# 项目开发文档对照review

## 先上总体结论

项目要求书提及的主要技术路线：

- 用户端：Linux + Qt；
- PC 服务与管理端：Linux + Qt；
- 主要开发语言：C++；
- 数据库：QtSql + SQLite，Qt 驱动名为 `QSQLITE`；
- 通信：Socket；
- 主程序：多线程；
- 管理端图表：QChart；
- 导航：腾讯地图 Web API + QWebEngineView；
- 大屏：Web + ECharts；
- 机器学习：完成负荷、空闲桩和高峰时段预测。

因此，现有文档中的 Spring Boot + MySQL 不适合作为项目主架构。FastAPI 不能替代 Qt/C++ 主服务，但如果只是帮助机器学习模块提供预测结果，可在不影响主要要求的前提下使用（保险起见，可以问老师）。

暂时统一为以下结构：

```text
Qt 用户端（QTcpSocket）
          │
          ▼
Qt/C++ PC 服务与管理端
（QTcpServer + 管理界面 + QChart + 多线程）
          │
          ▼
QtSql + SQLite（QSQLITE）
          │
          ├── 腾讯地图
          ├── Web + ECharts 大屏
          └── 机器学习预测模块
```

## 问题清单

* Tip: Decision 和 Owner 留给组内评审及分工时填写。

| ID | Severity | Location | Issue | Reviewer | Decision | Action | Owner |
| --- | --- | --- | --- | --- | --- | --- | --- |
| R-001 | Blocker | `README.md` §2～§4；`00-SRS` §1～§2；`01-ARCHITECTURE` §2～§5 | 文档把 Spring Boot 作为业务中心，把两个 Qt 程序都写成客户端。要求书明确 PC 服务器端同样使用 Qt，核心逻辑使用 C++。 | wyf |  | 删除 Spring Boot 主后端设计，改为 Qt/C++ PC 服务与管理端，统一负责 Socket 服务、业务处理和管理界面。 |  |
| R-002 | Blocker | `README.md` §2.4/§4；`00-SRS` OI-001；`01-ARCHITECTURE` §5.4；`04-DATABASE` | 文档将 MySQL 定为主库，把 SQLite 当成待确认的本地缓存，与要求书指定 QSQLlite 不符。 | wyf |  | 主库改为 SQLite，通过 QtSql 的 `QSQLITE` 驱动访问；删除 MySQL 主库方案和“SQLite 是否必需”的待确认项。 |  |
| R-003 | High | `02-DEVELOPMENT-GUIDE` §2～§9/§11；`03-API` | 开发规范主要是 Java/Spring 内容，如 Controller、Mapper、MyBatis、`BigDecimal`、SpringDoc 和 Java Socket，不能直接指导 Qt/C++ 开发。REST 内容也多于 Socket。 | wyf |  | 按 Qt/C++ 重写开发规范；接口文档以 Qt Socket 消息为主，补全登录、查站、充电、结算和管理操作的请求与响应。Web 大屏接口放在次要章节。 |  |
| R-004 | High | `00-SRS` NFR-THR-001；`01-ARCHITECTURE` §6；`02-DEVELOPMENT-GUIDE` §11 | 要求书写明主程序应为多线程结构，并提到 pthread；当前文档只写“UI 不阻塞”，没有说明 Qt 服务端如何分线程。 | wyf |  | 补充 Qt 服务端线程设计，例如连接处理、业务处理、充电计时和界面刷新。向老师确认是否必须直接使用 pthread，还是 `QThread` 也可以。 |  |
| R-005 | High | `04-DATABASE`；`02-DEVELOPMENT-GUIDE` §5 | 数据库规范使用 MySQL 的字段类型和 Java 数据类型，没有 SQLite 建表语句、Qt 查询示例和多线程访问规则。 | wyf |  | 改为 SQLite 规范；金额建议按“分”保存，时间格式保持统一；每个数据库线程使用自己的连接，写操作集中处理。 |  |
| R-006 | High | `README.md` §1/§9；`00-SRS` §8；`06-AGENT-GUIDE` §11 | 要求书规定 VMware 17、Ubuntu 22.04+、Qt Creator 6.2+，文档却写成推荐或待确认，并把 JDK、Maven、MySQL 列为主要环境。 | wyf |  | 固定 Qt 项目的运行和开发环境；列出 Qt Widgets、Network、Sql、Charts、WebEngine。JDK、Maven、MySQL 从必需环境中删除。 |  |
| R-007 | High | `README.md` §2.7；`00-SRS` §3.7；`07-DEVICE-PROTOCOL` | 文档增加了完整设备网关、心跳、遥测和串口协议。原要求只明确“模拟发送远程重启指令”，当前设计范围过大，而且仍依赖 Spring Boot 和 MySQL。 | wyf |  | 必做部分只保留远程重启的指令、返回结果和状态变化。完整设备协议改为扩展内容，不能代替用户端与 Qt 服务端之间的 Socket 通信。 |  |
| R-008 | High | `00-SRS` §3.6/§10.3；`README.md` §2.6 | 机器学习在要求书中属于基本功能，不能像原 `review.md` 建议的那样整体改为 OPTIONAL。但现文档没有确定最小数据、模型输出和演示方法。 | wyf |  | 保留负荷、空闲桩和高峰时段预测；使用固定演示数据，明确输入字段、预测结果和基本评价方法。天气、节假日等没有数据的特征可先向老师确认。 |  |
| R-009 | Medium | `README.md` §2.2/§3；`00-SRS` §2.3/§3.4；`01-ARCHITECTURE` §1 | 文档称“Qt 管理端”，感觉容易让人把它做成只发送请求的客户端？同时规定 Qt 管理端不能访问数据库，与它承担服务器职责冲突。 | wyf |  | 统一名称为“Qt PC 服务与管理端”（有待进一步讨论）。只有 Qt 用户端不能直接访问数据库，PC 服务端可以通过 QtSql 读写 SQLite。 |  |
| R-010 | Medium | `00-SRS` §3.3/§5；`02-DEVELOPMENT-GUIDE` §4；`04-DATABASE` §9 | 预约、开始充电、停止、计费、结算涉及订单和电桩多个状态，但文档没有把状态变化、失败恢复和计费规则讲清楚。 | wyf |  | 增加一张状态变化表；规定由服务端统一计时和计费，并说明余额不足、连接中断、重复结算时如何处理。 |  |
| R-011 | Medium | `00-SRS` FR-U-004/007/011；`02-DEVELOPMENT-GUIDE` §9；`03-API` §11～§12 | 头像上传仍按 REST 文件上传设计，地图 Key 仍假定由 Spring 后端管理，迁移到 Qt 后没有对应方案。 | wyf |  | 头像由 Qt 客户端选取并传给 Qt 服务端，数据库保存相对路径；Qt 服务端调用地图接口，用户端用 `QWebEngineView` 导航。 |  |
| R-012 | Medium | `00-SRS` FR-U-008/009、FR-A-002～004；状态模型 | 距离、空闲桩数量、今日/本月营收、7/30 日趋势及电桩状态占比的计算口径没有统一，不同人可能算出不同结果。 | wyf |  | 明确距离计算和排序规则、哪些状态算空闲、营收以已结算订单为准、日期范围是否包含当天，以及扩展状态如何计入统计。 |  |
| R-013 | Medium | `00-SRS` §3.5/§10.3；`README.md` §2.5 | Web 大屏的数据来源写死为 Spring REST；修改主架构后，大屏和预测结果如何取数没有方案，也缺少固定演示数据。 | wyf |  | 选择一种简单方式：Qt 服务端提供 HTTP/JSON，或定时导出 JSON 文件。准备固定数据和演示步骤，保证大屏能展示统计和预测结果。 |  |
| R-014 | Medium | `00-SRS` NFR-ERR-001/NFR-REL-001；`02-DEVELOPMENT-GUIDE` §8；`03-API` §2 | 错误处理示例主要是 Spring 写法；Qt 侧也没有完整说明断线、消息不完整、数据库失败、地图失败、余额不足等情况。 | wyf |  | 建立简单的“错误情况—提示信息—处理办法”表；Socket 补充消息长度、拆包、超时、重连和错误码规则。 |  |
| R-015 | Medium | `00-SRS` §9；`03-API` §7 | 认证方案使用 JWT 和 BCrypt，带有明显的 Java Web 方案特点| wyf |  | Qt Socket 登录后使用随机会话编号；管理员密码使用可靠的密码哈希方式保存。具体实现可以根据后续开发环境选择，不必强制 JWT。 |  |
| R-016 | Low | `README.md` 标题/§5；全部文档 | 正式项目名、目录结构和人员职责没有完全对齐要求书：项目被改名，目录仍以 Java 后端为中心，没有 PM/TL/PRL/SCML/PE 的分工。 | wyf |  | 正式名称恢复为“东软电动汽车充电桩应用管理平台”；目录改为 `qt-user/`、`qt-server-admin/`、`database/`、`web-dashboard/`、`ml/`；补充角色和负责人。 |  |

## 我目前可以想到的开发统一原则

1. Qt/C++、SQLite、Socket 和多线程是主线，不能被 Java 或 Python 服务替代。
2. Spring Boot、MySQL 从主架构删除；FastAPI 只能作为可选的机器学习辅助工具。
3. 机器学习预测保留为基本功能；完整物联网设备协议属于扩展内容。
4. 先修改 README、SRS 和架构，再修改数据库、接口和开发规范，最后同步 Git 与 Agent 文档。
5. 文档中的 `QT`、`QSQLlite`、`QSQLite` 统一写为 Qt、SQLite、QtSql（`QSQLITE` 驱动）。

## 可以向老师确认的点

- 多线程是否必须直接调用 pthread，还是 Qt 的 `QThread` 即可；
- 机器学习需要达到什么最低精度，是否提供统一数据集；
- Web 大屏是否允许读取 Qt 服务端导出的 JSON，还是必须使用网络接口。
