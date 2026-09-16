#!/usr/bin/env python3
"""Validate one ODS batch with PySpark and publish accepted/rejected datasets."""

from __future__ import annotations

import argparse
import json
import sys
from functools import reduce
from typing import Any

from pyspark.sql import DataFrame, SparkSession, Window
from pyspark.sql import functions as F


SPECS: dict[str, dict[str, Any]] = {
    "users": {
        "columns": {"user_id": "bigint", "status": "string", "created_at": "timestamp"},
        "primary_key": ["user_id"],
        "required": ["user_id", "status", "created_at"],
        "enums": {"status": ["NORMAL", "FROZEN"]},
        "numeric_rules": [],
    },
    "stations": {
        "columns": {
            "station_id": "bigint", "station_no": "string", "name": "string", "district": "string",
            "longitude": "double", "latitude": "double", "price_fen_per_kwh": "bigint",
            "service_fee_fen_per_kwh": "bigint", "status": "string", "created_at": "timestamp",
        },
        "primary_key": ["station_id"],
        "required": ["station_id", "station_no", "name", "longitude", "latitude", "price_fen_per_kwh", "service_fee_fen_per_kwh", "status", "created_at"],
        "enums": {"status": ["NORMAL", "DISABLED"]},
        "numeric_rules": [
            ("longitude", lambda value: (value < -180) | (value > 180)),
            ("latitude", lambda value: (value < -90) | (value > 90)),
            ("price_fen_per_kwh", lambda value: value < 0),
            ("service_fee_fen_per_kwh", lambda value: value < 0),
        ],
    },
    "piles": {
        "columns": {
            "pile_id": "bigint", "station_id": "bigint", "pile_no": "string", "type": "string",
            "power_kw": "double", "status": "string", "total_charge_count": "bigint",
            "total_charge_minutes": "bigint", "total_energy_kwh": "double", "updated_at": "timestamp",
        },
        "primary_key": ["pile_id"],
        "required": ["pile_id", "station_id", "pile_no", "type", "power_kw", "status", "total_charge_count", "total_charge_minutes", "total_energy_kwh", "updated_at"],
        "enums": {"type": ["FAST", "SLOW"], "status": ["AVAILABLE", "RESERVED", "CHARGING", "FAULT", "OFFLINE", "RESTARTING"]},
        "numeric_rules": [
            ("power_kw", lambda value: value <= 0),
            ("total_charge_count", lambda value: value < 0),
            ("total_charge_minutes", lambda value: value < 0),
            ("total_energy_kwh", lambda value: value < 0),
        ],
    },
    "orders": {
        "columns": {
            "order_id": "bigint", "order_no": "string", "user_id": "bigint", "station_id": "bigint",
            "pile_id": "bigint", "status": "string", "start_at": "timestamp", "end_at": "timestamp",
            "charge_minutes": "int", "energy_kwh": "double", "amount_fen": "bigint", "paid_at": "timestamp",
            "created_at": "timestamp",
        },
        "primary_key": ["order_id"],
        "required": ["order_id", "order_no", "user_id", "station_id", "pile_id", "status", "charge_minutes", "energy_kwh", "amount_fen", "created_at"],
        "enums": {"status": ["CREATED", "CHARGING", "PENDING_PAYMENT", "COMPLETED", "CANCELLED"]},
        "numeric_rules": [
            ("charge_minutes", lambda value: value < 0),
            ("energy_kwh", lambda value: value < 0),
            ("amount_fen", lambda value: value < 0),
        ],
    },
    "sessions": {
        "columns": {
            "source_session_key": "string", "station_id": "bigint", "source_station_name": "string",
            "start_at": "timestamp", "end_at": "timestamp", "duration_seconds": "bigint", "energy_kwh": "double",
        },
        "primary_key": ["source_session_key"],
        "required": ["source_session_key", "station_id", "start_at", "end_at", "duration_seconds", "energy_kwh"],
        "enums": {},
        "numeric_rules": [("duration_seconds", lambda value: value <= 0), ("energy_kwh", lambda value: value < 0)],
    },
    "station_hourly_metrics": {
        "columns": {
            "station_id": "bigint", "hour_start": "timestamp", "total_pile_count": "int",
            "session_starts": "int", "energy_kwh": "double", "charging_pile_minutes": "double",
            "average_occupied_count": "double", "average_available_count": "double",
            "station_load": "double", "source_type": "string",
        },
        "primary_key": ["station_id", "hour_start"],
        "required": ["station_id", "hour_start", "total_pile_count", "session_starts", "energy_kwh", "charging_pile_minutes", "average_occupied_count", "average_available_count", "station_load", "source_type"],
        "enums": {"source_type": ["BUSINESS", "CARY_SIMULATION"]},
        "numeric_rules": [
            ("total_pile_count", lambda value: value <= 0),
            ("session_starts", lambda value: value < 0),
            ("energy_kwh", lambda value: value < 0),
            ("charging_pile_minutes", lambda value: value < 0),
            ("station_load", lambda value: (value < 0) | (value > 1)),
        ],
    },
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--business-date", required=True)
    parser.add_argument("--batch-id", required=True)
    parser.add_argument("--hdfs-root", default="/evcharge")
    parser.add_argument("--replace", action="store_true", help="Replace only this exact quality output batch")
    return parser.parse_args()


def empty_string(column: str) -> F.Column:
    return F.col(column).isNull() | (F.trim(F.col(column)) == "")


def any_of(conditions: list[F.Column]) -> F.Column:
    return reduce(lambda left, right: left | right, conditions, F.lit(False))


def add_base_checks(raw: DataFrame, dataset: str, batch_id: str, ingested_at: str) -> DataFrame:
    spec = SPECS[dataset]
    columns = spec["columns"]
    # CSV 先统一按字符串读取。保留原值后再 cast，才能把类型错误的记录完整写入 rejected。
    frame = raw.select(*[F.col(name).cast("string").alias(name) for name in columns])
    frame = frame.withColumn("source_file", F.regexp_extract(F.input_file_name(), r"([^/]+)$", 1))
    frame = frame.withColumn("raw_record", F.to_json(F.struct(*[F.col(name) for name in columns])))
    for name, data_type in columns.items():
        frame = frame.withColumn(f"_t_{name}", F.col(name).cast(data_type))

    primary_key = spec["primary_key"]
    # DQ-001：主键为空或重复。主键为空不再重复归类为 DQ-002，保证原因清晰。
    key_blank = any_of([empty_string(name) for name in primary_key])
    key_typed = [F.col(f"_t_{name}") for name in primary_key]
    duplicate = (~key_blank) & (F.count(F.lit(1)).over(Window.partitionBy(*key_typed)) > 1)
    # DQ-002：非主键必填字段为空，或非空字符串无法转换为目标类型。
    required_non_key = [name for name in spec["required"] if name not in primary_key]
    required_missing = any_of([empty_string(name) for name in required_non_key])
    type_invalid = any_of([
        (~empty_string(name)) & F.col(f"_t_{name}").isNull()
        for name, data_type in columns.items() if data_type != "string"
    ])
    # DQ-003、DQ-005 只在类型可解析时检查，避免一条记录出现误导性的重复原因。
    range_invalid = any_of([
        F.col(f"_t_{name}").isNotNull() & invalid(F.col(f"_t_{name}"))
        for name, invalid in spec["numeric_rules"]
    ])
    enum_invalid = any_of([
        (~empty_string(name)) & (~F.col(f"_t_{name}").isin(values))
        for name, values in spec["enums"].items()
    ])
    time_invalid = F.lit(False)
    if dataset in {"orders", "sessions"}:
        time_invalid = (
            F.col("_t_start_at").isNotNull()
            & F.col("_t_end_at").isNotNull()
            & (F.col("_t_end_at") <= F.col("_t_start_at"))
        )
    if dataset == "orders":
        time_invalid = time_invalid | (
            F.col("_t_paid_at").isNotNull()
            & F.col("_t_start_at").isNotNull()
            & (F.col("_t_paid_at") < F.col("_t_start_at"))
        )
    if dataset == "station_hourly_metrics":
        total = F.col("_t_total_pile_count")
        time_invalid = ((F.col("_t_hour_start").isNotNull()) & (F.date_trunc("hour", F.col("_t_hour_start")) != F.col("_t_hour_start")))
        range_invalid = range_invalid | (
            total.isNotNull()
            & ((F.col("_t_charging_pile_minutes") > total * F.lit(60))
               | (F.col("_t_average_occupied_count") > total)
               | (F.col("_t_average_available_count") > total))
        )

    return (
        frame.withColumn("_dq001", key_blank | duplicate)
        .withColumn("_dq002", required_missing | type_invalid)
        .withColumn("_dq003", range_invalid)
        .withColumn("_dq004", time_invalid)
        .withColumn("_dq005", enum_invalid)
        .withColumn("_dq006", F.lit(False))
        .withColumn("batch_id", F.lit(batch_id))
        .withColumn("ingested_at", F.to_timestamp(F.lit(ingested_at), "yyyy-MM-dd'T'HH:mm:ssXXX"))
        .withColumn("processed_at", F.current_timestamp())
    )


def add_fk_check(frame: DataFrame, reference: DataFrame, left_column: str, reference_column: str, temporary: str) -> DataFrame:
    reference_key = reference.select(F.col(reference_column).alias(temporary)).distinct()
    # left join 保留待检查的原始行；关联不到才标记 DQ-006，而不是在 join 时静默丢行。
    joined = frame.join(reference_key, F.col(left_column) == F.col(temporary), "left")
    missing = F.col(left_column).isNotNull() & F.col(temporary).isNull()
    return joined.withColumn("_dq006", F.col("_dq006") | missing).drop(temporary)


def add_order_pile_fk(frame: DataFrame, piles: DataFrame) -> DataFrame:
    reference = piles.select(
        F.col("_t_pile_id").alias("_fk_pile_id"),
        F.col("_t_station_id").alias("_fk_pile_station_id"),
    ).distinct()
    joined = frame.join(
        reference,
        (F.col("_t_pile_id") == F.col("_fk_pile_id")) & (F.col("_t_station_id") == F.col("_fk_pile_station_id")),
        "left",
    )
    missing = F.col("_t_pile_id").isNotNull() & F.col("_fk_pile_id").isNull()
    return joined.withColumn("_dq006", F.col("_dq006") | missing).drop("_fk_pile_id", "_fk_pile_station_id")


def finalise(frame: DataFrame, dataset: str, business_date: str) -> tuple[DataFrame, DataFrame]:
    rules = F.filter(F.array(
        F.when(F.col("_dq001"), F.lit("DQ-001")),
        F.when(F.col("_dq002"), F.lit("DQ-002")),
        F.when(F.col("_dq003"), F.lit("DQ-003")),
        F.when(F.col("_dq004"), F.lit("DQ-004")),
        F.when(F.col("_dq005"), F.lit("DQ-005")),
        F.when(F.col("_dq006"), F.lit("DQ-006")),
    ), lambda item: item.isNotNull())
    reasons = F.filter(F.array(
        F.when(F.col("_dq001"), F.lit("主键为空或重复")),
        F.when(F.col("_dq002"), F.lit("必填字段为空或字段类型无法解析")),
        F.when(F.col("_dq003"), F.lit("数值范围不合法")),
        F.when(F.col("_dq004"), F.lit("时间先后关系不合法")),
        F.when(F.col("_dq005"), F.lit("枚举值不合法")),
        F.when(F.col("_dq006"), F.lit("关联站点或电桩不存在")),
    ), lambda item: item.isNotNull())
    # 一条记录可同时命中多条规则，因此用数组完整保存全部规则和原因。
    with_results = frame.withColumn("rule_ids", rules).withColumn("reasons", reasons)
    typed_columns = [F.col(f"_t_{name}").alias(name) for name in SPECS[dataset]["columns"]]
    accepted = with_results.where(F.size(F.col("rule_ids")) == 0).select(
        *typed_columns,
        "batch_id", "source_file", "ingested_at", "processed_at",
        F.lit(business_date).cast("date").alias("dt"),
    )
    rejected = with_results.where(F.size(F.col("rule_ids")) > 0).select(
        F.lit(dataset).alias("dataset"), "batch_id", "source_file", "raw_record", "rule_ids", "reasons",
        F.col("processed_at").alias("rejected_at"), F.lit(business_date).cast("date").alias("dt"),
    )
    return accepted, rejected


def write_batch(frame: DataFrame, path: str, replace: bool) -> None:
    writer = frame.write.mode("overwrite" if replace else "errorifexists")
    writer.parquet(path)


def main() -> None:
    args = parse_args()
    root = args.hdfs_root.rstrip("/")
    spark = SparkSession.builder.appName(f"evcharge-quality-{args.batch_id}").getOrCreate()
    spark.sparkContext.setLogLevel("WARN")
    spark.conf.set("spark.sql.session.timeZone", "Asia/Shanghai")
    try:
        manifest_path = f"{root}/ods/_manifests/dt={args.business_date}/batch={args.batch_id}/manifest.json"
        manifest = spark.read.option("multiLine", "true").json(manifest_path).select("ingested_at").first()
        if manifest is None or manifest["ingested_at"] is None:
            raise RuntimeError(f"Cannot read ingestion metadata from {manifest_path}")
        ingested_at = manifest["ingested_at"]

        prepared: dict[str, DataFrame] = {}
        for dataset in SPECS:
            source = f"{root}/ods/{dataset}/dt={args.business_date}/batch={args.batch_id}/{dataset}.csv"
            prepared[dataset] = add_base_checks(spark.read.option("header", "true").csv(source), dataset, args.batch_id, ingested_at)

        # 外键校验必须按依赖顺序执行：站点 → 电桩 → 订单；会话和小时指标只依赖站点。
        # 下游只认可质量通过的维度记录，避免脏维度扩散到 DWD。
        valid_stations = prepared["stations"].where(~(F.col("_dq001") | F.col("_dq002") | F.col("_dq003") | F.col("_dq004") | F.col("_dq005")))
        prepared["piles"] = add_fk_check(prepared["piles"], valid_stations, "_t_station_id", "_t_station_id", "_fk_station_id")
        valid_piles = prepared["piles"].where(~(F.col("_dq001") | F.col("_dq002") | F.col("_dq003") | F.col("_dq004") | F.col("_dq005") | F.col("_dq006")))
        prepared["orders"] = add_fk_check(prepared["orders"], valid_stations, "_t_station_id", "_t_station_id", "_fk_station_id")
        prepared["orders"] = add_order_pile_fk(prepared["orders"], valid_piles)
        prepared["sessions"] = add_fk_check(prepared["sessions"], valid_stations, "_t_station_id", "_t_station_id", "_fk_station_id")
        prepared["station_hourly_metrics"] = add_fk_check(prepared["station_hourly_metrics"], valid_stations, "_t_station_id", "_t_station_id", "_fk_station_id")

        summary: list[dict[str, Any]] = []
        for dataset, frame in prepared.items():
            accepted, rejected = finalise(frame, dataset, args.business_date)
            accepted_path = f"{root}/quality/accepted/{dataset}/dt={args.business_date}/batch={args.batch_id}"
            rejected_path = f"{root}/quality/rejected/{dataset}/dt={args.business_date}/batch={args.batch_id}"
            write_batch(accepted, accepted_path, args.replace)
            write_batch(rejected, rejected_path, args.replace)
            rule_counts = {
                row["rule_id"]: row["count"]
                for row in rejected.select(F.explode("rule_ids").alias("rule_id")).groupBy("rule_id").count().collect()
            }
            source_rows = frame.count()
            rejected_rows = rejected.count()
            summary.append({
                "dataset": dataset,
                "source_rows": source_rows,
                "accepted_rows": source_rows - rejected_rows,
                "rejected_rows": rejected_rows,
                "rules": rule_counts,
            })
        report = spark.createDataFrame(
            [(json.dumps({"batch_id": args.batch_id, "business_date": args.business_date, "datasets": summary}, ensure_ascii=False),)],
            ["report_json"],
        )
        write_batch(report, f"{root}/quality/reports/dt={args.business_date}/batch={args.batch_id}", args.replace)
        print(json.dumps({"batch_id": args.batch_id, "datasets": summary}, ensure_ascii=False, indent=2))
    finally:
        spark.stop()


if __name__ == "__main__":
    try:
        main()
    except Exception as error:  # Spark exceptions are implementation-specific.
        print(f"Quality check failed: {error}", file=sys.stderr)
        raise SystemExit(1)
