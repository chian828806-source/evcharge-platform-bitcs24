# B 数据平台运行手册

本手册只覆盖 B 的职责：环境检查、模拟 Raw 数据、HDFS ODS 摄取、PySpark 质量检测和 DWD 发布。

## 1. 虚拟机准备

在 Hadoop 虚拟机中进入项目目录：

```bash
cd ~/EVCharge
bash bigdata/environment/manage_services.sh start all
bash bigdata/environment/check_environment.sh
```

如果当前机器只配置了 HDFS、没有 ResourceManager/NodeManager，UrbanEV 本地演示可改用
`manage_services.sh start hdfs`；B 的正式五服务验收仍必须使用 `all`。

如果 HDFS 显示 Safe Mode 导致无法写入，先确认状态：

```bash
hdfs dfsadmin -safemode get
```

只有确认 DataNode 正常但 Safe Mode 长时间未自动退出时，才执行：

```bash
hdfs dfsadmin -safemode leave
```

## 2. 生成一批可复现的 Raw 数据

```bash
python3 bigdata/data-generator/generate_raw.py \
  --output-dir output/raw/BD-20260915-001 \
  --batch-id BD-20260915-001 \
  --business-date 2026-09-15
```

检查生成结果：

```bash
ls output/raw/BD-20260915-001
cat output/raw/BD-20260915-001/manifest.json
```

应包含六份 CSV 和 `manifest.json`。manifest 中的 seed、行数、哈希与异常注入统计是演示证据。

## 3. 上传到 HDFS ODS

```bash
python3 bigdata/ingestion/ingest_ods.py \
  --raw-dir output/raw/BD-20260915-001 \
  --business-date 2026-09-15 \
  --batch-id BD-20260915-001
```

验证：

```bash
hdfs dfs -ls -R /evcharge/ods | grep BD-20260915-001
```

默认不覆盖已有批次。仅在重跑同一个测试批次时使用 `--replace`，它只会替换该批次的 ODS 路径。

## 4. 执行质量检测

```bash
spark-submit bigdata/spark/quality/quality_check.py \
  --business-date 2026-09-15 \
  --batch-id BD-20260915-001
```

作业输出：

```text
/evcharge/quality/accepted/<dataset>/dt=2026-09-15/batch=BD-20260915-001
/evcharge/quality/rejected/<dataset>/dt=2026-09-15/batch=BD-20260915-001
/evcharge/quality/reports/dt=2026-09-15/batch=BD-20260915-001
```

`rejected` 保留原始记录、命中的 `DQ-*` 规则和中文原因；它是数据治理的核心演示证据。

## 5. 发布 DWD

```bash
spark-submit bigdata/spark/dwd/build_dwd.py \
  --business-date 2026-09-15 \
  --batch-id BD-20260915-001
```

验证：

```bash
hdfs dfs -ls -R /evcharge/dwd | grep BD-20260915-001
```

应有四个输出：

```text
dwd_order_detail
dwd_charging_session_detail
dwd_pile_detail
dwd_station_hour_metric
```

这些 Parquet 数据是交给 C 开发 DWS、ADS 和 Flask API 的唯一上游输入。C 不应绕过 DWD 读取 ODS。

UrbanEV 真实数据适配器同样必须先生成冻结 Raw Contract，再复用本手册的 ODS、DQ 和 DWD，
不能让 ML 或大屏直接读取论文 CSV。完整入口见 `bigdata/urbanev/run_pipeline.sh`。

## 6. 重跑同一批次

若需要反复演示同一个 batch，ODS、quality、DWD 三个命令都添加 `--replace`。该开关只覆盖命令中
指定日期和批次号的输出，使用前仍应确认 `business-date` 与 `batch-id`。
