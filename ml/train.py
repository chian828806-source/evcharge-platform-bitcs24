"""按时间切分数据，训练 1h、6h、24h 负荷模型并生成评估报告。"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

import joblib
import numpy as np
from sklearn.ensemble import HistGradientBoostingRegressor
from sklearn.metrics import mean_absolute_error, mean_squared_error

from .features import HORIZONS, build_supervised, load_history, model_feature_names


def _sha256(path: Path) -> str:
    """计算训练输入摘要，便于模型结果追溯。"""

    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _split(frame):
    """每个站点独立按 70%/15%/15% 划分，避免随机切分造成未来泄漏。"""

    labels = np.empty(len(frame), dtype=object)
    for _, indices in frame.groupby("station_id", sort=True).groups.items():
        ordered = list(indices)
        if len(ordered) < 30:
            raise ValueError("each station requires at least 30 supervised rows")
        train_end = max(1, int(len(ordered) * 0.70))
        valid_end = max(train_end + 1, int(len(ordered) * 0.85))
        labels[ordered[:train_end]] = "train"
        labels[ordered[train_end:valid_end]] = "valid"
        labels[ordered[valid_end:]] = "test"
    return labels


def _metrics(frame, actual, predicted) -> dict[str, Any]:
    """计算整体和逐站点 MAE/RMSE，使总分不会掩盖单站异常。"""

    def calculate(y_true, y_pred):
        return {
            "mae": float(mean_absolute_error(y_true, y_pred)),
            "rmse": float(np.sqrt(mean_squared_error(y_true, y_pred))),
        }

    result: dict[str, Any] = {"overall": calculate(actual, predicted), "byStation": {}}
    for station_id in sorted(frame["station_id"].unique()):
        mask = frame["station_id"].to_numpy() == station_id
        result["byStation"][str(int(station_id))] = calculate(actual[mask], predicted[mask])
    return result


def _attach_baseline(metrics, baseline_metrics):
    """在模型指标旁记录同周基线和相对提升，便于判断模型是否值得采用。"""

    for key in ["overall", *metrics["byStation"]]:
        current = metrics["overall"] if key == "overall" else metrics["byStation"][key]
        baseline = baseline_metrics["overall"] if key == "overall" else baseline_metrics["byStation"][key]
        current["seasonalMae"] = baseline["mae"]
        current["seasonalRmse"] = baseline["rmse"]
        current["maeSkill"] = (
            0.0 if baseline["mae"] == 0 else 1.0 - current["mae"] / baseline["mae"]
        )
    return metrics


def _atomic_joblib(document: Any, path: Path) -> None:
    """先写临时文件再替换，防止中断留下半个模型。"""

    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    joblib.dump(document, temporary)
    os.replace(temporary, path)


def _atomic_json(document: Any, path: Path) -> None:
    """以 UTF-8 原子写入训练报告。"""

    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(document, ensure_ascii=False, indent=2), encoding="utf-8")
    os.replace(temporary, path)


def train_all(input_path: Path, model_dir: Path, report_path: Path, calendar_timezone: str) -> dict[str, Any]:
    """训练全部约定时间跨度，并用验证集决定采用学习模型还是季节基线。"""

    history = load_history(str(input_path))
    station_ids = sorted(int(value) for value in history["station_id"].unique())
    feature_names = model_feature_names(station_ids)
    trained_at = datetime.now(timezone.utc).isoformat()
    source_sha256 = _sha256(input_path)
    report: dict[str, Any] = {
        "schemaVersion": "1.0",
        "trainedAt": trained_at,
        "source": str(input_path),
        "sourceSha256": source_sha256,
        "calendarTimezone": calendar_timezone,
        "stationIds": station_ids,
        "horizons": {},
    }

    for horizon in HORIZONS:
        frame = build_supervised(history, horizon, calendar_timezone, station_ids)
        labels = _split(frame)
        train_mask, valid_mask, test_mask = labels == "train", labels == "valid", labels == "test"
        estimator = HistGradientBoostingRegressor(
            learning_rate=0.06,
            max_iter=220,
            max_leaf_nodes=31,
            l2_regularization=0.1,
            random_state=24,
        )
        estimator.fit(frame.loc[train_mask, feature_names], frame.loc[train_mask, "target"])
        validation_prediction = np.clip(estimator.predict(frame.loc[valid_mask, feature_names]), 0, 1)
        validation_actual = frame.loc[valid_mask, "target"].to_numpy()
        validation_baseline = frame.loc[valid_mask, "seasonal_prediction"].to_numpy()
        learned_validation = _metrics(frame.loc[valid_mask], validation_actual, validation_prediction)
        baseline_validation = _metrics(frame.loc[valid_mask], validation_actual, validation_baseline)

        # 至少改善 1% 才采用学习模型，避免仅由浮点噪声造成模型切换。
        use_learned = learned_validation["overall"]["mae"] < baseline_validation["overall"]["mae"] * 0.99
        if use_learned:
            fit_mask = train_mask | valid_mask
            estimator.fit(frame.loc[fit_mask, feature_names], frame.loc[fit_mask, "target"])
            test_prediction = np.clip(estimator.predict(frame.loc[test_mask, feature_names]), 0, 1)
            model_kind = "hist-gradient-boosting"
            model_name = f"hgb-{horizon}h-v1"
            stored_estimator = estimator
        else:
            test_prediction = frame.loc[test_mask, "seasonal_prediction"].to_numpy()
            model_kind = "seasonal-lag-168"
            model_name = f"seasonal-{horizon}h-v1"
            stored_estimator = None

        test_actual = frame.loc[test_mask, "target"].to_numpy()
        test_baseline = frame.loc[test_mask, "seasonal_prediction"].to_numpy()
        test_metrics = _attach_baseline(
            _metrics(frame.loc[test_mask], test_actual, test_prediction),
            _metrics(frame.loc[test_mask], test_actual, test_baseline),
        )
        validation_metrics = _attach_baseline(learned_validation, baseline_validation)
        horizon_report = {
            "horizonHours": horizon,
            "selectedModel": model_kind,
            "modelName": model_name,
            "rowCounts": {
                "train": int(train_mask.sum()),
                "validation": int(valid_mask.sum()),
                "test": int(test_mask.sum()),
            },
            "learnedModelValidation": validation_metrics,
            "selectedModelTest": test_metrics,
        }
        artifact = {
            "schemaVersion": "1.0",
            "kind": model_kind,
            "modelName": model_name,
            "horizonHours": horizon,
            "featureNames": feature_names,
            "stationIds": station_ids,
            "calendarTimezone": calendar_timezone,
            "trainedAt": trained_at,
            "sourceSha256": source_sha256,
            "metrics": test_metrics,
            "estimator": stored_estimator,
        }
        _atomic_joblib(artifact, model_dir / f"load_{horizon}h.joblib")
        report["horizons"][f"{horizon}h"] = horizon_report

    _atomic_json(report, report_path)
    return report


def main() -> None:
    """解析命令行参数并执行训练。"""

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--model-dir", type=Path, default=Path("ml/models"))
    parser.add_argument("--report", type=Path, default=Path("ml/reports/training_report.json"))
    parser.add_argument("--calendar-timezone", default="America/New_York")
    args = parser.parse_args()
    report = train_all(args.input, args.model_dir, args.report, args.calendar_timezone)
    for name, result in report["horizons"].items():
        test = result["selectedModelTest"]["overall"]
        print(f"{name}: {result['selectedModel']}, MAE={test['mae']:.4f}, skill={test['maeSkill']:.1%}")


if __name__ == "__main__":
    main()
