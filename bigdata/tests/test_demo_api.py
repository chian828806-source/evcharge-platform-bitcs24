"""验证 Dashboard Demo API 的资源覆盖和统一响应信封。"""

import importlib.util
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
        self.client = demo_api.create_app().test_client()

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


if __name__ == "__main__":
    unittest.main()
