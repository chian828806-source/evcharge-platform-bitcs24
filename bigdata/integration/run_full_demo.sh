#!/usr/bin/env bash
# 依次运行 B 数据管道、临时 DWS 桥、E 的 MLlib 作业，生成 Flask/大屏可直接读取的结果。

set -euo pipefail

business_date="${1:-2026-09-15}"
batch_id="${2:-BD-20260915-DEMO}"
project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
raw_dir="${project_root}/bigdata/runtime/raw/${batch_id}"
dws_path="hdfs:///evcharge/dws/dws_station_hour/batch=${batch_id}"
quality_report="hdfs:///evcharge/quality/reports/dt=${business_date}/batch=${batch_id}"

cd "${project_root}"

# 所有步骤使用同一 business_date 与 batch_id，使 ODS、DQ、DWD、DWS、ADS 可以追溯。
hdfs dfs -test -d / || {
  echo "HDFS 当前不可用，请先启动 NameNode 和 DataNode。" >&2
  exit 1
}

python3 bigdata/data-generator/generate_raw.py \
  --config bigdata/integration/pipeline_demo_config.json \
  --output-dir "${raw_dir}" \
  --batch-id "${batch_id}" \
  --business-date "${business_date}"

python3 bigdata/ingestion/ingest_ods.py \
  --raw-dir "${raw_dir}" \
  --business-date "${business_date}" \
  --batch-id "${batch_id}" \
  --replace

spark-submit bigdata/spark/quality/quality_check.py \
  --business-date "${business_date}" \
  --batch-id "${batch_id}" \
  --replace

spark-submit bigdata/spark/dwd/build_dwd.py \
  --business-date "${business_date}" \
  --batch-id "${batch_id}" \
  --replace

spark-submit bigdata/integration/build_demo_dws.py \
  --business-date "${business_date}" \
  --batch-id "${batch_id}" \
  --replace

spark-submit bigdata/ml/jobs/station_load_demo.py \
  --dws-input "${dws_path}" \
  --quality-report "${quality_report}" \
  --output bigdata/runtime/demo

echo "Demo 数据已生成：bigdata/runtime/demo/dashboard.json"
