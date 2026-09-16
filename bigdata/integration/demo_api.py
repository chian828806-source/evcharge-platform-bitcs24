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


def read_snapshot() -> dict[str, Any]:
    """读取 ML 发布的 UTF-8 JSON；真实模式禁止用联调样本冒充完成批次。"""
    import json
    if not SNAPSHOT_PATH.is_file():
        raise FileNotFoundError(f"Dashboard snapshot is not ready: {SNAPSHOT_PATH}")
    return json.loads(SNAPSHOT_PATH.read_text(encoding="utf-8"))


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
        try:
            snapshot = read_snapshot()
        except (OSError, ValueError) as error:
            return jsonify({"error": {
                "code": "BATCH_NOT_READY",
                "message": str(error),
            }}), 409
        return envelope(snapshot, filter_items(normalized, snapshot[key]))

    return app


app = create_app()

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=int(os.getenv("PORT", "5000")), debug=False)
