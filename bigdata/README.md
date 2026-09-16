# Phase 2 Big Data Foundation

本目录是第二阶段大数据子系统的唯一入口。它以一期 SQLite 的业务语义为输入，新增
`HDFS → PySpark → Spark SQL → Flask → Vue + ECharts → Spark MLlib` 分析链路；不替换
Qt/C++ 服务端，也不修改一期业务状态机。

## 目录与责任

| 目录 | Owner | 交付边界 |
| --- | --- | --- |
| `environment/`、`data-generator/`、`ingestion/`、`spark/quality/`、`spark/dwd/` | B | 从 Hadoop/HDFS、Raw/ODS 到质量治理和 DWD |
| `spark/warehouse/`、`api/` | C | SparkSQL 的 DWS/ADS、运营分析与 Flask API |
| `ml/` | E | Spark MLlib 训练、评估和预测结果 |
| `contracts/`、`integration/`、`scripts/`、`web-dashboard/src/core/`（规划） | A | 架构/Contract、Dashboard Core、编排、验收与集成 |
| `web-dashboard/src/views/`、`components/`、`charts/`、`styles/`（规划） | D | Vue 页面、ECharts、交互、布局与视觉表现 |

正式 UrbanEV 演示入口、下载说明和字段映射见
[urbanev/README.md](urbanev/README.md)。通用随机数据生成器只保留作 Contract 和 DQ 回归夹具；
UrbanEV 的负荷、电量、价格仍来自官方 Raw，仅缺失的用户/订单关系由固定 seed 补全并明确标记。
实施代码必须遵守 [contracts/README.md](contracts/README.md)。

## 数据流

```text
业务导出/UrbanEV Raw → HDFS ODS → 质量检测与清洗 → DWD → DWS → ADS → C: Flask
                                                     │                 ↓
                                                     └→ ML 特征 → E: MLlib → ads_prediction
                                                                       ↓
                                      Qt WebSocket ─→ A: Dashboard Core → D: Vue/ECharts UI
```

Dashboard V2 不是单人模块：A 仅负责 API Client、Store、Adapter、Mock/Real、Realtime 与
ViewModel 等非视觉数据层；D 仅负责 Vue 页面、组件、ECharts、CSS、布局和交互。D 只能消费
A 公布的 Store / ViewModel，不能直接调用 Flask 或重新计算正式 KPI。

执行顺序、验收和边界见 [docs/bigdata/README.md](../docs/bigdata/README.md)。

## Module Responsibility
作为第二阶段目录、角色、数据流和跨模块边界的总入口。
## Owner
A 维护总体架构与 Contract；各子目录由上表 Owner 负责。
## Input
一期业务语义、经版本化的 Raw/DWD/DWS/ADS/ML/API Contract。
## Output
可供五个实施分支共同遵守的 Foundation 基线。
## Allowed Dependencies
`docs/bigdata/`、`bigdata/contracts/` 与各模块公开输出。
## Forbidden Dependencies
跨 Owner 内部实现、一期业务写路径与任何未获批 Contract 变更。
## Public Contract
[contracts/README.md](contracts/README.md) 以及 22–25 号文档。
## Future Implementation
分别在已规划的五条 feature 分支中实现，不在 Foundation 分支实现 Hadoop/Spark/Flask/Vue/ML。
## Acceptance
目录 Owner、输入输出、禁止依赖和 Dashboard Core/UI 边界均可由独立成员执行。
