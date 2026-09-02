"""加载已训练模型，为每个站点生成可由 Qt/C++ 服务端导入的预测 JSON。"""

from __future__ import annotations

import argparse
import json
import os
from datetime import datetime, timezone
from pathlib import Path
from zoneinfo import ZoneInfo

import joblib
import numpy as np

from .contracts import peak_level, validate_prediction_document
from .features import HORIZONS, build_inference, load_history


def _atomic_json(document: dict, path: Path) -> None:
    """原子写入最终批次，确保服务端不会读到未完成的 JSON。"""

    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(document, ensure_ascii=False, indent=2), encoding="utf-8")
    os.replace(temporary, path)


def _load_artifacts(model_dir: Path) -> dict[int, dict]:
    """读取三个时间跨度的模型，并核对它们是否来自同一套站点契约。"""

    artifacts: dict[int, dict] = {}
    identity = None
    for horizon in HORIZONS:
        path = model_dir / f"load_{horizon}h.joblib"
        artifact = joblib.load(path)
        if artifact.get("schemaVersion") != "1.0" or artifact.get("horizonHours") != horizon:
            raise ValueError(f"invalid model artifact: {path}")
        current = (
            tuple(artifact.get("stationIds", [])),
            artifact.get("calendarTimezone"),
            artifact.get("sourceSha256"),
        )
        if identity is None:
            identity = current
        elif current != identity:
            raise ValueError("model artifacts were not produced by one training run")
        artifacts[horizon] = artifact
    return artifacts


def generate_predictions(
    history_path: Path,
    model_dir: Path,
    output_path: Path,
    batch_id: str,
    output_timezone: str,
) -> dict:
    """生成并校验整个预测批次；任一记录错误时不会产生正式输出。"""

    if not batch_id.strip():
        raise ValueError("batch_id must not be empty")
    output_zone = ZoneInfo(output_timezone)
    history = load_history(str(history_path))
    artifacts = _load_artifacts(model_dir)
    generated_at = datetime.now(timezone.utc).astimezone(output_zone).isoformat()
    predictions: list[dict] = []

    for horizon, artifact in artifacts.items():
        station_ids = [int(value) for value in artifact["stationIds"]]
        frame = build_inference(
            history,
            horizon,
            artifact["calendarTimezone"],
            station_ids,
        )
        if artifact["kind"] == "hist-gradient-boosting":
            loads = artifact["estimator"].predict(frame[artifact["featureNames"]])
        elif artifact["kind"] == "seasonal-lag-168":
            loads = frame["seasonal_prediction"].to_numpy()
        else:
            raise ValueError(f"unsupported model kind: {artifact['kind']}")

        metrics = artifact["metrics"]["overall"]
        for (_, row), raw_load in zip(frame.iterrows(), loads):
            predicted_load = float(np.clip(raw_load, 0, 1))
            capacity = int(row["capacity"])
            available = max(0, min(capacity, round(capacity * (1 - predicted_load))))
            prediction_time = row["target_time"].to_pydatetime().astimezone(output_zone).isoformat()
            predictions.append(
                {
                    "stationId": int(row["station_id"]),
                    "predictionTime": prediction_time,
                    "horizon": f"{horizon}h",
                    "predictedLoad": round(predicted_load, 6),
                    "predictedAvailableCount": available,
                    "peakLevel": peak_level(predicted_load),
                    "modelName": artifact["modelName"],
                    "mae": round(float(metrics["mae"]), 6),
                    "rmse": round(float(metrics["rmse"]), 6),
                    "generatedAt": generated_at,
                }
            )

    predictions.sort(key=lambda item: (item["stationId"], item["predictionTime"], item["horizon"]))
    document = {
        "schemaVersion": "1.0",
        "batchId": batch_id,
        "predictions": predictions,
    }
    validate_prediction_document(document)
    _atomic_json(document, output_path)
    return document


def main() -> None:
    """解析命令行参数并执行批量预测。"""

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--history", type=Path, required=True)
    parser.add_argument("--model-dir", type=Path, default=Path("ml/models"))
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--batch-id", required=True)
    parser.add_argument("--output-timezone", default="Asia/Shanghai")
    args = parser.parse_args()
    document = generate_predictions(
        args.history,
        args.model_dir,
        args.output,
        args.batch_id,
        args.output_timezone,
    )
    print(f"wrote {len(document['predictions'])} predictions to {args.output}")


if __name__ == "__main__":
    main()
