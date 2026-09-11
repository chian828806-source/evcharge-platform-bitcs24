"""第一阶段六模块测试清单与代码证据的一致性检查。"""

import json
import sqlite3
import tempfile
import unittest
from pathlib import Path


TEST_DIR = Path(__file__).resolve().parent
REPO_ROOT = TEST_DIR.parents[1]
MATRIX_PATH = TEST_DIR / "case-matrix.json"


class FirstStageContractTests(unittest.TestCase):
    """保证测试表中的 48 个编号均能追溯到可运行测试或代码契约。"""

    @classmethod
    def setUpClass(cls):
        cls.matrix = json.loads(MATRIX_PATH.read_text(encoding="utf-8"))

    def test_six_modules_and_48_unique_cases(self):
        modules = self.matrix["modules"]
        self.assertEqual(6, len(modules))
        self.assertTrue(all(len(cases) == 8 for cases in modules.values()))
        case_ids = [case["id"] for cases in modules.values() for case in cases]
        self.assertEqual(48, len(case_ids))
        self.assertEqual(48, len(set(case_ids)))

    def test_every_case_has_existing_evidence(self):
        for module, cases in self.matrix["modules"].items():
            for case in cases:
                with self.subTest(module=module, case=case["id"]):
                    source = REPO_ROOT / case["file"]
                    self.assertTrue(source.is_file(), f"missing evidence file: {source}")
                    self.assertIn(case["token"], source.read_text(encoding="utf-8"))
                    self.assertIn(case["mode"], {"automated", "source-contract"})

    def test_database_schema_and_seed_initialize(self):
        """用临时数据库验证第一阶段服务端依赖的 Schema 与种子数据。"""
        schema = (REPO_ROOT / "database/schema.sql").read_text(encoding="utf-8")
        raw_seed = (REPO_ROOT / "database/init_data.sql").read_text(encoding="utf-8")
        # sqlite3 的 Python API 不识别 CLI 的 .command；产品初始化器同样会跳过这些行。
        seed = "\n".join(
            line for line in raw_seed.splitlines() if not line.lstrip().startswith(".")
        )
        with tempfile.TemporaryDirectory() as temp_dir:
            db_path = Path(temp_dir) / "first-stage.db"
            connection = sqlite3.connect(db_path)
            try:
                connection.executescript(schema)
                connection.executescript(seed)
                table_names = {
                    row[0]
                    for row in connection.execute(
                        "SELECT name FROM sqlite_master WHERE type='table'"
                    )
                }
                self.assertTrue(
                    {"user", "charging_station", "charging_pile", "charging_order"}
                    .issubset(table_names)
                )
                self.assertGreater(
                    connection.execute("SELECT COUNT(*) FROM charging_station").fetchone()[0],
                    0,
                )
                self.assertGreater(
                    connection.execute("SELECT COUNT(*) FROM charging_pile").fetchone()[0],
                    0,
                )
            finally:
                connection.close()


if __name__ == "__main__":
    unittest.main(verbosity=2)
