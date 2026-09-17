#!/usr/bin/env python3
"""Build the contract DWS/ADS tables from one published DWD batch with Spark SQL."""

from __future__ import annotations

import argparse
import json
import os
import sys
from datetime import date, datetime, timezone
from pathlib import Path
from typing import Any

from pyspark.sql import DataFrame, SparkSession
from pyspark.sql import functions as F


DWD_TABLES = (
    "dwd_order_detail",
    "dwd_charging_session_detail",
    "dwd_pile_detail",
    "dwd_station_hour_metric",
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--business-date", required=True)
    parser.add_argument("--batch-id", required=True)
    parser.add_argument("--hdfs-root", default="/evcharge")
    parser.add_argument("--snapshot-output", default="bigdata/runtime/warehouse/dashboard.json")
    parser.add_argument("--replace", action="store_true", help="Replace only this exact warehouse batch")
    return parser.parse_args()


def dwd_path(root: str, table: str, business_date: str, batch_id: str) -> str:
    return f"{root}/dwd/{table}/dt={business_date}/batch={batch_id}"


def warehouse_path(root: str, layer: str, table: str, business_date: str, batch_id: str) -> str:
    return f"{root}/{layer}/{table}/dt={business_date}/batch={batch_id}"


def write_batch(frame: DataFrame, path: str, replace: bool) -> None:
    frame.write.mode("overwrite" if replace else "errorifexists").parquet(path)


def require_columns(frame: DataFrame, table: str, required: set[str]) -> None:
    missing = sorted(required - set(frame.columns))
    if missing:
        raise ValueError(f"{table} missing required columns: {', '.join(missing)}")


def validate_inputs(inputs: dict[str, DataFrame], batch_id: str) -> None:
    required = {
        "dwd_order_detail": {"order_id", "order_no", "station_id", "pile_id", "station_name", "district", "status", "charge_minutes", "energy_kwh", "amount_fen", "paid_at", "batch_id"},
        "dwd_charging_session_detail": {"source_session_key", "station_id", "start_at", "energy_kwh", "batch_id"},
        "dwd_pile_detail": {"pile_id", "station_id", "station_name", "district", "status", "updated_at", "batch_id"},
        "dwd_station_hour_metric": {"station_id", "station_name", "district", "hour_start", "total_pile_count", "session_starts", "energy_kwh", "charging_pile_minutes", "average_occupied_count", "average_available_count", "station_load", "source_type", "batch_id"},
    }
    for table, frame in inputs.items():
        require_columns(frame, table, required[table])
        batches = [row.batch_id for row in frame.select("batch_id").distinct().collect()]
        if batches and batches != [batch_id]:
            raise ValueError(f"{table} batch mismatch: expected={batch_id}, actual={batches}")
        if table in {"dwd_pile_detail", "dwd_station_hour_metric"} and frame.limit(1).count() == 0:
            raise ValueError(f"{table} is empty; warehouse publication was refused")


def build_dws(spark: SparkSession, inputs: dict[str, DataFrame], batch_id: str) -> dict[str, DataFrame]:
    for table, frame in inputs.items():
        frame.createOrReplaceTempView(table)
    escaped_batch = batch_id.replace("'", "''")
    paid_orders = spark.sql(f"""
        SELECT station_id, pile_id, station_name, district,
               date_trunc('hour', paid_at) AS paid_hour,
               to_date(paid_at) AS stat_date,
               count(*) AS completed_order_count,
               sum(charge_minutes) AS charge_minutes,
               sum(energy_kwh) AS business_energy_kwh,
               sum(amount_fen) AS revenue_fen
        FROM dwd_order_detail
        WHERE batch_id = '{escaped_batch}'
          AND status = 'COMPLETED' AND paid_at IS NOT NULL
          AND order_no NOT LIKE 'URBANEV-SYN-%'
        GROUP BY station_id, pile_id, station_name, district,
                 date_trunc('hour', paid_at), to_date(paid_at)
    """)
    paid_orders.createOrReplaceTempView("paid_orders")

    station_hour = spark.sql(f"""
        SELECT m.station_id, m.station_name, m.district, m.hour_start,
               m.total_pile_count, m.session_starts, m.energy_kwh,
               m.charging_pile_minutes, m.average_occupied_count,
               m.average_available_count,
               least(1D, greatest(0D, m.charging_pile_minutes / (m.total_pile_count * 60D))) AS utilization_rate,
               m.station_load, m.source_type,
               coalesce(sum(o.completed_order_count), 0L) AS completed_order_count,
               coalesce(sum(o.business_energy_kwh), 0D) AS business_energy_kwh,
               coalesce(sum(o.revenue_fen), 0L) AS revenue_fen,
               '{escaped_batch}' AS batch_id, current_timestamp() AS calculated_at,
               to_date(m.hour_start) AS dt
        FROM dwd_station_hour_metric m
        LEFT JOIN paid_orders o
          ON m.station_id = o.station_id AND m.hour_start = o.paid_hour
        GROUP BY m.station_id, m.station_name, m.district, m.hour_start,
                 m.total_pile_count, m.session_starts, m.energy_kwh,
                 m.charging_pile_minutes, m.average_occupied_count,
                 m.average_available_count, m.station_load, m.source_type
    """)
    station_hour.createOrReplaceTempView("dws_station_hour")

    station_day = spark.sql("""
        SELECT station_id, station_name, district, to_date(hour_start) AS stat_date,
               sum(completed_order_count) AS order_count,
               sum(business_energy_kwh) AS energy_kwh,
               sum(revenue_fen) AS revenue_fen,
               sum(charging_pile_minutes) AS charging_pile_minutes,
               sum(total_pile_count * 60D) AS capacity_pile_minutes,
               least(1D, greatest(0D, sum(charging_pile_minutes) / sum(total_pile_count * 60D))) AS utilization_rate,
               avg(average_available_count) AS available_count,
               max(total_pile_count) AS total_pile_count,
               first(batch_id) AS batch_id, max(calculated_at) AS calculated_at,
               to_date(hour_start) AS dt
        FROM dws_station_hour
        GROUP BY station_id, station_name, district, to_date(hour_start)
    """)
    station_day.createOrReplaceTempView("dws_station_day")

    region_day = spark.sql("""
        SELECT district, stat_date, sum(order_count) AS order_count,
               sum(energy_kwh) AS energy_kwh, sum(revenue_fen) AS revenue_fen,
               sum(charging_pile_minutes) AS charging_pile_minutes,
               sum(capacity_pile_minutes) AS capacity_pile_minutes,
               least(1D, greatest(0D, sum(charging_pile_minutes) / sum(capacity_pile_minutes))) AS utilization_rate,
               sum(available_count) AS available_count, sum(total_pile_count) AS total_pile_count,
               first(batch_id) AS batch_id, max(calculated_at) AS calculated_at,
               stat_date AS dt
        FROM dws_station_day
        GROUP BY district, stat_date
    """)

    pile_day = spark.sql(f"""
        SELECT p.pile_id, p.station_id, p.station_name, p.district,
               coalesce(o.stat_date, to_date(p.updated_at)) AS stat_date, p.status,
               coalesce(sum(o.completed_order_count), 0L) AS order_count,
               coalesce(sum(o.business_energy_kwh), 0D) AS energy_kwh,
               coalesce(sum(o.revenue_fen), 0L) AS revenue_fen,
               least(1D, greatest(0D, coalesce(sum(o.charge_minutes), 0D) / 1440D)) AS utilization_rate,
               '{escaped_batch}' AS batch_id, current_timestamp() AS calculated_at,
               coalesce(o.stat_date, to_date(p.updated_at)) AS dt
        FROM dwd_pile_detail p
        LEFT JOIN paid_orders o ON p.pile_id = o.pile_id
        GROUP BY p.pile_id, p.station_id, p.station_name, p.district,
                 coalesce(o.stat_date, to_date(p.updated_at)), to_date(p.updated_at), p.status
    """)
    return {
        "dws_station_hour": station_hour,
        "dws_station_day": station_day,
        "dws_region_day": region_day,
        "dws_pile_day": pile_day,
    }


def build_ads(spark: SparkSession, dws: dict[str, DataFrame], inputs: dict[str, DataFrame]) -> dict[str, DataFrame]:
    for table, frame in dws.items():
        frame.createOrReplaceTempView(table)
    inputs["dwd_pile_detail"].createOrReplaceTempView("dwd_pile_detail")

    overview = spark.sql("""
        SELECT first(batch_id) AS batch_id, min(stat_date) AS range_from, max(stat_date) AS range_to,
               sum(order_count) AS order_count, sum(energy_kwh) AS energy_kwh,
               sum(revenue_fen) AS revenue_fen,
               (SELECT count(*) FROM dwd_pile_detail WHERE status <> 'OFFLINE') AS online_pile_count,
               least(1D, greatest(0D, sum(charging_pile_minutes) / sum(capacity_pile_minutes))) AS utilization_rate,
               current_timestamp() AS calculated_at
        FROM dws_station_day
    """)
    station_rank = spark.sql("""
        SELECT station_id, station_name, district, sum(energy_kwh) AS energy_kwh,
               sum(revenue_fen) AS revenue_fen,
               least(1D, greatest(0D, sum(charging_pile_minutes) / sum(capacity_pile_minutes))) AS utilization_rate,
               row_number() OVER (ORDER BY sum(energy_kwh) DESC, station_id) AS rank,
               first(batch_id) AS batch_id, max(calculated_at) AS calculated_at
        FROM dws_station_day
        GROUP BY station_id, station_name, district
    """)
    energy_trend = spark.sql("""
        SELECT stat_date, cast(NULL AS BIGINT) AS station_id,
               sum(energy_kwh) AS energy_kwh, sum(order_count) AS order_count,
               first(batch_id) AS batch_id, max(calculated_at) AS calculated_at
        FROM dws_station_day GROUP BY stat_date
        UNION ALL
        SELECT stat_date, station_id, energy_kwh, order_count, batch_id, calculated_at
        FROM dws_station_day
    """)
    revenue_trend = spark.sql("""
        SELECT stat_date, cast(NULL AS BIGINT) AS station_id,
               sum(revenue_fen) AS revenue_fen, sum(order_count) AS order_count,
               first(batch_id) AS batch_id, max(calculated_at) AS calculated_at
        FROM dws_station_day GROUP BY stat_date
        UNION ALL
        SELECT stat_date, station_id, revenue_fen, order_count, batch_id, calculated_at
        FROM dws_station_day
    """)
    pile_status = spark.sql("""
        SELECT status, count(*) AS count,
               count(*) / sum(count(*)) OVER () AS ratio,
               first(batch_id) AS batch_id, current_timestamp() AS calculated_at
        FROM dwd_pile_detail GROUP BY status
    """)
    hour_heatmap = spark.sql("""
        SELECT dayofweek(hour_start) AS day_of_week, hour(hour_start) AS hour,
               sum(business_energy_kwh) AS energy_kwh,
               least(1D, greatest(0D, sum(charging_pile_minutes) / sum(total_pile_count * 60D))) AS utilization_rate,
               first(batch_id) AS batch_id, max(calculated_at) AS calculated_at
        FROM dws_station_hour GROUP BY dayofweek(hour_start), hour(hour_start)
    """)
    return {
        "ads_dashboard_overview": overview,
        "ads_station_rank": station_rank,
        "ads_energy_trend": energy_trend,
        "ads_revenue_trend": revenue_trend,
        "ads_pile_status": pile_status,
        "ads_hour_heatmap": hour_heatmap,
    }


def json_value(value: Any) -> Any:
    if isinstance(value, datetime):
        return value.isoformat(sep=" ")
    if isinstance(value, date):
        return value.isoformat()
    return value


def collect_quality(spark: SparkSession, root: str, business_date: str, batch_id: str) -> dict[str, Any]:
    path = f"{root}/quality/reports/dt={business_date}/batch={batch_id}"
    row = spark.read.parquet(path).select("report_json").first()
    if not row or not row.report_json:
        raise ValueError("quality report is empty")
    payload = json.loads(row.report_json)
    rules: dict[str, int] = {}
    for dataset in payload.get("datasets", []):
        for rule_id, count in dataset.get("rules", {}).items():
            rules[rule_id] = rules.get(rule_id, 0) + int(count)
    return {
        "sourceRows": sum(int(x.get("source_rows", 0)) for x in payload.get("datasets", [])),
        "acceptedRows": sum(int(x.get("accepted_rows", 0)) for x in payload.get("datasets", [])),
        "rejectedRows": sum(int(x.get("rejected_rows", 0)) for x in payload.get("datasets", [])),
        "rules": [{"ruleId": key, "count": rules[key]} for key in sorted(rules)],
    }


def build_snapshot(ads: dict[str, DataFrame], dws: dict[str, DataFrame], quality: dict[str, Any], batch_id: str) -> dict[str, Any]:
    overview = ads["ads_dashboard_overview"].first()
    generated_at = datetime.now(timezone.utc).isoformat()
    energy = ads["ads_energy_trend"].where("station_id IS NULL").orderBy("stat_date").collect()
    revenue = ads["ads_revenue_trend"].where("station_id IS NULL").orderBy("stat_date").collect()
    station_days = dws["dws_station_day"].orderBy("stat_date", "station_id").collect()
    query_days = [{"stationId": int(x.station_id), "stationName": x.station_name, "district": x.district,
                   "date": str(x.stat_date), "orderCount": int(x.order_count or 0),
                   "energyKwh": round(float(x.energy_kwh or 0), 4), "revenueFen": int(x.revenue_fen or 0),
                   "chargingPileMinutes": round(float(x.charging_pile_minutes or 0), 4),
                   "capacityPileMinutes": round(float(x.capacity_pile_minutes or 0), 4),
                   "availableCount": round(float(x.available_count or 0), 4),
                   "totalPileCount": int(x.total_pile_count or 0)} for x in station_days]
    return {
        "meta": {"batchId": batch_id, "generatedAt": generated_at,
                 "rangeFrom": str(overview.range_from), "rangeTo": str(overview.range_to),
                 "source": "spark-sql-ads"},
        "overview": {"orderCount": int(overview.order_count or 0), "energyKwh": round(float(overview.energy_kwh or 0), 2),
                     "revenueFen": int(overview.revenue_fen or 0), "onlinePileCount": int(overview.online_pile_count or 0),
                     "utilizationRate": round(float(overview.utilization_rate or 0), 4)},
        "energyTrend": {"items": [{"date": str(x.stat_date), "energyKwh": round(float(x.energy_kwh or 0), 2),
                                     "orderCount": int(x.order_count or 0)} for x in energy]},
        "revenueTrend": {"items": [{"date": str(x.stat_date), "revenueFen": int(x.revenue_fen or 0),
                                      "orderCount": int(x.order_count or 0)} for x in revenue]},
        "stationRanking": {"items": [{"stationId": int(x.station_id), "stationName": x.station_name,
                                         "district": x.district, "energyKwh": round(float(x.energy_kwh or 0), 2),
                                         "revenueFen": int(x.revenue_fen or 0), "utilizationRate": round(float(x.utilization_rate or 0), 4),
                                         "rank": int(x.rank)} for x in ads["ads_station_rank"].orderBy("rank").collect()]},
        "pileStatus": {"items": [{"status": x.status, "count": int(x["count"]), "ratio": round(float(x.ratio), 4)}
                                   for x in ads["ads_pile_status"].orderBy("status").collect()]},
        "hourlyHeatmap": {"items": [{"dayOfWeek": int(x.day_of_week), "hour": int(x.hour),
                                        "energyKwh": round(float(x.energy_kwh or 0), 2),
                                        "utilizationRate": round(float(x.utilization_rate or 0), 4)}
                                       for x in ads["ads_hour_heatmap"].orderBy("day_of_week", "hour").collect()]},
        "stationUtilization": {"items": [{"stationId": int(x.station_id), "stationName": x.station_name,
                                             "date": str(x.stat_date), "utilizationRate": round(float(x.utilization_rate or 0), 4),
                                             "availableCount": int(round(float(x.available_count or 0))),
                                             "totalPileCount": int(x.total_pile_count or 0)} for x in station_days]},
        "prediction": {"items": []},
        "dataQuality": quality,
        "_query": {"stationDays": query_days},
    }


def write_snapshot(payload: dict[str, Any], output: str) -> None:
    path = Path(output).resolve()
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(payload, ensure_ascii=False, indent=2, default=json_value), encoding="utf-8")
    os.replace(temporary, path)


def main() -> None:
    args = parse_args()
    root = args.hdfs_root.rstrip("/")
    spark = SparkSession.builder.appName(f"evcharge-warehouse-{args.batch_id}").getOrCreate()
    spark.sparkContext.setLogLevel("WARN")
    spark.conf.set("spark.sql.session.timeZone", "Asia/Shanghai")
    try:
        inputs = {table: spark.read.parquet(dwd_path(root, table, args.business_date, args.batch_id)) for table in DWD_TABLES}
        validate_inputs(inputs, args.batch_id)
        dws = build_dws(spark, inputs, args.batch_id)
        ads = build_ads(spark, dws, inputs)
        report: list[dict[str, Any]] = []
        for layer, frames in (("dws", dws), ("ads", ads)):
            for table, frame in frames.items():
                target = warehouse_path(root, layer, table, args.business_date, args.batch_id)
                write_batch(frame, target, args.replace)
                report.append({"layer": layer, "table": table, "path": target, "rows": frame.count()})
        quality = collect_quality(spark, root, args.business_date, args.batch_id)
        write_snapshot(build_snapshot(ads, dws, quality, args.batch_id), args.snapshot_output)
        manifest = spark.createDataFrame([(json.dumps({"batch_id": args.batch_id, "business_date": args.business_date,
                                                       "tables": report}, ensure_ascii=False),)], ["manifest_json"])
        write_batch(manifest, f"{root}/ads/_manifests/dt={args.business_date}/batch={args.batch_id}", args.replace)
        print(json.dumps({"batch_id": args.batch_id, "tables": report, "snapshot": str(Path(args.snapshot_output).resolve())},
                         ensure_ascii=False, indent=2))
    finally:
        spark.stop()


if __name__ == "__main__":
    try:
        main()
    except Exception as error:  # Spark exceptions vary by runtime.
        print(f"Warehouse build failed: {error}", file=sys.stderr)
        raise SystemExit(1)
