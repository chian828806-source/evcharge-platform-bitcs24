#!/usr/bin/env python3
"""Upload one generated Raw batch to immutable HDFS ODS directories."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
import tempfile
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


DATASETS = ["users", "stations", "piles", "orders", "sessions", "station_hourly_metrics"]


def run_hdfs(*arguments: str, capture: bool = False) -> subprocess.CompletedProcess[str]:
    command = ["hdfs", "dfs", *arguments]
    return subprocess.run(command, check=True, text=True, capture_output=capture)


def exists(hdfs_path: str) -> bool:
    result = subprocess.run(["hdfs", "dfs", "-test", "-e", hdfs_path], check=False)
    return result.returncode == 0


def load_json(path: Path) -> dict[str, Any]:
    with path.open(encoding="utf-8") as handle:
        return json.load(handle)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--raw-dir", type=Path, required=True, help="Directory containing six CSVs and manifest.json")
    parser.add_argument("--business-date", required=True, help="ODS partition date, YYYY-MM-DD")
    parser.add_argument("--batch-id", required=True)
    parser.add_argument("--hdfs-root", default="/evcharge")
    parser.add_argument("--replace", action="store_true", help="Replace only this exact batch path when it already exists")
    return parser.parse_args()


def prepare_target(path: str, replace: bool) -> None:
    if exists(path):
        if not replace:
            raise FileExistsError(f"HDFS target already exists: {path}. Use --replace only to replace this batch.")
        run_hdfs("-rm", "-r", "-f", path)
    run_hdfs("-mkdir", "-p", path)


def main() -> None:
    args = parse_args()
    datetime.strptime(args.business_date, "%Y-%m-%d")
    manifest_path = args.raw_dir / "manifest.json"
    if not manifest_path.is_file():
        raise FileNotFoundError(f"Missing generator manifest: {manifest_path}")
    generator_manifest = load_json(manifest_path)
    if generator_manifest.get("batch_id") != args.batch_id:
        raise ValueError("--batch-id must match the generator manifest batch_id")

    hdfs_root = args.hdfs_root.rstrip("/")
    ingested_at = datetime.now(timezone.utc).isoformat(timespec="seconds")
    datasets: dict[str, Any] = {}
    for dataset in DATASETS:
        source = args.raw_dir / f"{dataset}.csv"
        if not source.is_file():
            raise FileNotFoundError(f"Missing Raw CSV: {source}")
        target = f"{hdfs_root}/ods/{dataset}/dt={args.business_date}/batch={args.batch_id}"
        prepare_target(target, args.replace)
        run_hdfs("-put", str(source), target)
        hdfs_file = f"{target}/{source.name}"
        if not exists(hdfs_file):
            raise RuntimeError(f"HDFS verification failed: {hdfs_file}")
        datasets[dataset] = {
            **generator_manifest["datasets"][dataset],
            "hdfs_path": hdfs_file,
        }

    ingestion_manifest = {
        "batch_id": args.batch_id,
        "business_date": args.business_date,
        "ingested_at": ingested_at,
        "hdfs_root": hdfs_root,
        "generator": {
            "seed": generator_manifest["seed"],
            "generated_at": generator_manifest["generated_at"],
            "injection_rate": generator_manifest["injection_rate"],
        },
        "datasets": datasets,
    }
    manifest_target = f"{hdfs_root}/ods/_manifests/dt={args.business_date}/batch={args.batch_id}"
    prepare_target(manifest_target, args.replace)
    with tempfile.TemporaryDirectory(prefix="evcharge-ingestion-") as temporary:
        local_manifest = Path(temporary) / "manifest.json"
        local_manifest.write_text(json.dumps(ingestion_manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        run_hdfs("-put", str(local_manifest), manifest_target)

    print(json.dumps({"batch_id": args.batch_id, "manifest": f"{manifest_target}/manifest.json", "datasets": datasets}, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    try:
        main()
    except (FileNotFoundError, FileExistsError, ValueError, subprocess.CalledProcessError, RuntimeError) as error:
        print(f"ODS ingestion failed: {error}", file=sys.stderr)
        raise SystemExit(1)
