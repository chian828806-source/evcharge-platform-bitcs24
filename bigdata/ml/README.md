# Spark MLlib

## Module Responsibility

使用完整数据流水线发布的 `dws_station_hour` 完成特征工程、模型训练、评价和
`1h/6h/24h` 预测。ML 不再直接读取 UrbanEV、Raw、ODS 或 DWD。

## Owner

E（Spark MLlib）。

## Input

- `dws_station_hour`：C 的正式 DWS，或端到端 Demo 的临时 DWS 桥接输出。
- 同一 `batch_id` 的质量报告：用于把 B 的 DQ 统计传递给演示 API 和大屏。

输入字段：

```text
station_id
station_name
hour_start
total_pile_count
session_starts
energy_kwh
charging_pile_minutes
average_occupied_count
average_available_count
station_load
batch_id
source_type
dt
```

## Output

```text
bigdata/runtime/demo/
├── dws_station_hour/     # 本次训练输入快照
├── models/               # 1h/6h/24h Spark MLlib 模型
├── ads_prediction/       # E 交给 C 发布的预测 Parquet
├── training_report.json  # 时间切分、特征、指标和 baseline 对比
└── dashboard.json        # Flask Demo 可直接读取的联调快照
```

## 单一流水线 Demo

在项目根目录运行：

```bash
bash bigdata/integration/run_full_demo.sh
```

它按同一个批次依次执行：

```text
B: generator → ODS → DQ → DWD
Demo bridge: DWD → dws_station_hour
E: DWS → features → MLlib → ads_prediction
Flask/Dashboard: 读取 dashboard.json
```

`build_demo_dws.py` 只是 C 的正式数仓作业尚未提交时使用的直通桥。C 发布正式
`dws_station_hour` 后，编排脚本只需替换 DWS 生产步骤，ML 作业及其字段不需要修改。

也可以单独重跑 ML：

```bash
spark-submit bigdata/ml/jobs/station_load_demo.py \
  --dws-input hdfs:///evcharge/dws/dws_station_hour/batch=BD-20260915-DEMO \
  --quality-report hdfs:///evcharge/quality/reports/dt=2026-09-15/batch=BD-20260915-DEMO
```

## Boundary with B and C

- B 的生成器、ODS、DQ 和 DWD 实现保持原样，ML 只消费其公开产物。
- Demo 桥不修改上游分区，也不冒充 C 的正式 DWS 实现。
- E 只发布模型、评估报告与预测结果，不实现 Flask 或 Vue。
- C 可读取 `ads_prediction`，不需要读取模型文件。

## Acceptance

DWS schema、单批次、主键和范围校验通过；质量报告批次与 DWS 批次一致；预测范围、
指标、baseline 与版本信息齐全；`prediction_time` 表示未来目标小时。
