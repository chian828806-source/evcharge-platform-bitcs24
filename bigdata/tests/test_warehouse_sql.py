"""Small Spark SQL contract test for C's DWS and ADS builders."""

import importlib.util
import unittest
from datetime import datetime
from pathlib import Path

from pyspark.sql import SparkSession


MODULE_PATH = Path(__file__).resolve().parents[1] / "spark" / "warehouse" / "build_warehouse.py"
SPEC = importlib.util.spec_from_file_location("build_warehouse", MODULE_PATH)
build_warehouse = importlib.util.module_from_spec(SPEC)
assert SPEC and SPEC.loader
SPEC.loader.exec_module(build_warehouse)


class WarehouseSqlTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.spark = SparkSession.builder.master("local[1]").appName("warehouse-sql-test").getOrCreate()
        cls.spark.sparkContext.setLogLevel("ERROR")
        cls.spark.conf.set("spark.sql.session.timeZone", "Asia/Shanghai")

    @classmethod
    def tearDownClass(cls):
        cls.spark.stop()

    def inputs(self):
        batch = "BD-C-TEST"
        paid = datetime(2026, 9, 15, 10, 20)
        return {
            "dwd_order_detail": self.spark.createDataFrame([
                (1, "ORDER-1", 101, 1001, "站点一", "南山区", "COMPLETED", 30, 12.5, 1500, paid, batch),
            ], "order_id long, order_no string, station_id long, pile_id long, station_name string, district string, status string, charge_minutes int, energy_kwh double, amount_fen long, paid_at timestamp, batch_id string"),
            "dwd_charging_session_detail": self.spark.createDataFrame([
                ("S-1", 101, paid, 12.5, batch),
            ], "source_session_key string, station_id long, start_at timestamp, energy_kwh double, batch_id string"),
            "dwd_pile_detail": self.spark.createDataFrame([
                (1001, 101, "站点一", "南山区", "AVAILABLE", paid, batch),
            ], "pile_id long, station_id long, station_name string, district string, status string, updated_at timestamp, batch_id string"),
            "dwd_station_hour_metric": self.spark.createDataFrame([
                (101, "站点一", "南山区", datetime(2026, 9, 15, 10), 4, 1, 12.5, 60.0, 1.0, 3.0, 0.25, "BUSINESS", batch),
            ], "station_id long, station_name string, district string, hour_start timestamp, total_pile_count int, session_starts int, energy_kwh double, charging_pile_minutes double, average_occupied_count double, average_available_count double, station_load double, source_type string, batch_id string"),
        }

    def test_builds_contract_metrics_without_estimated_revenue(self):
        inputs = self.inputs()
        build_warehouse.validate_inputs(inputs, "BD-C-TEST")
        dws = build_warehouse.build_dws(self.spark, inputs, "BD-C-TEST")
        hour = dws["dws_station_hour"].first()
        self.assertEqual(hour.completed_order_count, 1)
        self.assertEqual(hour.revenue_fen, 1500)
        self.assertAlmostEqual(hour.utilization_rate, 0.25)
        ads = build_warehouse.build_ads(self.spark, dws, inputs)
        overview = ads["ads_dashboard_overview"].first()
        self.assertEqual(overview.order_count, 1)
        self.assertEqual(overview.revenue_fen, 1500)


if __name__ == "__main__":
    unittest.main()
