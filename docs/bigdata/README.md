# 第二阶段大数据平台基线

本目录冻结 Phase 2 的范围、分层、公共接口和验收依据。它补充而不替代一期文档；一期的
Socket、SQLite 业务规则、WebSocket 和 sklearn 基线仍由既有文档维护。

| 文档 | 用途 |
| --- | --- |
| [20-BIGDATA-SRS.md](20-BIGDATA-SRS.md) | 可验收需求与边界 |
| [21-BIGDATA-ARCHITECTURE.md](21-BIGDATA-ARCHITECTURE.md) | 架构、依赖墙和 Owner |
| [22-DATA-CONTRACT.md](22-DATA-CONTRACT.md) | Raw/ODS 数据集和质量规则 |
| [23-WAREHOUSE-DESIGN.md](23-WAREHOUSE-DESIGN.md) | ODS-DWD-DWS-ADS 血缘与口径 |
| [24-DASHBOARD-API.md](24-DASHBOARD-API.md) | Flask 与 Vue 的 REST 契约 |
| [25-ML-CONTRACT.md](25-ML-CONTRACT.md) | MLlib 特征、输出和评价 |
| [26-OWNERSHIP-GIT.md](26-OWNERSHIP-GIT.md) | 五人边界、分支和 CCR |
| [27-BIGDATA-ACCEPTANCE.md](27-BIGDATA-ACCEPTANCE.md) | 演示与验收证据 |

本基线的版本为 `v0.1-foundation`。后续实现只能扩展已定义契约，改变其含义必须走 CCR。

最终 Ownership：A 为 Architecture / Dashboard Core / Integration；B 为 Data Pipeline 到 DWD；
C 为 Warehouse / Analytics / Flask API；D 为 Dashboard UI / Visualization；E 为 Spark MLlib。
其中 Dashboard 的公共 UI Contract 是 A 与 D 唯一共享边界。
