#!/usr/bin/env bash
# Run the real UrbanEV Raw -> ODS -> DQ -> DWD -> DWS -> ML -> Dashboard workflow.

set -euo pipefail

dataset_root="${1:?Usage: run_pipeline.sh <unpacked-UrbanEV-root> [max-stations] [batch-id]}"
max_stations="${2:-8}"
batch_id="${3:-URBANEV-20230228-RAW-V1}"
business_date="2023-02-28"
project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
raw_dir="${project_root}/bigdata/runtime/urbanev/raw/${batch_id}"
dws_path="hdfs:///evcharge/dws/dws_station_hour/dt=${business_date}/batch=${batch_id}"
quality_report="hdfs:///evcharge/quality/reports/dt=${business_date}/batch=${batch_id}"

cd "${project_root}"

# Refuse to fall back to the random generator: this entry point requires the official archive.
hdfs dfs -test -d / || {
  echo "HDFS is unavailable. Start NameNode and DataNode before importing UrbanEV." >&2
  exit 1
}

python3 bigdata/urbanev/prepare_raw.py \
  --dataset-root "${dataset_root}" \
  --output-dir "${raw_dir}" \
  --batch-id "${batch_id}" \
  --business-date "${business_date}" \
  --max-stations "${max_stations}"

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

spark-submit bigdata/spark/warehouse/build_warehouse.py \
  --business-date "${business_date}" \
  --batch-id "${batch_id}" \
  --snapshot-output bigdata/runtime/warehouse/dashboard.json \
  --replace

spark-submit bigdata/ml/jobs/station_load_demo.py \
  --dws-input "${dws_path}" \
  --quality-report "${quality_report}" \
  --output bigdata/runtime/demo

echo "UrbanEV workflow completed: C ADS and E prediction snapshots are ready."
