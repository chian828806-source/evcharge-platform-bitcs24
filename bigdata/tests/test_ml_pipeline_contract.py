"""验证单一流水线中 ML 的 DWS 与质量报告输入边界。"""

import importlib.util
import json
import tempfile
import unittest
from pathlib import Path

from pyspark.sql import SparkSession


MODULE_PATH = Path(__file__).resolve().parents[1] / "ml" / "jobs" / "station_load_demo.py"
SPEC = importlib.util.spec_from_file_location("station_load_demo", MODULE_PATH)
station_load_demo = importlib.util.module_from_spec(SPEC)
assert SPEC and SPEC.loader
SPEC.loader.exec_module(station_load_demo)


class MlPipelineContractTest(unittest.TestCase):
    """使用极小 DataFrame 验证消费契约，不启动训练任务。"""

    @classmethod
    def setUpClass(cls):
        cls.spark = SparkSession.builder.master("local[1]").appName("ml-pipeline-contract-test").getOrCreate()
        cls.spark.sparkContext.setLogLevel("ERROR")

    @classmethod
    def tearDownClass(cls):
        cls.spark.stop()

    def valid_frame(self):
        return self.spark.createDataFrame([
            (101, "测试站点", "2026-09-15 10:00:00", 10, 2.0, 35.0, 180.0, 3.0, 7.0, 0.3, "BD-TEST-001", "BUSINESS", "2026-09-15")
        ], list(station_load_demo.DWS_REQUIRED_COLUMNS)).selectExpr(
            "station_id", "station_name", "to_timestamp(hour_start) AS hour_start", "total_pile_count",
            "session_starts", "energy_kwh", "charging_pile_minutes", "average_occupied_count",
            "average_available_count", "station_load", "batch_id", "source_type", "to_date(dt) AS dt",
        )

    def test_valid_contract(self):
        report = station_load_demo.validate_dws(self.valid_frame())
        self.assertEqual(report["acceptedRows"], 1)

    def test_missing_column_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "缺少字段"):
            station_load_demo.validate_dws(self.valid_frame().drop("station_load"))

    def test_duplicate_key_is_rejected(self):
        frame = self.valid_frame()
        with self.assertRaisesRegex(ValueError, "duplicateKeys=1"):
            station_load_demo.validate_dws(frame.unionByName(frame))

    def test_out_of_range_load_is_rejected(self):
        frame = self.valid_frame().withColumn("station_load", station_load_demo.F.lit(1.2))
        with self.assertRaisesRegex(ValueError, "invalidRows=1"):
            station_load_demo.validate_dws(frame)

    def test_quality_report_is_aggregated(self):
        payload = {
            "batch_id": "BD-TEST-001",
            "datasets": [
                {"source_rows": 10, "accepted_rows": 8, "rejected_rows": 2, "rules": {"DQ-001": 2}},
                {"source_rows": 5, "accepted_rows": 4, "rejected_rows": 1, "rules": {"DQ-001": 1, "DQ-003": 1}},
            ],
        }
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "quality"
            self.spark.createDataFrame([(json.dumps(payload),)], ["report_json"]).write.parquet(path.as_uri())
            report = station_load_demo.load_quality_report(
                self.spark, str(path), {"contract": "dws_station_hour-v1"}
            )
        self.assertEqual(report["sourceRows"], 15)
        self.assertEqual(report["acceptedRows"], 12)
        self.assertEqual(report["rejectedRows"], 3)
        self.assertEqual(report["rules"], [
            {"ruleId": "DQ-001", "count": 3},
            {"ruleId": "DQ-003", "count": 1},
        ])


if __name__ == "__main__":
    unittest.main()
