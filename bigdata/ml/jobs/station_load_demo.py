"""从标准 DWS 到 ADS 预测的 Spark MLlib 演示作业。

功能：只读取完整流水线发布的 ``dws_station_hour``，完成契约校验、特征工程、
时间切分、模型评估以及 ``ads_prediction`` 发布。数据生成、ODS、质量治理和
DWD 均由上游流水线负责，本作业不再提供绕过上游的 Raw 数据入口。
"""

from __future__ import annotations

import argparse
import json
import math
from datetime import datetime, timezone
from pathlib import Path

from pyspark.ml.evaluation import RegressionEvaluator
from pyspark.ml.feature import VectorAssembler
from pyspark.ml.regression import GBTRegressor
from pyspark.sql import DataFrame, SparkSession, Window
from pyspark.sql import functions as F


FEATURE_VERSION = "station-hour-v1"
HORIZONS = (1, 6, 24)
DWS_REQUIRED_COLUMNS = (
    "station_id", "station_name", "hour_start", "total_pile_count", "session_starts",
    "energy_kwh", "charging_pile_minutes", "average_occupied_count",
    "average_available_count", "station_load", "batch_id", "source_type", "dt",
)
FEATURE_COLUMNS = (
    "lag_1", "lag_24", "lag_168", "rolling_mean_24", "rolling_mean_168",
    "hour_sin", "hour_cos", "dow_sin", "dow_cos", "total_pile_count",
)


def spark_path(value: str | Path) -> str:
    """保留显式 HDFS/S3 URI；普通路径转换为 Spark 可识别的本地 URI。"""
    text = str(value)
    return text if "://" in text else Path(text).resolve().as_uri()


def parse_args() -> argparse.Namespace:
    """读取唯一的标准 DWS 输入和本地演示产物目录。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dws-input", required=True, help="C 或 Demo 桥接层发布的 dws_station_hour Parquet 路径")
    parser.add_argument("--quality-report", required=True, help="同批次质量作业发布的 report Parquet 路径")
    parser.add_argument("--output", default="bigdata/runtime/demo")
    return parser.parse_args()


def validate_dws(frame: DataFrame) -> dict:
    """验证 C→E 输入边界，避免字段漂移或非法数据静默进入模型。"""
    missing = sorted(set(DWS_REQUIRED_COLUMNS) - set(frame.columns))
    if missing:
        raise ValueError(f"dws_station_hour 缺少字段: {', '.join(missing)}")

    duplicate_rows = frame.groupBy("station_id", "hour_start").count().where(F.col("count") > 1).count()
    invalid_rows = frame.where(
        F.col("station_id").isNull()
        | F.col("hour_start").isNull()
        | F.col("total_pile_count").isNull()
        | (F.col("total_pile_count") <= 0)
        | F.col("station_load").isNull()
        | (F.col("station_load") < 0)
        | (F.col("station_load") > 1)
        | F.col("average_available_count").isNull()
        | (F.col("average_available_count") < 0)
        | (F.col("average_available_count") > F.col("total_pile_count"))
    ).count()
    batch_count = frame.select("batch_id").distinct().count()
    if duplicate_rows or invalid_rows or batch_count != 1:
        raise ValueError(
            "dws_station_hour 输入不合法: "
            f"duplicateKeys={duplicate_rows}, invalidRows={invalid_rows}, batches={batch_count}"
        )
    row_count = frame.count()
    return {
        "sourceRows": row_count,
        "acceptedRows": row_count,
        "rejectedRows": 0,
        "rules": [],
        "contract": "dws_station_hour-v1",
    }


def load_contract_dws(spark: SparkSession, input_path: str) -> tuple[DataFrame, dict]:
    """只读加载 C 发布的标准 DWS，并执行 E 侧消费前契约校验。"""
    frame = spark.read.parquet(spark_path(input_path))
    report = validate_dws(frame)
    return frame.select(*DWS_REQUIRED_COLUMNS), report


def load_quality_report(spark: SparkSession, report_path: str, dws_validation: dict) -> dict:
    """汇总 B 的质量报告，并附上 ML 消费 DWS 时执行的契约检查结果。"""
    row = spark.read.parquet(spark_path(report_path)).select("report_json").first()
    if row is None or not row.report_json:
        raise ValueError("质量报告为空，不能发布本次模型结果。")
    payload = json.loads(row.report_json)
    datasets = payload.get("datasets", [])
    rule_counts: dict[str, int] = {}
    for dataset in datasets:
        for rule_id, count in dataset.get("rules", {}).items():
            rule_counts[rule_id] = rule_counts.get(rule_id, 0) + int(count)
    return {
        "sourceRows": sum(int(item.get("source_rows", 0)) for item in datasets),
        "acceptedRows": sum(int(item.get("accepted_rows", 0)) for item in datasets),
        "rejectedRows": sum(int(item.get("rejected_rows", 0)) for item in datasets),
        "rules": [{"ruleId": key, "count": rule_counts[key]} for key in sorted(rule_counts)],
        "batchId": payload.get("batch_id"),
        "dwsContract": dws_validation,
    }


def build_features(dws: DataFrame) -> DataFrame:
    """只使用目标时间之前的信息构造周期、滞后和滚动特征。"""
    ordered = Window.partitionBy("station_id").orderBy("hour_start")
    trailing_24 = ordered.rowsBetween(-24, -1)
    trailing_168 = ordered.rowsBetween(-168, -1)
    frame = dws
    for lag in (1, 24, 168):
        frame = frame.withColumn(f"lag_{lag}", F.lag("station_load", lag).over(ordered))
    return (
        frame.withColumn("rolling_mean_24", F.avg("station_load").over(trailing_24))
        .withColumn("rolling_mean_168", F.avg("station_load").over(trailing_168))
        .withColumn("hour", F.hour("hour_start"))
        .withColumn("dow", F.dayofweek("hour_start"))
        .withColumn("hour_sin", F.sin(F.col("hour") * F.lit(2 * math.pi / 24)))
        .withColumn("hour_cos", F.cos(F.col("hour") * F.lit(2 * math.pi / 24)))
        .withColumn("dow_sin", F.sin((F.col("dow") - 1) * F.lit(2 * math.pi / 7)))
        .withColumn("dow_cos", F.cos((F.col("dow") - 1) * F.lit(2 * math.pi / 7)))
        .na.drop(subset=list(FEATURE_COLUMNS))
    )


def evaluate(rows: DataFrame, prediction_col: str) -> dict[str, float]:
    """按 ML Contract 返回 MAE、RMSE 与 R²。"""
    return {
        metric: float(RegressionEvaluator(labelCol="label", predictionCol=prediction_col, metricName=metric).evaluate(rows))
        for metric in ("mae", "rmse", "r2")
    }


def train_models(features: DataFrame, output: Path, batch_id: str) -> tuple[list[dict], list[dict]]:
    """按全局时间顺序 70/15/15 切分，并与七天周期 baseline 比较。"""
    bounds = features.select(
        F.min(F.unix_timestamp("hour_start")).alias("lo"),
        F.max(F.unix_timestamp("hour_start")).alias("hi"),
    ).first()
    split_70 = bounds.lo + (bounds.hi - bounds.lo) * 0.70
    split_85 = bounds.lo + (bounds.hi - bounds.lo) * 0.85
    vectorizer = VectorAssembler(inputCols=list(FEATURE_COLUMNS), outputCol="features")
    reports: list[dict] = []
    prediction_rows: list[dict] = []
    ordered = Window.partitionBy("station_id").orderBy("hour_start")
    generated_at = datetime.now(timezone.utc).isoformat(timespec="seconds")

    for horizon in HORIZONS:
        labelled = (
            features.withColumn("label", F.lead("station_load", horizon).over(ordered))
            .withColumn("baseline", F.lag("station_load", 168 - horizon).over(ordered))
            .na.drop(subset=["label", "baseline"])
        )
        encoded = vectorizer.transform(labelled)
        epoch = F.unix_timestamp("hour_start")
        train = encoded.where(epoch <= split_70).cache()
        validation = encoded.where((epoch > split_70) & (epoch <= split_85)).cache()
        test = encoded.where(epoch > split_85).cache()

        model = GBTRegressor(featuresCol="features", labelCol="label", maxIter=16, maxDepth=4, seed=24).fit(train)
        validation_gbt = evaluate(model.transform(validation), "prediction")
        validation_baseline = evaluate(validation, "baseline")
        use_gbt = validation_gbt["mae"] <= validation_baseline["mae"] * 0.99
        test_scored = model.transform(test) if use_gbt else test.withColumn("prediction", F.col("baseline"))
        metrics = evaluate(test_scored, "prediction")
        baseline_metrics = evaluate(test, "baseline")
        model_name = "SparkMLlib-GBT" if use_gbt else "lag_168-baseline"
        model_version = f"station-load-v1-h{horizon}"
        if use_gbt:
            model.write().overwrite().save(spark_path(output / "models" / f"horizon_{horizon}h"))

        forecast_input = (
            features.withColumn("baseline", F.lag("station_load", 168 - horizon).over(ordered))
            .withColumn("rn", F.row_number().over(ordered.orderBy(F.desc("hour_start"))))
            .where("rn = 1")
            .na.drop(subset=["baseline"])
        )
        forecast_encoded = vectorizer.transform(forecast_input)
        forecast_scored = model.transform(forecast_encoded) if use_gbt else forecast_encoded.withColumn("prediction", F.col("baseline"))
        # 目标时间在 Spark 内计算并格式化，避免 Driver 系统时区影响 Python datetime。
        forecast_rows = forecast_scored.withColumn(
            "target_time", F.expr(f"hour_start + INTERVAL {horizon} HOURS")
        ).select(
            "station_id", "station_name", "total_pile_count", "prediction",
            F.date_format("target_time", "yyyy-MM-dd HH:mm:ss").alias("prediction_time"),
        )
        for row in forecast_rows.collect():
            load = max(0.0, min(1.0, float(row.prediction)))
            prediction_rows.append({
                "station_id": int(row.station_id),
                "station_name": row.station_name,
                "prediction_time": row.prediction_time,
                "horizon": f"{horizon}h",
                "predicted_load": round(load, 4),
                "predicted_available_count": max(0, int(round(float(row.total_pile_count) * (1 - load)))),
                "peak_level": "HIGH" if load >= 0.70 else "MEDIUM" if load >= 0.40 else "LOW",
                "model_name": model_name,
                "model_version": model_version,
                "generated_at": generated_at,
                "mae": round(metrics["mae"], 4),
                "rmse": round(metrics["rmse"], 4),
                "r2": round(metrics["r2"], 4),
                "batch_id": batch_id,
                "feature_version": FEATURE_VERSION,
            })
        reports.append({
            "horizon": f"{horizon}h",
            "selectedModel": model_name,
            "modelVersion": model_version,
            "featureVersion": FEATURE_VERSION,
            "batchId": batch_id,
            "metrics": metrics,
            "baselineMetrics": baseline_metrics,
            "validationMae": {"gbt": validation_gbt["mae"], "baseline": validation_baseline["mae"]},
            "rows": {"train": train.count(), "validation": validation.count(), "test": test.count()},
            "features": list(FEATURE_COLUMNS),
        })
        train.unpersist(); validation.unpersist(); test.unpersist()
    return reports, prediction_rows


def dashboard_payload(dws: DataFrame, quality: dict, predictions: list[dict], batch_id: str) -> dict:
    """将标准 DWS 和预测结果转换成现有 Dashboard Demo 的九类资源。"""
    daily = dws.groupBy(F.to_date("hour_start").alias("date")).agg(
        F.sum("energy_kwh").alias("energy"), F.sum("session_starts").alias("orders")
    ).orderBy("date").tail(14)
    latest_time = dws.agg(F.max("hour_start")).first()[0]
    latest = dws.where(F.col("hour_start") == latest_time).cache()
    overview = dws.agg(
        F.sum("session_starts").alias("orders"), F.sum("energy_kwh").alias("energy"),
        F.avg("station_load").alias("utilization"),
    ).first()
    ranking = dws.groupBy("station_id", "station_name").agg(
        F.sum("energy_kwh").alias("energy"), F.avg("station_load").alias("utilization")
    ).orderBy(F.desc("energy")).collect()
    heatmap = dws.groupBy(
        F.dayofweek("hour_start").alias("dow"), F.hour("hour_start").alias("hour")
    ).agg(F.sum("energy_kwh").alias("energy"), F.avg("station_load").alias("utilization")).collect()
    pile = latest.agg(
        F.sum("total_pile_count").alias("total"), F.sum("average_occupied_count").alias("charging")
    ).first()
    total, charging = int(round(pile.total)), int(round(pile.charging))
    energy_trend = [{"date": str(row.date), "energyKwh": round(row.energy, 2), "orderCount": int(round(row.orders))} for row in daily]
    revenue_trend = [{"date": row["date"], "revenueFen": int(round(row["energyKwh"] * 120)), "orderCount": row["orderCount"]} for row in energy_trend]
    dashboard_predictions = [{
        "stationId": row["station_id"], "stationName": row["station_name"],
        "predictionTime": row["prediction_time"], "horizon": row["horizon"],
        "predictedLoad": row["predicted_load"],
        "predictedAvailableCount": row["predicted_available_count"],
        "peakLevel": row["peak_level"], "modelName": row["model_name"],
        "modelVersion": row["model_version"], "mae": row["mae"], "rmse": row["rmse"], "r2": row["r2"],
    } for row in predictions]
    payload = {
        "meta": {
            "batchId": batch_id,
            "generatedAt": datetime.now(timezone.utc).isoformat(),
            "notice": "数据来自完整 Raw/ODS/DQ/DWD/DWS 演示流水线；ML 只消费标准 DWS。",
        },
        "overview": {
            "orderCount": int(round(overview.orders)), "energyKwh": round(overview.energy, 2),
            "revenueFen": int(round(overview.energy * 120)), "onlinePileCount": total,
            "utilizationRate": round(float(overview.utilization), 4),
        },
        "energyTrend": {"items": energy_trend},
        "revenueTrend": {"items": revenue_trend},
        "stationRanking": {"items": [{
            "stationId": int(row.station_id), "stationName": row.station_name,
            "district": "分析站点", "energyKwh": round(row.energy, 2),
            "revenueFen": int(round(row.energy * 120)),
            "utilizationRate": round(float(row.utilization), 4), "rank": index + 1,
        } for index, row in enumerate(ranking)]},
        "pileStatus": {"items": [
            {"status": "CHARGING", "count": charging, "ratio": round(charging / total, 4)},
            {"status": "AVAILABLE", "count": total - charging, "ratio": round((total - charging) / total, 4)},
        ]},
        "hourlyHeatmap": {"items": [{
            "dayOfWeek": int(row.dow), "hour": int(row.hour), "energyKwh": round(row.energy, 2),
            "utilizationRate": round(float(row.utilization), 4),
        } for row in heatmap]},
        "stationUtilization": {"items": [{
            "stationId": int(row.station_id), "stationName": row.station_name,
            "date": str(latest_time.date()), "utilizationRate": round(float(row.station_load), 4),
            "availableCount": int(round(row.average_available_count)),
            "totalPileCount": int(round(row.total_pile_count)),
        } for row in latest.collect()]},
        "prediction": {"items": dashboard_predictions},
        "dataQuality": quality,
    }
    latest.unpersist()
    return payload


def main() -> None:
    """串联标准 DWS 校验、训练、评估和演示输出。"""
    args = parse_args()
    output = Path(args.output).resolve()
    output.mkdir(parents=True, exist_ok=True)
    spark = SparkSession.builder.appName("EVCharge-Spark-MLlib-Demo").master("local[*]").getOrCreate()
    spark.sparkContext.setLogLevel("WARN")
    spark.conf.set("spark.sql.session.timeZone", "Asia/Shanghai")
    try:
        dws, dws_validation = load_contract_dws(spark, args.dws_input)
        quality = load_quality_report(spark, args.quality_report, dws_validation)
        batch_ids = [row.batch_id for row in dws.select("batch_id").distinct().collect()]
        if len(batch_ids) != 1:
            raise ValueError(f"一次训练必须且只能使用一个 batch_id，实际为 {batch_ids}")
        batch_id = batch_ids[0]
        if quality.get("batchId") != batch_id:
            raise ValueError(
                f"DWS batch_id={batch_id} 与质量报告 batch_id={quality.get('batchId')} 不一致"
            )

        dws.write.mode("overwrite").parquet(spark_path(output / "dws_station_hour"))
        reports, predictions = train_models(build_features(dws), output, batch_id)
        # Parquet 交付保持 timestamp 类型；Dashboard JSON 再序列化为字符串。
        prediction_frame = (
            spark.createDataFrame(predictions)
            .withColumn("prediction_time", F.to_timestamp("prediction_time"))
            .withColumn("generated_at", F.to_timestamp("generated_at"))
        )
        prediction_frame.write.mode("overwrite").parquet(spark_path(output / "ads_prediction"))
        (output / "training_report.json").write_text(
            json.dumps({"inputMode": "pipeline-dws", "batchId": batch_id, "models": reports}, ensure_ascii=False, indent=2),
            encoding="utf-8",
        )
        (output / "dashboard.json").write_text(
            json.dumps(dashboard_payload(dws, quality, predictions, batch_id), ensure_ascii=False, indent=2),
            encoding="utf-8",
        )
        print(json.dumps({
            "inputMode": "pipeline-dws", "batchId": batch_id, "models": reports,
            "dashboard": str(output / "dashboard.json"),
        }, ensure_ascii=False))
    finally:
        spark.stop()


if __name__ == "__main__":
    main()
