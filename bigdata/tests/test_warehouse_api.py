"""Contract tests for C's ADS-backed Flask service."""

import importlib.util
import json
import tempfile
import unittest
from pathlib import Path


MODULE_PATH = Path(__file__).resolve().parents[1] / "api" / "app.py"
SPEC = importlib.util.spec_from_file_location("warehouse_api", MODULE_PATH)
warehouse_api = importlib.util.module_from_spec(SPEC)
assert SPEC and SPEC.loader
SPEC.loader.exec_module(warehouse_api)


class WarehouseApiTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        root = Path(self.temporary.name)
        self.ads_path, self.ml_path = root / "ads.json", root / "ml.json"
        station_days = [
            {"stationId": 1, "stationName": "站点一", "district": "南山区", "date": "2026-09-15",
             "orderCount": 2, "energyKwh": 12.5, "revenueFen": 1500, "chargingPileMinutes": 120.0,
             "capacityPileMinutes": 480.0, "availableCount": 6.0, "totalPileCount": 8},
            {"stationId": 2, "stationName": "站点二", "district": "福田区", "date": "2026-09-15",
             "orderCount": 1, "energyKwh": 5.0, "revenueFen": 600, "chargingPileMinutes": 60.0,
             "capacityPileMinutes": 240.0, "availableCount": 3.0, "totalPileCount": 4},
        ]
        self.ads_path.write_text(json.dumps({
            "meta": {"batchId": "BD-C-TEST", "generatedAt": "2026-09-16T00:00:00Z",
                     "rangeFrom": "2026-09-15", "rangeTo": "2026-09-15"},
            "overview": {"orderCount": 3, "energyKwh": 17.5, "revenueFen": 2100,
                         "onlinePileCount": 12, "utilizationRate": 0.25},
            "energyTrend": {"items": []}, "revenueTrend": {"items": []},
            "stationRanking": {"items": []}, "pileStatus": {"items": []},
            "hourlyHeatmap": {"items": []}, "stationUtilization": {"items": []},
            "prediction": {"items": []},
            "dataQuality": {"sourceRows": 10, "acceptedRows": 9, "rejectedRows": 1, "rules": []},
            "_query": {"stationDays": station_days},
        }, ensure_ascii=False), encoding="utf-8")
        self.ml_path.write_text(json.dumps({
            "meta": {"batchId": "BD-C-TEST"},
            "prediction": {"items": [{"stationId": 1, "predictionTime": "2026-09-15 12:00:00"}]},
        }), encoding="utf-8")
        store = warehouse_api.SnapshotStore(self.ads_path, self.ml_path)
        self.client = warehouse_api.create_app(store, warehouse_api.WeatherService.unavailable).test_client()

    def test_all_frozen_resources(self):
        for resource in warehouse_api.CANONICAL_RESOURCES:
            with self.subTest(resource=resource):
                response = self.client.get(f"/api/v1/{resource}")
                self.assertEqual(response.status_code, 200)
                self.assertEqual(response.get_json()["meta"]["batchId"], "BD-C-TEST")

    def test_overview_and_trend_use_requested_station(self):
        overview = self.client.get("/api/v1/dashboard/overview?stationId=1").get_json()["data"]
        self.assertEqual(overview["orderCount"], 2)
        self.assertEqual(overview["revenueFen"], 1500)
        trend = self.client.get("/api/v1/dashboard/energy-trend?stationId=2").get_json()["data"]["items"]
        self.assertEqual(trend, [{"date": "2026-09-15", "energyKwh": 5.0, "orderCount": 1}])

    def test_prediction_is_merged_only_for_same_batch(self):
        items = self.client.get("/api/v1/dashboard/prediction").get_json()["data"]["items"]
        self.assertEqual(items[0]["stationId"], 1)

    def test_invalid_query_and_missing_batch_have_contract_errors(self):
        response = self.client.get("/api/v1/dashboard/overview?from=bad-date")
        self.assertEqual(response.status_code, 400)
        self.assertEqual(response.get_json()["error"]["code"], "INVALID_QUERY")
        self.ads_path.unlink()
        response = self.client.get("/api/v1/dashboard/overview")
        self.assertEqual(response.status_code, 409)
        self.assertEqual(response.get_json()["error"]["code"], "BATCH_NOT_READY")


if __name__ == "__main__":
    unittest.main()
