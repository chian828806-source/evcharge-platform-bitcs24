"""验证特征无泄漏、JSON 契约、预处理规则和训练预测最小闭环。"""

from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

import numpy as np
import pandas as pd

from ml.contracts import peak_level, validate_prediction_document
from ml.features import build_supervised, validate_history
from ml.predict import generate_predictions
from ml.prepare_cary_data import normalize_source_key, parse_duration
from ml.train import train_all


def synthetic_history(hours: int = 520) -> pd.DataFrame:
    """构造含日/周周期的三站点连续 UTC 历史，供快速且可重复的测试使用。"""

    timestamps = pd.date_range("2025-01-01", periods=hours, freq="h", tz="UTC")
    parts = []
    for station_id, capacity in ((1, 4), (2, 6), (3, 8)):
        hour = np.arange(hours)
        daily = 0.18 * (1 + np.sin(2 * np.pi * (hour - 7) / 24))
        weekly = 0.08 * (1 + np.sin(2 * np.pi * hour / 168 + station_id))
        load = np.clip(0.08 * station_id + daily + weekly, 0, 1)
        parts.append(
            pd.DataFrame(
                {
                    "timestamp": timestamps,
                    "station_id": station_id,
                    "total_pile_count": capacity,
                    "session_starts": np.maximum(0, np.rint(load * capacity)),
                    "energy_kwh": load * capacity * 7.2,
                    "station_load": load,
                }
            )
        )
    return pd.concat(parts, ignore_index=True)


class FeatureTests(unittest.TestCase):
    """覆盖小时连续性和只使用预测原点之前数据的约束。"""

    def test_rejects_hour_gap(self):
        history = synthetic_history(200).drop(index=50)
        with self.assertRaisesRegex(ValueError, "not continuous"):
            validate_history(history)

    def test_future_change_does_not_change_earlier_features(self):
        history = synthetic_history()
        before = build_supervised(history, 24, "UTC")
        changed = history.copy()
        changed.loc[(changed["station_id"] == 1) & (changed["timestamp"] >= pd.Timestamp("2025-01-18", tz="UTC")), "station_load"] = 1.0
        after = build_supervised(changed, 24, "UTC")
        key = (1, pd.Timestamp("2025-01-15", tz="UTC"))
        feature_columns = [column for column in before.columns if column not in {"target", "seasonal_prediction"}]
        row_before = before[(before["station_id"] == key[0]) & (before["origin_time"] == key[1])][feature_columns].reset_index(drop=True)
        row_after = after[(after["station_id"] == key[0]) & (after["origin_time"] == key[1])][feature_columns].reset_index(drop=True)
        pd.testing.assert_frame_equal(row_before, row_after)


class ContractTests(unittest.TestCase):
    """覆盖服务端导入所依赖的必填字段、范围和重复键规则。"""

    def test_valid_contract_and_peak_thresholds(self):
        document = {
            "schemaVersion": "1.0",
            "batchId": "test",
            "predictions": [{
                "stationId": 1,
                "predictionTime": "2026-09-01T13:00:00+08:00",
                "horizon": "1h",
                "predictedLoad": 0.65,
                "predictedAvailableCount": 3,
                "peakLevel": "MEDIUM",
                "modelName": "test-v1",
            }],
        }
        self.assertIs(validate_prediction_document(document), document)
        self.assertEqual([peak_level(value) for value in (0.1, 0.4, 0.7)], ["LOW", "MEDIUM", "HIGH"])

    def test_rejects_naive_time(self):
        document = {
            "schemaVersion": "1.0",
            "batchId": "test",
            "predictions": [{
                "stationId": 1,
                "predictionTime": "2026-09-01T13:00:00",
                "horizon": "1h",
                "predictedLoad": 0.5,
                "predictedAvailableCount": 1,
                "peakLevel": "MEDIUM",
                "modelName": "test-v1",
            }],
        }
        with self.assertRaisesRegex(ValueError, "timezone"):
            validate_prediction_document(document)


class PreparationTests(unittest.TestCase):
    """覆盖地址归一化和跨日充电时长解析。"""

    def test_source_key_and_duration(self):
        self.assertEqual(normalize_source_key(" 801  High House Road ", "27513"), "801 HIGH HOUSE ROAD|27513")
        self.assertEqual(parse_duration("25:01:02"), 90062)


class EndToEndTests(unittest.TestCase):
    """用小型数据完成训练、加载、预测和契约校验闭环。"""

    def test_train_and_predict(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            history_path = root / "history.csv"
            model_dir = root / "models"
            report_path = root / "report.json"
            output_path = root / "predictions.json"
            synthetic_history().to_csv(history_path, index=False)
            report = train_all(history_path, model_dir, report_path, "UTC")
            result = generate_predictions(history_path, model_dir, output_path, "test-batch", "Asia/Shanghai")
            self.assertEqual(set(report["horizons"]), {"1h", "6h", "24h"})
            self.assertEqual(len(result["predictions"]), 9)
            self.assertEqual(json.loads(output_path.read_text(encoding="utf-8")), result)


if __name__ == "__main__":
    unittest.main()
