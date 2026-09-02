"""验证 Cary 会话写入 SQLite 并重新导出为模型输入的完整路径。"""

from __future__ import annotations

import csv
import json
import sqlite3
import tempfile
import unittest
from pathlib import Path

from database.simulation.build_cary_database import build_database
from database.simulation.export_ml_history import export_history
from ml.features import load_history


ROOT = Path(__file__).resolve().parents[2]


class CaryDatabaseWorkflowTests(unittest.TestCase):
    """使用两条小会话检查建库、来源追踪和导出契约。"""

    def test_build_and_export(self):
        with tempfile.TemporaryDirectory() as directory:
            workspace = Path(directory)
            raw_path = workspace / "raw.csv"
            mapping_path = workspace / "mapping.json"
            database_path = workspace / "simulation.db"
            output_path = workspace / "history.csv"
            raw_path.write_text(
                "start_date;station_name;charging_time_hh_mm_ss;energy_kwh;address_1;address_2;city;state_province;zip_postal_code\n"
                "2023-01-01T00:30:00+00:00;TEST-A;01:00:00;4.0;Test Road;;Cary;NC;00000\n"
                "2023-01-01T02:00:00+00:00;TEST-A;00:30:00;2.0;Test Road;;Cary;NC;00000\n",
                encoding="utf-8",
            )
            mapping_path.write_text(
                json.dumps(
                    {
                        "schemaVersion": "1.0",
                        "calendarTimezone": "UTC",
                        "stations": [{
                            "sourceKey": "TEST ROAD|00000",
                            "stationId": 1,
                            "sourcePortCount": 2,
                            "projectPileCount": 4,
                            "label": "Test station",
                        }],
                    }
                ),
                encoding="utf-8",
            )

            result = build_database(
                raw_path,
                database_path,
                mapping_path,
                ROOT / "database/schema.sql",
                ROOT / "database/init_data.sql",
                "2019-01-01",
                "TEST-BATCH",
            )
            self.assertEqual(result["acceptedSessions"], 2)
            self.assertEqual(result["hourlyRows"], 3)

            connection = sqlite3.connect(database_path)
            try:
                table_count = connection.execute(
                    "SELECT COUNT(*) FROM sqlite_master "
                    "WHERE type = 'table' AND name NOT LIKE 'sqlite_%'"
                ).fetchone()[0]
                index_count = connection.execute(
                    "SELECT COUNT(*) FROM sqlite_master "
                    "WHERE type = 'index' AND sql IS NOT NULL"
                ).fetchone()[0]
                self.assertEqual(table_count, 12)
                self.assertEqual(index_count, 21)
                self.assertEqual(connection.execute("SELECT COUNT(*) FROM data_import_batch").fetchone()[0], 1)
                self.assertEqual(connection.execute("SELECT COUNT(*) FROM charging_session_history").fetchone()[0], 2)
                self.assertEqual(connection.execute("SELECT COUNT(*) FROM station_hourly_metric").fetchone()[0], 3)
                self.assertEqual(connection.execute("PRAGMA foreign_key_check").fetchall(), [])
                self.assertEqual(connection.execute("PRAGMA quick_check").fetchone()[0], "ok")
            finally:
                connection.close()

            self.assertEqual(export_history(database_path, "TEST-BATCH", output_path), 3)
            with output_path.open(encoding="utf-8", newline="") as stream:
                self.assertEqual(len(list(csv.DictReader(stream))), 3)

    def test_full_export_is_accepted_by_ml_reader(self):
        """模型读取器应接受真实工作流导出的连续小时字段。"""

        existing = ROOT / "ml/data/processed/station_hourly_load.csv"
        if not existing.exists():
            self.skipTest("tracked full dataset is not present")
        history = load_history(str(existing))
        self.assertEqual(sorted(history["station_id"].unique().tolist()), [1, 2, 3])


if __name__ == "__main__":
    unittest.main()
