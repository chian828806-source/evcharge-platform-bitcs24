# Phase 2 Big Data Foundation

本目录是第二阶段大数据子系统的唯一入口。它以一期 SQLite 的业务语义为输入，新增
`HDFS → PySpark → Spark SQL → Flask → Vue + ECharts → Spark MLlib` 分析链路；不替换
Qt/C++ 服务端，也不修改一期业务状态机。

## 目录与责任

| 目录 | Owner | 交付边界 |
| --- | --- | --- |
| `environment/`、`data-generator/`、`ingestion/` | B | 环境、可复现模拟数据和 ODS 落地 |
| `spark/quality/`、`spark/dwd/` | C | 质量检测、报告和 DWD 清洗 |
| `spark/warehouse/`、`api/` | D | SparkSQL 的 DWS/ADS 与 Flask API |
| `ml/` | E | Spark MLlib 训练、评估和预测结果 |
| `contracts/`、`integration/`、`scripts/` | A | 公共契约、编排、验收与集成 |

目录当前只提供接口和文档骨架。实施代码必须遵守 [contracts/README.md](contracts/README.md)，
并在各自 feature 分支完成后才进入 `develop`。

## 数据流

```text
模拟/导出 CSV → HDFS ODS → 质量检测与清洗 → DWD → DWS → ADS
                                                     ├→ Flask → Vue/ECharts
                                                     └→ ML 特征 → MLlib → ads_prediction
```

执行顺序、验收和边界见 [docs/bigdata/README.md](../docs/bigdata/README.md)。
