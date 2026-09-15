#!/usr/bin/env python3
"""Build four contract-aligned DWD Parquet datasets from quality-approved records."""

from __future__ import annotations

import argparse
import json
import sys
from typing import Any

from pyspark.sql import DataFrame, SparkSession
from pyspark.sql import functions as F


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--business-date", required=True)
    parser.add_argument("--batch-id", required=True)
    parser.add_argument("--hdfs-root", default="/evcharge")
    parser.add_argument("--replace", action="store_true", help="Replace only this exact DWD output batch")
    return parser.parse_args()


def accepted_path(root: str, dataset: str, date: str, batch_id: str) -> str:
    return f"{root}/quality/accepted/{dataset}/dt={date}/batch={batch_id}"


def dwd_path(root: str, table: str, date: str, batch_id: str) -> str:
    return f"{root}/dwd/{table}/dt={date}/batch={batch_id}"


def write_batch(frame: DataFrame, path: str, replace: bool) -> None:
    frame.write.mode("overwrite" if replace else "errorifexists").parquet(path)


def audit_columns(alias: str) -> list[F.Column]:
    return [
        F.col(f"{alias}.batch_id").alias("batch_id"),
        F.col(f"{alias}.source_file").alias("source_file"),
        F.col(f"{alias}.ingested_at").alias("ingested_at"),
        F.col(f"{alias}.processed_at").alias("processed_at"),
        F.col(f"{alias}.dt").alias("dt"),
    ]


def build_order_detail(orders: DataFrame, stations: DataFrame, piles: DataFrame) -> DataFrame:
    return orders.alias("o").join(stations.alias("s"), "station_id", "inner").join(piles.alias("p"), "pile_id", "inner").select(
        F.col("o.order_id"), F.col("o.order_no"), F.col("o.user_id"), F.col("o.station_id"),
        F.col("s.station_no"), F.col("s.name").alias("station_name"), F.col("s.district"), F.col("s.status").alias("station_status"),
        F.col("o.pile_id"), F.col("p.pile_no"), F.col("p.type").alias("pile_type"), F.col("p.power_kw").alias("pile_power_kw"),
        F.col("o.status"), F.col("o.start_at"), F.col("o.end_at"), F.col("o.charge_minutes"), F.col("o.energy_kwh"),
        F.col("o.amount_fen"), F.col("o.paid_at"), F.col("o.created_at"), *audit_columns("o"),
    )


def build_session_detail(sessions: DataFrame, stations: DataFrame) -> DataFrame:
    return sessions.alias("x").join(stations.alias("s"), "station_id", "inner").select(
        F.col("x.source_session_key"), F.col("x.station_id"), F.col("s.station_no"),
        F.col("s.name").alias("station_name"), F.col("s.district"), F.col("s.status").alias("station_status"),
        F.col("x.source_station_name"), F.col("x.start_at"), F.col("x.end_at"), F.col("x.duration_seconds"),
        F.col("x.energy_kwh"), *audit_columns("x"),
    )


def build_pile_detail(piles: DataFrame, stations: DataFrame) -> DataFrame:
    return piles.alias("p").join(stations.alias("s"), "station_id", "inner").select(
        F.col("p.pile_id"), F.col("p.station_id"), F.col("s.station_no"), F.col("s.name").alias("station_name"),
        F.col("s.district"), F.col("s.status").alias("station_status"), F.col("s.longitude"), F.col("s.latitude"),
        F.col("p.pile_no"), F.col("p.type").alias("pile_type"), F.col("p.power_kw"), F.col("p.status"),
        F.col("p.total_charge_count"), F.col("p.total_charge_minutes"), F.col("p.total_energy_kwh"), F.col("p.updated_at"),
        *audit_columns("p"),
    )


def build_station_hour_metric(metrics: DataFrame, stations: DataFrame) -> DataFrame:
    return metrics.alias("m").join(stations.alias("s"), "station_id", "inner").select(
        F.col("m.station_id"), F.col("s.station_no"), F.col("s.name").alias("station_name"), F.col("s.district"),
        F.col("s.status").alias("station_status"), F.col("m.hour_start"), F.col("m.total_pile_count"),
        F.col("m.session_starts"), F.col("m.energy_kwh"), F.col("m.charging_pile_minutes"),
        F.col("m.average_occupied_count"), F.col("m.average_available_count"), F.col("m.station_load"),
        F.col("m.source_type"), *audit_columns("m"),
    )


def main() -> None:
    args = parse_args()
    root = args.hdfs_root.rstrip("/")
    spark = SparkSession.builder.appName(f"evcharge-dwd-{args.batch_id}").getOrCreate()
    spark.sparkContext.setLogLevel("WARN")
    spark.conf.set("spark.sql.session.timeZone", "Asia/Shanghai")
    try:
        inputs = {
            dataset: spark.read.parquet(accepted_path(root, dataset, args.business_date, args.batch_id))
            for dataset in ("stations", "piles", "orders", "sessions", "station_hourly_metrics")
        }
        outputs = {
            "dwd_order_detail": build_order_detail(inputs["orders"], inputs["stations"], inputs["piles"]),
            "dwd_charging_session_detail": build_session_detail(inputs["sessions"], inputs["stations"]),
            "dwd_pile_detail": build_pile_detail(inputs["piles"], inputs["stations"]),
            "dwd_station_hour_metric": build_station_hour_metric(inputs["station_hourly_metrics"], inputs["stations"]),
        }
        report: list[dict[str, Any]] = []
        for table, frame in outputs.items():
            target = dwd_path(root, table, args.business_date, args.batch_id)
            write_batch(frame, target, args.replace)
            report.append({"table": table, "path": target, "rows": frame.count()})
        manifest = spark.createDataFrame(
            [(json.dumps({"batch_id": args.batch_id, "business_date": args.business_date, "tables": report}, ensure_ascii=False),)],
            ["manifest_json"],
        )
        write_batch(manifest, f"{root}/dwd/_manifests/dt={args.business_date}/batch={args.batch_id}", args.replace)
        print(json.dumps({"batch_id": args.batch_id, "tables": report}, ensure_ascii=False, indent=2))
    finally:
        spark.stop()


if __name__ == "__main__":
    try:
        main()
    except Exception as error:  # Spark exceptions are implementation-specific.
        print(f"DWD build failed: {error}", file=sys.stderr)
        raise SystemExit(1)
