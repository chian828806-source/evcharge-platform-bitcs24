#!/usr/bin/env python3
"""Read C's published ADS snapshot and expose the frozen Dashboard REST API."""

from __future__ import annotations

import json
import os
import threading
import time
from copy import deepcopy
from datetime import date, datetime, timedelta, timezone
from pathlib import Path
from typing import Any, Callable
from urllib.parse import urlencode
from urllib.request import urlopen

from flask import Flask, jsonify, request
from flask_cors import CORS


PROJECT_ROOT = Path(__file__).resolve().parents[2]
ADS_SNAPSHOT_PATH = Path(os.getenv("EVCHARGE_ADS_SNAPSHOT", PROJECT_ROOT / "bigdata/runtime/warehouse/dashboard.json"))
ML_SNAPSHOT_PATH = Path(os.getenv("EVCHARGE_ML_SNAPSHOT", PROJECT_ROOT / "bigdata/runtime/demo/dashboard.json"))
WEATHER_CACHE_TTL_SECONDS = int(os.getenv("EVCHARGE_WEATHER_CACHE_TTL_SECONDS", "1200"))
WEATHER_TIMEOUT_SECONDS = float(os.getenv("EVCHARGE_WEATHER_TIMEOUT_SECONDS", "4"))

RESOURCE_NAMES = {
    "overview": "overview", "energy-trend": "energyTrend", "revenue-trend": "revenueTrend",
    "station-ranking": "stationRanking", "pile-status": "pileStatus", "hourly-heatmap": "hourlyHeatmap",
    "station-utilization": "stationUtilization", "prediction": "prediction",
    "data-quality/summary": "dataQuality",
}
CANONICAL_RESOURCES = tuple(f"dashboard/{name}" if name != "data-quality/summary" else name for name in RESOURCE_NAMES)
WMO_WEATHER_TEXT = {0: "晴", 1: "晴间多云", 2: "多云", 3: "阴", 45: "雾", 48: "雾", 51: "毛毛雨",
                    53: "毛毛雨", 55: "毛毛雨", 61: "小雨", 63: "中雨", 65: "大雨", 71: "小雪",
                    73: "中雪", 75: "大雪", 80: "阵雨", 81: "阵雨", 82: "强阵雨", 95: "雷暴",
                    96: "雷暴伴冰雹", 99: "强雷暴伴冰雹"}


def load_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


class SnapshotStore:
    """Reload snapshots per request so a successful batch is visible without restarting Flask."""

    def __init__(self, ads_path: Path = ADS_SNAPSHOT_PATH, ml_path: Path = ML_SNAPSHOT_PATH):
        self.ads_path, self.ml_path = ads_path, ml_path

    def read(self) -> dict[str, Any]:
        snapshot = load_json(self.ads_path)
        if not isinstance(snapshot.get("meta"), dict):
            raise ValueError("ADS snapshot does not contain metadata")
        if self.ml_path.is_file():
            ml = load_json(self.ml_path)
            if ml.get("meta", {}).get("batchId") == snapshot["meta"].get("batchId"):
                snapshot["prediction"] = ml.get("prediction", {"items": []})
        return snapshot


def fetch_json(url: str, timeout: float) -> dict[str, Any]:
    with urlopen(url, timeout=timeout) as response:  # noqa: S310 - host is fixed below.
        return json.loads(response.read().decode("utf-8"))


class WeatherService:
    def __init__(self, fetcher: Callable[[str, float], dict[str, Any]] = fetch_json,
                 clock: Callable[[], float] = time.monotonic):
        self.fetcher, self.clock = fetcher, clock
        self.cache: dict[str, Any] | None = None
        self.cached_at = 0.0
        self.lock = threading.Lock()

    @staticmethod
    def unavailable() -> dict[str, Any]:
        return {"city": "深圳", "temperature": None, "apparentTemperature": None, "humidity": None,
                "precipitation": None, "windSpeed": None, "weatherCode": None, "weatherText": "暂不可用",
                "updatedAt": None, "available": False, "isStale": True, "source": "unavailable"}

    def current(self) -> dict[str, Any]:
        now = self.clock()
        with self.lock:
            if self.cache is not None and now - self.cached_at < WEATHER_CACHE_TTL_SECONDS:
                return deepcopy(self.cache)
        query = urlencode({"latitude": 22.5431, "longitude": 114.0579,
                           "current": "temperature_2m,apparent_temperature,relative_humidity_2m,precipitation,weather_code,wind_speed_10m",
                           "timezone": "Asia/Shanghai"})
        try:
            current = self.fetcher(f"https://api.open-meteo.com/v1/forecast?{query}", WEATHER_TIMEOUT_SECONDS)["current"]
            code = int(current["weather_code"])
            updated = str(current["time"])
            if len(updated) == 16:
                updated += ":00+08:00"
            fresh = {"city": "深圳", "temperature": float(current["temperature_2m"]),
                     "apparentTemperature": float(current["apparent_temperature"]),
                     "humidity": int(current["relative_humidity_2m"]), "precipitation": float(current["precipitation"]),
                     "windSpeed": float(current["wind_speed_10m"]), "weatherCode": code,
                     "weatherText": WMO_WEATHER_TEXT.get(code, "未知天气"), "updatedAt": updated,
                     "available": True, "isStale": False, "source": "open-meteo"}
        except (OSError, KeyError, TypeError, ValueError):
            with self.lock:
                stale = deepcopy(self.cache)
            if stale is None:
                return self.unavailable()
            stale.update({"isStale": True, "source": "cache"})
            return stale
        with self.lock:
            self.cache, self.cached_at = deepcopy(fresh), now
        return fresh


def parse_date(name: str, value: str | None) -> date | None:
    if not value:
        return None
    try:
        return date.fromisoformat(value)
    except ValueError as error:
        raise ValueError(f"{name} must use YYYY-MM-DD") from error


def query_window(snapshot: dict[str, Any]) -> tuple[date, date, int | None]:
    meta = snapshot.get("meta", {})
    range_to = parse_date("meta.rangeTo", meta.get("rangeTo")) or datetime.now().date()
    default_from = range_to - timedelta(days=29)
    start = parse_date("from", request.args.get("from")) or default_from
    end = parse_date("to", request.args.get("to")) or range_to
    if start > end:
        raise ValueError("from must not be later than to")
    station_raw = request.args.get("stationId")
    if station_raw is None:
        station_id = None
    else:
        try:
            station_id = int(station_raw)
        except ValueError as error:
            raise ValueError("stationId must be an integer") from error
        if station_id <= 0:
            raise ValueError("stationId must be positive")
    return start, end, station_id


def selected_station_days(snapshot: dict[str, Any], start: date, end: date, station_id: int | None) -> list[dict[str, Any]]:
    rows = snapshot.get("_query", {}).get("stationDays", [])
    return [row for row in rows if start <= date.fromisoformat(row["date"]) <= end
            and (station_id is None or row["stationId"] == station_id)]


def aggregate_days(rows: list[dict[str, Any]]) -> dict[str, Any]:
    capacity = sum(float(row["capacityPileMinutes"]) for row in rows)
    charging = sum(float(row["chargingPileMinutes"]) for row in rows)
    return {"orderCount": sum(int(row["orderCount"]) for row in rows),
            "energyKwh": round(sum(float(row["energyKwh"]) for row in rows), 2),
            "revenueFen": sum(int(row["revenueFen"]) for row in rows),
            "utilizationRate": round(charging / capacity, 4) if capacity else 0.0}


def filtered_resource(snapshot: dict[str, Any], resource: str) -> Any:
    if resource in {"pile-status", "hourly-heatmap", "prediction", "data-quality/summary"}:
        data = deepcopy(snapshot[RESOURCE_NAMES[resource]])
        if resource == "prediction":
            start, end, station_id = query_window(snapshot)
            filter_dates = request.args.get("from") is not None or request.args.get("to") is not None
            data["items"] = [row for row in data.get("items", [])
                             if (not filter_dates or start <= date.fromisoformat(str(row["predictionTime"])[:10]) <= end)
                             and (station_id is None or row["stationId"] == station_id)]
        return data
    start, end, station_id = query_window(snapshot)
    days = selected_station_days(snapshot, start, end, station_id)
    if resource == "overview":
        result = aggregate_days(days)
        result["onlinePileCount"] = snapshot["overview"].get("onlinePileCount", 0)
        return result
    if resource in {"energy-trend", "revenue-trend"}:
        grouped: dict[str, list[dict[str, Any]]] = {}
        for row in days:
            grouped.setdefault(row["date"], []).append(row)
        items = []
        for day in sorted(grouped):
            totals = aggregate_days(grouped[day])
            item = {"date": day, "orderCount": totals["orderCount"]}
            item["energyKwh" if resource == "energy-trend" else "revenueFen"] = totals[
                "energyKwh" if resource == "energy-trend" else "revenueFen"]
            items.append(item)
        return {"items": items}
    if resource == "station-ranking":
        grouped: dict[int, list[dict[str, Any]]] = {}
        for row in days:
            grouped.setdefault(row["stationId"], []).append(row)
        ranking = []
        for station_rows in grouped.values():
            totals = aggregate_days(station_rows)
            first = station_rows[0]
            ranking.append({"stationId": first["stationId"], "stationName": first["stationName"],
                            "district": first.get("district"), "energyKwh": totals["energyKwh"],
                            "revenueFen": totals["revenueFen"], "utilizationRate": totals["utilizationRate"]})
        ranking.sort(key=lambda row: (-row["energyKwh"], row["stationId"]))
        for index, row in enumerate(ranking, 1):
            row["rank"] = index
        return {"items": ranking}
    return {"items": [{"stationId": row["stationId"], "stationName": row["stationName"], "date": row["date"],
                        "utilizationRate": round(row["chargingPileMinutes"] / row["capacityPileMinutes"], 4)
                        if row["capacityPileMinutes"] else 0.0, "availableCount": int(round(row["availableCount"])),
                        "totalPileCount": row["totalPileCount"]} for row in days]}


def create_app(store: SnapshotStore | None = None, weather_provider: Callable[[], dict[str, Any]] | None = None) -> Flask:
    app = Flask(__name__)
    CORS(app, resources={r"/api/*": {"origins": "*"}})
    snapshots = store or SnapshotStore()
    provider = weather_provider or WeatherService().current

    @app.get("/health")
    def health():
        return jsonify({"status": "ok", "adsSnapshot": str(snapshots.ads_path),
                        "snapshotExists": snapshots.ads_path.is_file()})

    @app.get("/api/v1/context/weather")
    def weather():
        data = provider()
        return jsonify({"data": data, "meta": {"generatedAt": datetime.now(timezone.utc).isoformat(),
                                                "source": data["source"],
                                                "cacheTtlSeconds": WEATHER_CACHE_TTL_SECONDS}})

    @app.get("/api/v1/<path:resource>")
    def resource(resource: str):
        normalized = resource.removeprefix("dashboard/")
        if normalized not in RESOURCE_NAMES:
            return jsonify({"error": {"code": "NOT_FOUND", "message": "Unknown dashboard resource."}}), 404
        try:
            snapshot = snapshots.read()
            data = filtered_resource(snapshot, normalized)
        except ValueError as error:
            return jsonify({"error": {"code": "INVALID_QUERY", "message": str(error)}}), 400
        except (OSError, json.JSONDecodeError, KeyError) as error:
            return jsonify({"error": {"code": "BATCH_NOT_READY", "message": str(error)}}), 409
        meta = snapshot["meta"]
        start, end, _ = query_window(snapshot) if normalized not in {"pile-status", "hourly-heatmap", "data-quality/summary"} else (None, None, None)
        response_meta = {"batchId": meta["batchId"], "generatedAt": meta["generatedAt"]}
        if start and end and (normalized != "prediction" or request.args.get("from") or request.args.get("to")):
            response_meta.update({"from": str(start), "to": str(end)})
        return jsonify({"data": data, "meta": response_meta})

    return app


app = create_app()

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=int(os.getenv("PORT", "5000")), debug=False)
