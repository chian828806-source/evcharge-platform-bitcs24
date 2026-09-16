"""第二阶段 Dashboard 演示 API。

功能：读取 PySpark 作业生成的 dashboard.json，并按照 24-DASHBOARD-API.md
提供九个只读 REST 接口。每次请求都重新读取快照，训练完成后无需重启服务。
"""

from __future__ import annotations

import os
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

from flask import Flask, jsonify, request
from flask_cors import CORS


PROJECT_ROOT = Path(__file__).resolve().parents[2]
SNAPSHOT_PATH = Path(os.getenv("EVCHARGE_DASHBOARD_SNAPSHOT", PROJECT_ROOT / "bigdata/runtime/demo/dashboard.json"))

RESOURCE_NAMES = {
    "overview": "overview",
    "energy-trend": "energyTrend",
    "revenue-trend": "revenueTrend",
    "station-ranking": "stationRanking",
    "pile-status": "pileStatus",
    "hourly-heatmap": "hourlyHeatmap",
    "station-utilization": "stationUtilization",
    "prediction": "prediction",
    "data-quality/summary": "dataQuality",
}

CANONICAL_RESOURCES = tuple(
    f"dashboard/{name}" if name != "data-quality/summary" else name
    for name in RESOURCE_NAMES
)


def fallback_snapshot() -> dict[str, Any]:
    """训练尚未运行时返回完整的小样本，使前端仍可独立联调。"""
    generated = datetime.now(timezone.utc).isoformat()
    return {
        "meta": {"batchId": "PIPELINE-DEMO-FALLBACK", "generatedAt": generated,
                 "notice": "接口使用内置联调样本；运行完整流水线后自动切换为当前批次结果。"},
        "overview": {"orderCount": 286, "energyKwh": 1842.4, "revenueFen": 221088,
                     "onlinePileCount": 128, "utilizationRate": 0.63},
        "energyTrend": {"items": [{"date": "2023-02-26", "energyKwh": 1690.2, "orderCount": 252},
                                    {"date": "2023-02-27", "energyKwh": 1778.6, "orderCount": 271},
                                    {"date": "2023-02-28", "energyKwh": 1842.4, "orderCount": 286}]},
        "revenueTrend": {"items": [{"date": "2023-02-26", "revenueFen": 202824, "orderCount": 252},
                                     {"date": "2023-02-27", "revenueFen": 213432, "orderCount": 271},
                                     {"date": "2023-02-28", "revenueFen": 221088, "orderCount": 286}]},
        "stationRanking": {"items": [{"stationId": 102, "stationName": "演示站点 102", "district": "演示区域",
            "energyKwh": 486.2, "revenueFen": 58344, "utilizationRate": 0.78, "rank": 1},
            {"stationId": 104, "stationName": "演示站点 104", "district": "演示区域",
             "energyKwh": 394.8, "revenueFen": 47376, "utilizationRate": 0.66, "rank": 2}]},
        "pileStatus": {"items": [{"status": "CHARGING", "count": 81, "ratio": 0.6328},
                                   {"status": "AVAILABLE", "count": 47, "ratio": 0.3672}]},
        "hourlyHeatmap": {"items": [{"dayOfWeek": day, "hour": hour,
            "energyKwh": round(12 + hour * 1.7 + day * 2.1, 2),
            "utilizationRate": round(min(0.92, 0.18 + hour / 35 + day / 50), 4)}
            for day in range(1, 8) for hour in range(24)]},
        "stationUtilization": {"items": [{"stationId": 102, "stationName": "演示站点 102", "date": "2023-02-28",
            "utilizationRate": 0.78, "availableCount": 16, "totalPileCount": 72},
            {"stationId": 104, "stationName": "演示站点 104", "date": "2023-02-28",
             "utilizationRate": 0.66, "availableCount": 19, "totalPileCount": 56}]},
        "prediction": {"items": [{"stationId": 102, "stationName": "演示站点 102", "predictionTime": "2023-02-28 23:00:00",
            "horizon": horizon, "predictedLoad": load, "predictedAvailableCount": available,
            "peakLevel": level, "modelName": "SparkMLlib-GBT", "mae": 0.071, "rmse": 0.098}
            for horizon, load, available, level in (("1h", 0.72, 20, "HIGH"), ("6h", 0.61, 28, "MEDIUM"), ("24h", 0.55, 32, "MEDIUM"))]},
        "dataQuality": {"sourceRows": 34752, "acceptedRows": 34590, "rejectedRows": 162,
                        "rules": [{"ruleId": "INVALID_TIMESTAMP", "count": 0},
                                  {"ruleId": "MISSING_VALUE", "count": 37},
                                  {"ruleId": "OUT_OF_CAPACITY_RANGE", "count": 125}]},
    }


def read_snapshot() -> dict[str, Any]:
    """读取 UTF-8 JSON；文件缺失或损坏时降级到可辨识的联调样本。"""
    try:
        import json
        return json.loads(SNAPSHOT_PATH.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return fallback_snapshot()


def envelope(snapshot: dict[str, Any], data: Any):
    """统一添加批次和生成时间，保持 API V1 契约结构。"""
    meta = snapshot.get("meta", {})
    return jsonify({"data": data, "meta": {
        "batchId": meta.get("batchId", "UNKNOWN"),
        "generatedAt": meta.get("generatedAt", datetime.now(timezone.utc).isoformat()),
    }})


def filter_items(resource: str, data: Any) -> Any:
    """支持文档约定的 stationId/from/to 查询参数；无参数时原样返回。"""
    if not isinstance(data, dict) or not isinstance(data.get("items"), list):
        return data
    rows = data["items"]
    station_id = request.args.get("stationId", type=int)
    date_from, date_to = request.args.get("from"), request.args.get("to")
    if station_id is not None:
        rows = [row for row in rows if row.get("stationId") == station_id]
    if date_from or date_to:
        date_key = "predictionTime" if resource == "prediction" else "date"
        rows = [row for row in rows if (not date_from or str(row.get(date_key, "")) >= date_from)
                and (not date_to or str(row.get(date_key, ""))[:10] <= date_to)]
    return {"items": rows}


def create_app() -> Flask:
    """创建可被测试或命令行复用的 Flask 应用。"""
    app = Flask(__name__)
    CORS(app, resources={r"/api/*": {"origins": "*"}})

    @app.get("/health")
    def health():
        return jsonify({"status": "ok", "snapshot": str(SNAPSHOT_PATH), "snapshotExists": SNAPSHOT_PATH.exists()})

    @app.get("/api/v1/<path:resource>")
    def dashboard_resource(resource: str):
        normalized = resource.removeprefix("dashboard/")
        key = RESOURCE_NAMES.get(normalized)
        if key is None:
            return jsonify({"error": {"code": "NOT_FOUND", "message": "Unknown dashboard resource."}}), 404
        snapshot = read_snapshot()
        return envelope(snapshot, filter_items(normalized, snapshot[key]))

    return app


app = create_app()

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=int(os.getenv("PORT", "5000")), debug=False)
