"""第二阶段 Dashboard 演示 API。

功能：读取 PySpark 作业生成的 dashboard.json，并按照 24-DASHBOARD-API.md
提供分析 REST 接口；同时代理 Open-Meteo 实时天气，避免浏览器跨域和泄露上游细节。
分析快照每次请求都重新读取，训练完成后无需重启服务。
"""

from __future__ import annotations

import os
import json
import threading
import time
from copy import deepcopy
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Callable
from urllib.parse import urlencode
from urllib.request import urlopen

from flask import Flask, jsonify, request
from flask_cors import CORS


PROJECT_ROOT = Path(__file__).resolve().parents[2]
SNAPSHOT_PATH = Path(os.getenv("EVCHARGE_DASHBOARD_SNAPSHOT", PROJECT_ROOT / "bigdata/runtime/demo/dashboard.json"))
WEATHER_CACHE_TTL_SECONDS = int(os.getenv("EVCHARGE_WEATHER_CACHE_TTL_SECONDS", "1200"))
WEATHER_TIMEOUT_SECONDS = float(os.getenv("EVCHARGE_WEATHER_TIMEOUT_SECONDS", "4"))
WEATHER_CITY = os.getenv("EVCHARGE_WEATHER_CITY", "深圳")
WEATHER_LATITUDE = float(os.getenv("EVCHARGE_WEATHER_LATITUDE", "22.5431"))
WEATHER_LONGITUDE = float(os.getenv("EVCHARGE_WEATHER_LONGITUDE", "114.0579"))

WMO_WEATHER_TEXT = {
    0: "晴", 1: "晴间多云", 2: "多云", 3: "阴", 45: "雾", 48: "雾",
    51: "毛毛雨", 53: "毛毛雨", 55: "毛毛雨", 56: "冻毛毛雨", 57: "冻毛毛雨",
    61: "小雨", 63: "中雨", 65: "大雨", 66: "冻雨", 67: "冻雨",
    71: "小雪", 73: "中雪", 75: "大雪", 77: "米雪",
    80: "阵雨", 81: "阵雨", 82: "强阵雨", 85: "阵雪", 86: "强阵雪",
    95: "雷暴", 96: "雷暴伴冰雹", 99: "强雷暴伴冰雹",
}

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


def weather_text(code: int | None) -> str:
    """把 Open-Meteo 的 WMO 天气码转换为可直接展示的中文。"""
    return WMO_WEATHER_TEXT.get(code, "未知天气")


def fetch_json(url: str, timeout: float) -> dict[str, Any]:
    """通过标准库读取上游 JSON，避免为一个只读接口增加第三方依赖。"""
    with urlopen(url, timeout=timeout) as response:  # noqa: S310 - URL is assembled from a fixed host.
        return json.loads(response.read().decode("utf-8"))


class WeatherService:
    """封装 Open-Meteo 请求、字段映射、20 分钟缓存与透明降级。"""

    def __init__(self, fetcher: Callable[[str, float], dict[str, Any]] = fetch_json,
                 clock: Callable[[], float] = time.monotonic):
        self._fetcher = fetcher
        self._clock = clock
        self._cache: dict[str, Any] | None = None
        self._cached_at = 0.0
        self._lock = threading.Lock()

    @staticmethod
    def unavailable() -> dict[str, Any]:
        """在没有任何可用缓存时返回显式空值，不伪造实时天气。"""
        return {
            "city": WEATHER_CITY, "temperature": None, "apparentTemperature": None,
            "humidity": None, "precipitation": None, "windSpeed": None,
            "weatherCode": None, "weatherText": "暂不可用", "updatedAt": None,
            "available": False, "isStale": True, "source": "unavailable",
        }

    def _url(self) -> str:
        query = urlencode({
            "latitude": WEATHER_LATITUDE,
            "longitude": WEATHER_LONGITUDE,
            "current": "temperature_2m,apparent_temperature,relative_humidity_2m,precipitation,weather_code,wind_speed_10m",
            "timezone": "Asia/Shanghai",
        })
        return f"https://api.open-meteo.com/v1/forecast?{query}"

    def _load_upstream(self) -> dict[str, Any]:
        payload = self._fetcher(self._url(), WEATHER_TIMEOUT_SECONDS)
        current = payload.get("current")
        if not isinstance(current, dict):
            raise ValueError("Open-Meteo response does not contain current weather.")
        code = int(current["weather_code"])
        updated_at = str(current["time"])
        if len(updated_at) == 16:
            updated_at += ":00+08:00"
        return {
            "city": WEATHER_CITY,
            "temperature": float(current["temperature_2m"]),
            "apparentTemperature": float(current["apparent_temperature"]),
            "humidity": int(current["relative_humidity_2m"]),
            "precipitation": float(current["precipitation"]),
            "windSpeed": float(current["wind_speed_10m"]),
            "weatherCode": code,
            "weatherText": weather_text(code),
            "updatedAt": updated_at,
            "available": True,
            "isStale": False,
            "source": "open-meteo",
        }

    def current(self) -> dict[str, Any]:
        """优先返回有效缓存；上游失败时返回过期缓存或显式不可用对象。"""
        now = self._clock()
        with self._lock:
            if self._cache is not None and now - self._cached_at < WEATHER_CACHE_TTL_SECONDS:
                return deepcopy(self._cache)
        try:
            fresh = self._load_upstream()
        except (OSError, ValueError, KeyError, TypeError):
            with self._lock:
                if self._cache is None:
                    return self.unavailable()
                stale = deepcopy(self._cache)
            stale.update({"isStale": True, "source": "cache"})
            return stale
        with self._lock:
            self._cache, self._cached_at = deepcopy(fresh), now
        return fresh


weather_service = WeatherService()


def read_snapshot() -> dict[str, Any]:
    """读取 ML 发布的 UTF-8 JSON；真实模式禁止用联调样本冒充完成批次。"""
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


def create_app(weather_provider: Callable[[], dict[str, Any]] | None = None) -> Flask:
    """创建可被测试或命令行复用的 Flask 应用。"""
    app = Flask(__name__)
    CORS(app, resources={r"/api/*": {"origins": "*"}})
    provider = weather_provider or weather_service.current

    @app.get("/health")
    def health():
        return jsonify({"status": "ok", "snapshot": str(SNAPSHOT_PATH), "snapshotExists": SNAPSHOT_PATH.exists()})

    @app.get("/api/v1/context/weather")
    def current_weather():
        """返回深圳当前天气；接口永远保持可解析，失败状态由 data.available 表达。"""
        data = provider()
        return jsonify({
            "data": data,
            "meta": {
                "generatedAt": datetime.now(timezone.utc).isoformat(),
                "source": data.get("source", "unavailable"),
                "cacheTtlSeconds": WEATHER_CACHE_TTL_SECONDS,
            },
        })

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
