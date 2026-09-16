"""验证 Dashboard Demo API 的资源覆盖和统一响应信封。"""

import importlib.util
import json
import tempfile
import unittest
from pathlib import Path


MODULE_PATH = Path(__file__).resolve().parents[1] / "integration" / "demo_api.py"
SPEC = importlib.util.spec_from_file_location("demo_api", MODULE_PATH)
demo_api = importlib.util.module_from_spec(SPEC)
assert SPEC and SPEC.loader
SPEC.loader.exec_module(demo_api)


class DashboardApiTest(unittest.TestCase):
    """使用 Flask 测试客户端，不启动端口即可验证九个接口。"""

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.original_snapshot = demo_api.SNAPSHOT_PATH
        self.addCleanup(setattr, demo_api, "SNAPSHOT_PATH", self.original_snapshot)
        demo_api.SNAPSHOT_PATH = Path(self.temporary.name) / "dashboard.json"
        demo_api.SNAPSHOT_PATH.write_text(json.dumps({
            "meta": {"batchId": "URBANEV-TEST", "generatedAt": "2026-09-16T00:00:00Z"},
            "overview": {"orderCount": 0, "energyKwh": 0, "revenueFen": 0,
                         "onlinePileCount": 0, "utilizationRate": 0},
            "energyTrend": {"items": []},
            "revenueTrend": {"items": []},
            "stationRanking": {"items": []},
            "pileStatus": {"items": []},
            "hourlyHeatmap": {"items": []},
            "stationUtilization": {"items": []},
            "prediction": {"items": []},
            "dataQuality": {"sourceRows": 0, "acceptedRows": 0, "rejectedRows": 0, "rules": []},
        }), encoding="utf-8")
        self.weather = {
            "city": "深圳", "temperature": 28.4, "apparentTemperature": 31.2,
            "humidity": 76, "precipitation": 0.0, "windSpeed": 12.3,
            "weatherCode": 2, "weatherText": "多云",
            "updatedAt": "2026-09-16T18:30:00+08:00", "available": True,
            "isStale": False, "source": "open-meteo",
        }
        self.client = demo_api.create_app(lambda: self.weather).test_client()

    def test_health(self):
        self.assertEqual(self.client.get("/health").status_code, 200)

    def test_all_contract_resources(self):
        for resource in demo_api.CANONICAL_RESOURCES:
            with self.subTest(resource=resource):
                response = self.client.get(f"/api/v1/{resource}")
                self.assertEqual(response.status_code, 200)
                body = response.get_json()
                self.assertIn("data", body)
                self.assertIn("batchId", body["meta"])
                self.assertIn("generatedAt", body["meta"])

    def test_missing_snapshot_is_not_replaced_with_fake_data(self):
        demo_api.SNAPSHOT_PATH.unlink()
        response = self.client.get("/api/v1/dashboard/overview")
        self.assertEqual(response.status_code, 409)
        self.assertEqual(response.get_json()["error"]["code"], "BATCH_NOT_READY")

    def test_weather_is_independent_from_batch_snapshot(self):
        demo_api.SNAPSHOT_PATH.unlink()
        response = self.client.get("/api/v1/context/weather")
        self.assertEqual(response.status_code, 200)
        self.assertEqual(response.get_json()["data"], self.weather)


class WeatherServiceTest(unittest.TestCase):
    """验证天气码映射、缓存复用与失败降级，不访问公网。"""

    def test_maps_open_meteo_and_reuses_cache(self):
        calls = []

        def fetcher(url, timeout):
            calls.append((url, timeout))
            return {"current": {
                "time": "2026-09-16T18:30", "temperature_2m": 28.4,
                "apparent_temperature": 31.2, "relative_humidity_2m": 76,
                "precipitation": 0, "weather_code": 2, "wind_speed_10m": 12.3,
            }}

        service = demo_api.WeatherService(fetcher=fetcher, clock=lambda: 100.0)
        self.assertEqual(service.current()["weatherText"], "多云")
        self.assertEqual(service.current()["temperature"], 28.4)
        self.assertEqual(len(calls), 1)

    def test_returns_transparent_unavailable_data_without_cache(self):
        def failing_fetcher(url, timeout):
            raise OSError("offline")

        weather = demo_api.WeatherService(fetcher=failing_fetcher).current()
        self.assertFalse(weather["available"])
        self.assertIsNone(weather["temperature"])
        self.assertEqual(weather["weatherText"], "暂不可用")


if __name__ == "__main__":
    unittest.main()
