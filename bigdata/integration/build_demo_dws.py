#!/usr/bin/env python3
"""为端到端演示把站点小时 DWD 发布成最小 DWS。

功能：只读取 B 已通过质量检查的 ``dwd_station_hour_metric``，按相同的站点小时
粒度投影出 ML Contract 所需字段。该脚本是 C 的正式 DWS 作业到位前的联调桥，
不会修改 ODS、质量结果或 DWD 分区。
"""

from __future__ import annotations

import argparse
import json
import sys

from pyspark.sql import SparkSession
from pyspark.sql import functions as F


DWS_COLUMNS = (
    "station_id", "station_name", "hour_start", "total_pile_count", "session_starts",
    "energy_kwh", "charging_pile_minutes", "average_occupied_count",
    "average_available_count", "station_load", "batch_id", "source_type", "dt",
)


def parse_args() -> argparse.Namespace:
    """读取单个批次的位置；重跑只能覆盖该批次自己的 DWS 路径。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--business-date", required=True)
    parser.add_argument("--batch-id", required=True)
    parser.add_argument("--hdfs-root", default="/evcharge")
    parser.add_argument("--replace", action="store_true")
    return parser.parse_args()


def main() -> None:
    """读取同批次 DWD、校验并发布可供 ML 消费的 DWS。"""
    args = parse_args()
    root = args.hdfs_root.rstrip("/")
    source = f"{root}/dwd/dwd_station_hour_metric/dt={args.business_date}/batch={args.batch_id}"
    target = f"{root}/dws/dws_station_hour/batch={args.batch_id}"
    spark = SparkSession.builder.appName(f"evcharge-demo-dws-{args.batch_id}").getOrCreate()
    spark.sparkContext.setLogLevel("WARN")
    spark.conf.set("spark.sql.session.timeZone", "Asia/Shanghai")
    try:
        dwd = spark.read.parquet(source)
        missing = sorted(set(DWS_COLUMNS) - set(dwd.columns))
        if missing:
            raise ValueError(f"DWD 缺少 DWS 所需字段: {', '.join(missing)}")

        dws = dwd.select(*DWS_COLUMNS)
        duplicate_rows = dws.groupBy("station_id", "hour_start").count().where("count > 1").count()
        invalid_rows = dws.where(
            F.col("station_id").isNull()
            | F.col("hour_start").isNull()
            | (F.col("total_pile_count") <= 0)
            | (F.col("station_load") < 0)
            | (F.col("station_load") > 1)
        ).count()
        foreign_batches = dws.where(F.col("batch_id") != args.batch_id).count()
        if duplicate_rows or invalid_rows or foreign_batches:
            raise ValueError(
                "DWD 不能发布为演示 DWS: "
                f"duplicateKeys={duplicate_rows}, invalidRows={invalid_rows}, foreignBatches={foreign_batches}"
            )

        dws.write.mode("overwrite" if args.replace else "errorifexists").partitionBy("dt").parquet(target)
        print(json.dumps({
            "batch_id": args.batch_id,
            "source": source,
            "target": target,
            "rows": dws.count(),
            "mode": "demo-pass-through",
        }, ensure_ascii=False, indent=2))
    finally:
        spark.stop()


if __name__ == "__main__":
    try:
        main()
    except Exception as error:  # Spark 异常类型随运行环境变化，入口统一转为失败状态。
        print(f"Demo DWS build failed: {error}", file=sys.stderr)
        raise SystemExit(1)
