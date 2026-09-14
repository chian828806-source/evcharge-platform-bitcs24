"""从站点小时指标表导出 ML 黑盒所需的稳定 CSV 契约。"""

from __future__ import annotations

import argparse
import csv
import os
import sqlite3
from pathlib import Path


OUTPUT_COLUMNS = (
    "timestamp",
    "station_id",
    "total_pile_count",
    "session_starts",
    "charging_minutes",
    "energy_kwh",
    "average_occupied_count",
    "peak_occupied_count",
    "average_available_count",
    "station_load",
)


def export_history(database_path: Path, batch_no: str, output_path: Path) -> int:
    """导出指定来源批次；没有数据时拒绝产生空文件。"""

    connection = sqlite3.connect(f"file:{database_path}?mode=ro", uri=True)
    connection.row_factory = sqlite3.Row
    try:
        rows = connection.execute(
            """
            SELECT
                metric.hour_start || 'Z' AS timestamp,
                metric.station_id,
                metric.total_pile_count,
                metric.session_starts,
                metric.charging_pile_minutes AS charging_minutes,
                metric.energy_kwh,
                metric.average_occupied_count,
                metric.peak_occupied_count,
                metric.average_available_count,
                metric.station_load
            FROM station_hourly_metric AS metric
            JOIN data_import_batch AS batch ON batch.id = metric.source_batch_id
            WHERE batch.batch_no = ? AND metric.source_type = 'CARY_SIMULATION'
            ORDER BY metric.station_id, metric.hour_start
            """,
            (batch_no,),
        ).fetchall()
    finally:
        connection.close()
    if not rows:
        raise ValueError(f"no hourly metrics found for batch {batch_no}")

    output_path.parent.mkdir(parents=True, exist_ok=True)
    temporary = output_path.with_suffix(output_path.suffix + ".tmp")
    with temporary.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=OUTPUT_COLUMNS)
        writer.writeheader()
        writer.writerows(dict(row) for row in rows)
    os.replace(temporary, output_path)
    return len(rows)


def main() -> None:
    """解析命令行参数并执行数据库到 ML 文件的参考导出。"""

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--database", type=Path, required=True)
    parser.add_argument("--batch-no", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    count = export_history(args.database, args.batch_no, args.output)
    print(f"wrote {count} hourly rows to {args.output}")


if __name__ == "__main__":
    main()
