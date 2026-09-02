"""构建包含业务种子数据、Cary 会话历史和小时指标的 SQLite 数据库。"""

from __future__ import annotations

import argparse
import hashlib
import os
import sqlite3
from datetime import datetime, timezone
from pathlib import Path

from ml.prepare_cary_data import (
    aggregate_sessions,
    load_sessions,
    load_station_config,
    sha256_file,
)


SOURCE_NAME = "Town of Cary Electric Vehicle Charging Stations"
SOURCE_URL = "https://catalog.data.gov/dataset/electric-vehicle-charging-stations"
LICENSE_NAME = "CC0"


def _sqlite_script(path: Path) -> str:
    """读取 SQLite 脚本，并移除仅由 sqlite3 命令行识别的点命令。"""

    return "\n".join(
        line
        for line in path.read_text(encoding="utf-8-sig").splitlines()
        if not line.lstrip().startswith(".")
    )


def _utc_text(value: datetime) -> str:
    """按数据库约定保存无歧义的 UTC 秒级时间。"""

    return value.astimezone(timezone.utc).strftime("%Y-%m-%d %H:%M:%S")


def _session_key(index: int, session) -> str:
    """从来源事实生成稳定键，使同一个批次可以检测重复会话。"""

    payload = "|".join(
        (
            str(index),
            str(session.station.station_id),
            session.start.isoformat(),
            str(session.duration_seconds),
            format(session.energy_kwh, ".12g"),
        )
    )
    return hashlib.sha256(payload.encode("utf-8")).hexdigest()


def build_database(
    input_path: Path,
    database_path: Path,
    mapping_path: Path,
    schema_path: Path,
    seed_path: Path,
    from_date: str,
    batch_no: str,
) -> dict[str, int | str]:
    """在临时数据库中完成全部写入和校验，成功后原子替换正式文件。"""

    stations, _ = load_station_config(mapping_path)
    from_time = datetime.fromisoformat(from_date).replace(tzinfo=timezone.utc)
    sessions, counters = load_sessions(input_path, stations, from_time)
    hourly_rows = aggregate_sessions(sessions)
    imported_at = datetime.now(timezone.utc).strftime("%Y-%m-%d %H:%M:%S")

    database_path.parent.mkdir(parents=True, exist_ok=True)
    temporary = database_path.with_suffix(database_path.suffix + ".tmp")
    if temporary.exists():
        temporary.unlink()

    connection = sqlite3.connect(temporary)
    try:
        connection.execute("PRAGMA foreign_keys = ON")
        connection.executescript(_sqlite_script(schema_path))
        connection.executescript(_sqlite_script(seed_path))
        with connection:
            cursor = connection.execute(
                """
                INSERT INTO data_import_batch (
                    batch_no, source_name, source_url, license_name, source_sha256,
                    time_shift_days, source_row_count, accepted_row_count, imported_at, note
                ) VALUES (?, ?, ?, ?, ?, 0, ?, ?, ?, ?)
                """,
                (
                    batch_no,
                    SOURCE_NAME,
                    SOURCE_URL,
                    LICENSE_NAME,
                    sha256_file(input_path),
                    counters["source_rows"],
                    counters["accepted_rows"],
                    imported_at,
                    "真实公开会话；未伪造用户、支付和订单状态",
                ),
            )
            batch_id = int(cursor.lastrowid)
            connection.executemany(
                """
                INSERT INTO charging_session_history (
                    batch_id, source_session_key, station_id, source_station_name,
                    start_at, end_at, duration_seconds, energy_kwh, created_at
                ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
                """,
                [
                    (
                        batch_id,
                        _session_key(index, session),
                        session.station.station_id,
                        session.station.label,
                        _utc_text(session.start),
                        _utc_text(session.end),
                        session.duration_seconds,
                        session.energy_kwh,
                        imported_at,
                    )
                    for index, session in enumerate(sessions, start=1)
                ],
            )
            connection.executemany(
                """
                INSERT INTO station_hourly_metric (
                    station_id, hour_start, total_pile_count, session_starts,
                    energy_kwh, charging_pile_minutes, average_occupied_count,
                    peak_occupied_count, average_available_count, station_load,
                    reserved_pile_minutes, fault_pile_minutes, offline_pile_minutes,
                    source_type, source_batch_id, created_at, updated_at
                ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 0, 0, 0,
                          'CARY_SIMULATION', ?, ?, ?)
                """,
                [
                    (
                        row["station_id"],
                        str(row["timestamp"]).replace("T", " ").removesuffix("Z"),
                        row["source_port_count"],
                        row["session_starts"],
                        row["energy_kwh"],
                        row["charging_minutes"],
                        row["average_occupied_count"],
                        row["peak_occupied_count"],
                        row["average_available_count"],
                        row["station_load"],
                        batch_id,
                        imported_at,
                        imported_at,
                    )
                    for row in hourly_rows
                ],
            )

        foreign_key_errors = connection.execute("PRAGMA foreign_key_check").fetchall()
        if foreign_key_errors:
            raise ValueError(f"foreign key check failed: {foreign_key_errors[:5]}")
        quick_check = connection.execute("PRAGMA quick_check").fetchone()[0]
        if quick_check != "ok":
            raise ValueError(f"SQLite quick_check failed: {quick_check}")
    except Exception:
        connection.close()
        temporary.unlink(missing_ok=True)
        raise
    else:
        connection.close()
        os.replace(temporary, database_path)

    return {
        "database": str(database_path),
        "batchNo": batch_no,
        "sourceRows": counters["source_rows"],
        "acceptedSessions": len(sessions),
        "hourlyRows": len(hourly_rows),
    }


def main() -> None:
    """解析命令行参数并创建可直接演示的数据库。"""

    root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--database", type=Path, required=True)
    parser.add_argument("--mapping", type=Path, default=root / "ml/config/station_mapping.json")
    parser.add_argument("--schema", type=Path, default=root / "database/schema.sql")
    parser.add_argument("--seed", type=Path, default=root / "database/init_data.sql")
    parser.add_argument("--from-date", default="2019-01-01")
    parser.add_argument("--batch-no", default="CARY-2019-V1")
    args = parser.parse_args()
    result = build_database(
        args.input,
        args.database,
        args.mapping,
        args.schema,
        args.seed,
        args.from_date,
        args.batch_no,
    )
    print(" ".join(f"{key}={value}" for key, value in result.items()))


if __name__ == "__main__":
    main()
