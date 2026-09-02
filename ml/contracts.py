"""校验 ML 与 Qt/C++ 服务端之间的预测 JSON 公共契约。"""

from __future__ import annotations

import math
from datetime import datetime
from typing import Any


HORIZONS = {"1h", "6h", "24h"}
PEAK_LEVELS = {"LOW", "MEDIUM", "HIGH"}


def _aware_datetime(value: Any, field_name: str) -> datetime:
    """把 ISO 8601 字符串解析成带时区时间，拒绝含义不明确的本地时间。"""

    if not isinstance(value, str):
        raise ValueError(f"{field_name} must be an ISO 8601 string")
    try:
        parsed = datetime.fromisoformat(value.replace("Z", "+00:00"))
    except ValueError as exc:
        raise ValueError(f"{field_name} is not a valid ISO 8601 time") from exc
    if parsed.tzinfo is None or parsed.utcoffset() is None:
        raise ValueError(f"{field_name} must include a timezone offset")
    return parsed


def _finite_number(value: Any, field_name: str) -> float:
    """读取有限数值；布尔值虽是 Python 整数子类，也不视为业务数值。"""

    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ValueError(f"{field_name} must be a number")
    result = float(value)
    if not math.isfinite(result):
        raise ValueError(f"{field_name} must be finite")
    return result


def validate_prediction_document(document: Any) -> dict[str, Any]:
    """按 docs/03-API.md 校验完整批次并返回原对象。"""

    if not isinstance(document, dict):
        raise ValueError("prediction document must be an object")
    if document.get("schemaVersion") != "1.0":
        raise ValueError("schemaVersion must be 1.0")
    batch_id = document.get("batchId")
    if not isinstance(batch_id, str) or not batch_id.strip():
        raise ValueError("batchId must be a non-empty string")
    predictions = document.get("predictions")
    if not isinstance(predictions, list) or not predictions:
        raise ValueError("predictions must be a non-empty array")

    unique_keys: set[tuple[int, str, str]] = set()
    for index, item in enumerate(predictions):
        prefix = f"predictions[{index}]"
        if not isinstance(item, dict):
            raise ValueError(f"{prefix} must be an object")
        station_id = item.get("stationId")
        if isinstance(station_id, bool) or not isinstance(station_id, int):
            raise ValueError(f"{prefix}.stationId must be an integer")
        if station_id <= 0:
            raise ValueError(f"{prefix}.stationId must be positive")
        prediction_time = item.get("predictionTime")
        _aware_datetime(prediction_time, f"{prefix}.predictionTime")
        horizon = item.get("horizon")
        if horizon not in HORIZONS:
            raise ValueError(f"{prefix}.horizon must be 1h, 6h or 24h")
        load = _finite_number(item.get("predictedLoad"), f"{prefix}.predictedLoad")
        if not 0 <= load <= 1:
            raise ValueError(f"{prefix}.predictedLoad must be between 0 and 1")
        available = item.get("predictedAvailableCount")
        if isinstance(available, bool) or not isinstance(available, int):
            raise ValueError(f"{prefix}.predictedAvailableCount must be an integer")
        if available < 0:
            raise ValueError(f"{prefix}.predictedAvailableCount must be non-negative")
        if item.get("peakLevel") not in PEAK_LEVELS:
            raise ValueError(f"{prefix}.peakLevel must be LOW, MEDIUM or HIGH")
        if not isinstance(item.get("modelName"), str) or not item["modelName"].strip():
            raise ValueError(f"{prefix}.modelName must be a non-empty string")
        if "generatedAt" not in item:
            raise ValueError(f"{prefix}.generatedAt is required")
        _aware_datetime(item["generatedAt"], f"{prefix}.generatedAt")
        for metric in ("mae", "rmse"):
            if metric in item and _finite_number(item[metric], f"{prefix}.{metric}") < 0:
                raise ValueError(f"{prefix}.{metric} must be non-negative")

        key = (station_id, prediction_time, horizon)
        if key in unique_keys:
            raise ValueError(f"duplicate prediction key: {key}")
        unique_keys.add(key)
    return document


def peak_level(predicted_load: float) -> str:
    """将统一的 0 到 1 负荷率映射为大屏和数据库使用的三级峰值。"""

    if predicted_load >= 0.7:
        return "HIGH"
    if predicted_load >= 0.4:
        return "MEDIUM"
    return "LOW"
