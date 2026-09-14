"""将 Town of Cary 充电会话转换为连续 UTC 小时级站点负荷。"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
from collections import defaultdict
from dataclasses import dataclass
from datetime import datetime, timedelta, timezone
from pathlib import Path
from typing import Iterable


UTC = timezone.utc


@dataclass(frozen=True)
class StationConfig:
    """保存一个来源地址到项目站点的稳定映射。"""

    source_key: str
    station_id: int
    source_port_count: int
    project_pile_count: int
    label: str


@dataclass(frozen=True)
class Session:
    """保存清洗后的单次充电会话。"""

    start: datetime
    duration_seconds: int
    energy_kwh: float
    station: StationConfig

    @property
    def end(self) -> datetime:
        return self.start + timedelta(seconds=self.duration_seconds)


def normalize_source_key(address: str, postal_code: str) -> str:
    """规范化地址键，避免场所副标题变化拆分同一物理站点。"""

    normalized_address = " ".join(address.strip().upper().split())
    normalized_postal = " ".join(postal_code.strip().upper().split())
    return f"{normalized_address}|{normalized_postal}"


def parse_duration(value: str) -> int:
    """把 HH:MM:SS 充电时长转换为秒。"""

    parts = value.strip().split(":")
    if len(parts) != 3:
        raise ValueError(f"invalid duration: {value!r}")
    hours, minutes, seconds = (int(part) for part in parts)
    if minutes not in range(60) or seconds not in range(60):
        raise ValueError(f"invalid duration: {value!r}")
    return hours * 3600 + minutes * 60 + seconds


def load_station_config(path: Path) -> tuple[dict[str, StationConfig], str]:
    """读取并校验站点映射配置。"""

    document = json.loads(path.read_text(encoding="utf-8"))
    if document.get("schemaVersion") != "1.0":
        raise ValueError("station mapping schemaVersion must be 1.0")
    timezone_name = document.get("calendarTimezone")
    if not isinstance(timezone_name, str) or not timezone_name:
        raise ValueError("calendarTimezone is required")

    result: dict[str, StationConfig] = {}
    station_ids: set[int] = set()
    for item in document.get("stations", []):
        station = StationConfig(
            source_key=str(item["sourceKey"]),
            station_id=int(item["stationId"]),
            source_port_count=int(item["sourcePortCount"]),
            project_pile_count=int(item["projectPileCount"]),
            label=str(item.get("label", "")),
        )
        if station.source_port_count <= 0:
            raise ValueError("sourcePortCount must be positive")
        if station.project_pile_count <= 0:
            raise ValueError("projectPileCount must be positive")
        if station.source_key in result or station.station_id in station_ids:
            raise ValueError("station mapping keys and stationId values must be unique")
        result[station.source_key] = station
        station_ids.add(station.station_id)
    if not result:
        raise ValueError("station mapping must contain at least one station")
    return result, timezone_name


def load_sessions(
    path: Path,
    stations: dict[str, StationConfig],
    from_date: datetime | None,
) -> tuple[list[Session], dict[str, int]]:
    """读取原始 CSV，并返回有效会话及数据质量计数。"""

    sessions: list[Session] = []
    counters: dict[str, int] = defaultdict(int)
    with path.open("r", encoding="utf-8-sig", newline="") as stream:
        reader = csv.DictReader(stream, delimiter=";")
        required = {
            "start_date",
            "charging_time_hh_mm_ss",
            "energy_kwh",
            "address_1",
            "zip_postal_code",
        }
        if not reader.fieldnames or not required.issubset(reader.fieldnames):
            raise ValueError("source CSV is missing required columns")

        for row in reader:
            counters["source_rows"] += 1
            try:
                start = datetime.fromisoformat(row["start_date"])
                if start.tzinfo is None:
                    raise ValueError("start_date must contain an offset")
                start = start.astimezone(UTC)
                duration = parse_duration(row["charging_time_hh_mm_ss"])
                energy = float(row["energy_kwh"])
            except (TypeError, ValueError):
                counters["invalid_rows"] += 1
                continue

            source_key = normalize_source_key(
                row["address_1"], row["zip_postal_code"]
            )
            station = stations.get(source_key)
            if station is None:
                counters["unmapped_rows"] += 1
                continue
            if duration <= 0 or energy < 0:
                counters["invalid_rows"] += 1
                continue
            if from_date is not None and start < from_date:
                counters["before_from_date"] += 1
                continue
            sessions.append(Session(start, duration, energy, station))
    counters["accepted_rows"] = len(sessions)
    return sessions, dict(counters)


def floor_hour(value: datetime) -> datetime:
    """返回 UTC 时间所在小时的起点。"""

    return value.astimezone(UTC).replace(minute=0, second=0, microsecond=0)


def iter_overlapping_hours(start: datetime, end: datetime) -> Iterable[datetime]:
    """依次产生与半开区间 [start, end) 相交的 UTC 小时。"""

    cursor = floor_hour(start)
    while cursor < end:
        yield cursor
        cursor += timedelta(hours=1)


def calculate_hourly_peaks(
    sessions: list[Session],
) -> dict[tuple[int, datetime], int]:
    """通过会话起止事件计算每小时最大并发占用数。"""

    events_by_station: dict[int, dict[datetime, int]] = defaultdict(
        lambda: defaultdict(int)
    )
    for session in sessions:
        events_by_station[session.station.station_id][session.start] += 1
        events_by_station[session.station.station_id][session.end] -= 1

    peaks: dict[tuple[int, datetime], int] = defaultdict(int)
    for station_id, event_map in events_by_station.items():
        active = 0
        previous: datetime | None = None
        for event_time in sorted(event_map):
            # previous到当前事件之间的所有小时都具有相同并发数。
            if previous is not None and previous < event_time and active > 0:
                for hour in iter_overlapping_hours(previous, event_time):
                    key = (station_id, hour)
                    peaks[key] = max(peaks[key], active)
            active += event_map[event_time]
            if active < 0:
                raise ValueError("session events produced negative concurrency")
            previous = event_time
    return dict(peaks)


def aggregate_sessions(sessions: list[Session]) -> list[dict[str, object]]:
    """把会话按站点和 UTC 小时聚合，并补齐没有会话的小时。"""

    if not sessions:
        raise ValueError("no sessions matched the station mapping and date filter")

    buckets: dict[tuple[int, datetime], dict[str, float]] = defaultdict(
        lambda: {
            "session_starts": 0.0,
            "charging_minutes": 0.0,
            "energy_kwh": 0.0,
        }
    )
    bounds: dict[int, list[datetime]] = {}
    configs: dict[int, StationConfig] = {}

    for session in sessions:
        station_id = session.station.station_id
        configs[station_id] = session.station
        first_hour = floor_hour(session.start)
        last_hour = floor_hour(session.end - timedelta(microseconds=1))
        bounds.setdefault(station_id, [first_hour, last_hour])
        bounds[station_id][0] = min(bounds[station_id][0], first_hour)
        bounds[station_id][1] = max(bounds[station_id][1], last_hour)
        buckets[(station_id, first_hour)]["session_starts"] += 1

        for hour in iter_overlapping_hours(session.start, session.end):
            next_hour = hour + timedelta(hours=1)
            overlap_seconds = (
                min(session.end, next_hour) - max(session.start, hour)
            ).total_seconds()
            if overlap_seconds <= 0:
                continue
            bucket = buckets[(station_id, hour)]
            bucket["charging_minutes"] += overlap_seconds / 60
            bucket["energy_kwh"] += (
                session.energy_kwh * overlap_seconds / session.duration_seconds
            )

    peaks = calculate_hourly_peaks(sessions)
    rows: list[dict[str, object]] = []
    for station_id in sorted(bounds):
        station = configs[station_id]
        cursor, final_hour = bounds[station_id]
        while cursor <= final_hour:
            bucket = buckets[(station_id, cursor)]
            average_occupied = bucket["charging_minutes"] / 60
            station_load = average_occupied / station.source_port_count
            if station_load > 1.0 + 1e-9:
                raise ValueError(
                    f"sourcePortCount too small for station {station_id} at {cursor}"
                )
            rows.append(
                {
                    "timestamp": cursor.isoformat().replace("+00:00", "Z"),
                    "station_id": station_id,
                    "source_port_count": station.source_port_count,
                    "session_starts": int(bucket["session_starts"]),
                    "charging_minutes": round(bucket["charging_minutes"], 4),
                    "energy_kwh": round(bucket["energy_kwh"], 6),
                    "average_occupied_count": round(average_occupied, 6),
                    "peak_occupied_count": peaks.get((station_id, cursor), 0),
                    "average_available_count": round(
                        max(0.0, station.source_port_count - average_occupied), 6
                    ),
                    "station_load": round(min(1.0, station_load), 6),
                }
            )
            cursor += timedelta(hours=1)
    return rows


def write_csv(path: Path, rows: list[dict[str, object]]) -> None:
    """以原子替换方式写出处理后的 CSV。"""

    if not rows:
        raise ValueError("cannot write an empty processed dataset")
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    with temporary.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    temporary.replace(path)


def sha256_file(path: Path) -> str:
    """计算输入文件哈希，供报告和模型产物追踪。"""

    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def main() -> None:
    """解析命令行、执行转换并输出可复现性报告。"""

    parser = argparse.ArgumentParser()
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument(
        "--mapping",
        type=Path,
        default=Path(__file__).parent / "config" / "station_mapping.json",
    )
    parser.add_argument("--from-date", help="UTC日期，例如 2019-01-01")
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()

    stations, calendar_timezone = load_station_config(args.mapping)
    from_date = (
        datetime.fromisoformat(args.from_date).replace(tzinfo=UTC)
        if args.from_date
        else None
    )
    sessions, counters = load_sessions(args.input, stations, from_date)
    rows = aggregate_sessions(sessions)
    write_csv(args.output, rows)

    report = {
        "schemaVersion": "1.0",
        "sourceSha256": sha256_file(args.input),
        "calendarTimezone": calendar_timezone,
        "qualityCounters": counters,
        "hourlyRows": len(rows),
        "stationIds": sorted({int(row["station_id"]) for row in rows}),
        "firstTimestamp": rows[0]["timestamp"],
        "lastTimestamp": rows[-1]["timestamp"],
    }
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(
            json.dumps(report, ensure_ascii=False, indent=2) + "\n",
            encoding="utf-8",
        )
    print(json.dumps(report, ensure_ascii=False))


if __name__ == "__main__":
    main()
