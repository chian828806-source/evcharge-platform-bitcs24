#!/usr/bin/env python3
"""Convert the official UrbanEV station-level 5-minute files into EVCharge Raw CSVs.

This adapter is the only place that knows the external UrbanEV layout.  Its output
uses the frozen project Raw contract, so the existing ODS, quality, DWD, DWS, MLlib,
Flask and Dashboard stages can remain unchanged.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
import random
import statistics
from collections import defaultdict
from datetime import datetime, timedelta, timezone
from pathlib import Path
from typing import Any, Iterable


DATASET_HEADERS: dict[str, list[str]] = {
    "users": ["user_id", "status", "created_at"],
    "stations": [
        "station_id", "station_no", "name", "district", "longitude", "latitude",
        "price_fen_per_kwh", "service_fee_fen_per_kwh", "status", "created_at",
    ],
    "piles": [
        "pile_id", "station_id", "pile_no", "type", "power_kw", "status",
        "total_charge_count", "total_charge_minutes", "total_energy_kwh", "updated_at",
    ],
    "orders": [
        "order_id", "order_no", "user_id", "station_id", "pile_id", "status",
        "start_at", "end_at", "charge_minutes", "energy_kwh", "amount_fen", "paid_at",
        "created_at",
    ],
    "sessions": [
        "source_session_key", "station_id", "source_station_name", "start_at", "end_at",
        "duration_seconds", "energy_kwh",
    ],
    "station_hourly_metrics": [
        "station_id", "hour_start", "total_pile_count", "session_starts", "energy_kwh",
        "charging_pile_minutes", "average_occupied_count", "average_available_count",
        "station_load", "source_type",
    ],
}

OFFICIAL_DOI = "10.5061/dryad.np5hqc04z"
SOURCE_PERIOD_START = "2022-09-01 00:00:00"


def parse_args() -> argparse.Namespace:
    """Read source, batch and smoke-test sizing options."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dataset-root", type=Path, required=True, help="Unzipped UrbanEVDataset directory")
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--batch-id", default="URBANEV-20230228-RAW-V1")
    parser.add_argument("--business-date", default="2023-02-28")
    parser.add_argument(
        "--max-stations", type=int, default=0,
        help="0 imports every station; a positive value selects the largest N stations for a quick demo",
    )
    parser.add_argument(
        "--min-samples-per-hour", type=int, default=10,
        help="Hours below this number are emitted as rejected project Raw rows",
    )
    parser.add_argument("--synthetic-users", type=int, default=500)
    parser.add_argument("--synthetic-seed", type=int, default=20260916)
    return parser.parse_args()


def canonical_id(value: Any) -> str:
    """Normalize CSV identifiers such as 1001 and 1001.0 to the same value."""
    text = str(value or "").strip()
    if not text:
        return ""
    try:
        number = float(text)
        if number.is_integer():
            return str(int(number))
    except ValueError:
        pass
    return text


def normalized_row(row: dict[str, str]) -> dict[str, str]:
    """Make external headers case-insensitive and remove an optional UTF-8 BOM."""
    return {(key or "").lstrip("\ufeff").strip().lower(): (value or "").strip() for key, value in row.items()}


def read_rows(path: Path) -> Iterable[dict[str, str]]:
    """Stream one official CSV without loading the complete archive into memory."""
    with path.open(newline="", encoding="utf-8-sig") as handle:
        for row in csv.DictReader(handle):
            yield normalized_row(row)


def to_float(value: Any) -> float | None:
    """Return a finite float or None for empty/invalid external values."""
    try:
        number = float(str(value).strip())
    except (TypeError, ValueError):
        return None
    return number if math.isfinite(number) else None


def to_int(value: Any) -> int | None:
    """Return an integer only when the external value is numerically integral."""
    number = to_float(value)
    return int(number) if number is not None and number.is_integer() else None


def parse_time(value: str) -> datetime | None:
    """Accept both documented minute timestamps and optional second timestamps."""
    for pattern in ("%Y-%m-%d %H:%M", "%Y-%m-%d %H:%M:%S"):
        try:
            return datetime.strptime(value, pattern)
        except ValueError:
            continue
    return None


def sha256sum(path: Path) -> str:
    """Hash generated Raw files for the immutable ODS ingestion manifest."""
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def find_station_layout(root: Path) -> tuple[Path, Path, Path]:
    """Locate the three station-level sources in the current official archive layout."""
    charge_dirs = [path for path in root.rglob("charge_5min") if "station-raw" in str(path.parent).lower()]
    if not charge_dirs:
        raise FileNotFoundError(
            "Cannot find 20220901-20230228_station-raw/charge_5min. "
            "The zone-cleaned charge_5min directory is not a Raw substitute."
        )
    charge_dir = sorted(charge_dirs, key=lambda path: len(path.parts))[0]
    station_root = charge_dir.parent
    station_info = station_root / "station_information.csv"
    pile_info = station_root / "pile_rated_power.csv"
    if not station_info.is_file() or not pile_info.is_file():
        raise FileNotFoundError(
            f"Expected station_information.csv and pile_rated_power.csv beside {charge_dir}"
        )
    return charge_dir, station_info, pile_info


def load_station_info(path: Path) -> dict[str, dict[str, Any]]:
    """Read official station coordinates, capacity and traffic-zone identity."""
    stations: dict[str, dict[str, Any]] = {}
    for row in read_rows(path):
        station_id = canonical_id(row.get("station_id"))
        capacity = to_int(row.get("charge_count"))
        longitude = to_float(row.get("longitude"))
        latitude = to_float(row.get("latitude"))
        if not station_id or capacity is None or capacity <= 0:
            continue
        stations[station_id] = {
            "station_id": station_id,
            "capacity": capacity,
            "longitude": longitude,
            "latitude": latitude,
            "fast_count": max(0, to_int(row.get("fast_count")) or 0),
            "slow_count": max(0, to_int(row.get("slow_count")) or 0),
            "taz_id": canonical_id(row.get("tazid")) or "UNKNOWN",
        }
    if not stations:
        raise ValueError(f"No usable stations in {path}")
    return stations


def choose_stations(stations: dict[str, dict[str, Any]], charge_dir: Path, limit: int) -> list[str]:
    """Select reproducibly: largest capacities first, then numeric station id."""
    available = [station_id for station_id in stations if (charge_dir / f"{station_id}.csv").is_file()]
    available.sort(key=lambda station_id: (-stations[station_id]["capacity"], int(station_id)))
    return available[:limit] if limit > 0 else available


def classify_pile(pile_type: str, power_kw: float | None) -> str:
    """Map UrbanEV DC/AC pile types to the project's FAST/SLOW enum."""
    normalized = pile_type.strip().upper()
    if normalized in {"DC", "直流", "FAST"}:
        return "FAST"
    if normalized in {"AC", "交流", "SLOW"}:
        return "SLOW"
    return "FAST" if power_kw is not None and power_kw > 22 else "SLOW"


def write_piles(path: Path, source: Path, selected: set[str]) -> tuple[int, dict[str, list[int]]]:
    """Project official pile power/type data; status and lifetime totals are neutral placeholders."""
    rows = []
    for row in read_rows(source):
        station_id = canonical_id(row.get("station_id"))
        if station_id not in selected:
            continue
        pile_no = row.get("pileno", "").strip()
        power_kw = to_float(row.get("power"))
        rows.append((station_id, pile_no, power_kw, row.get("piletype", "")))
    rows.sort(key=lambda item: (int(item[0]), item[1]))
    pile_ids_by_station: dict[str, list[int]] = defaultdict(list)
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=DATASET_HEADERS["piles"])
        writer.writeheader()
        for pile_id, (station_id, pile_no, power_kw, pile_type) in enumerate(rows, start=1):
            pile_ids_by_station[station_id].append(pile_id)
            writer.writerow({
                "pile_id": pile_id,
                "station_id": station_id,
                "pile_no": pile_no or f"URBANEV-{station_id}-{pile_id}",
                "type": classify_pile(pile_type, power_kw),
                "power_kw": "" if power_kw is None else round(power_kw, 3),
                # UrbanEV has aggregate station availability, not historical per-pile status.
                "status": "AVAILABLE",
                "total_charge_count": 0,
                "total_charge_minutes": 0,
                "total_energy_kwh": 0,
                "updated_at": "2023-02-28 23:55:00",
            })
    missing = sorted(selected - set(pile_ids_by_station), key=int)
    if missing:
        raise ValueError(f"Selected stations without pile_rated_power rows: {', '.join(missing[:10])}")
    return len(rows), dict(pile_ids_by_station)


def write_synthetic_users(path: Path, count: int, rng: random.Random) -> list[int]:
    """Create deterministic anonymous user dimensions for relational completeness."""
    if count <= 0:
        raise ValueError("--synthetic-users must be positive")
    user_ids: set[int] = set()
    while len(user_ids) < count:
        user_ids.add(rng.randint(10_000_000, 99_999_999))
    ordered = sorted(user_ids)
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=DATASET_HEADERS["users"])
        writer.writeheader()
        for user_id in ordered:
            created = datetime(2022, 9, 1) - timedelta(days=rng.randint(1, 730), minutes=rng.randint(0, 1439))
            writer.writerow({
                "user_id": user_id,
                "status": "FROZEN" if rng.random() < 0.02 else "NORMAL",
                "created_at": created.strftime("%Y-%m-%d %H:%M:%S"),
            })
    return ordered


def split_integer(total: int, count: int, rng: random.Random) -> list[int]:
    """Randomly split a non-negative integer while preserving it exactly."""
    if count <= 0:
        return []
    if total < 0:
        raise ValueError("A measured total cannot be negative")
    if total == 0:
        return [0] * count
    weights = [rng.uniform(0.5, 1.5) for _ in range(count)]
    raw = [total * weight / sum(weights) for weight in weights]
    shares = [int(value) for value in raw]
    remainder = total - sum(shares)
    ranked = sorted(range(count), key=lambda index: raw[index] - shares[index], reverse=True)
    for index in ranked[:remainder]:
        shares[index] += 1
    return shares


def split_energy(total: float, count: int, rng: random.Random) -> list[float]:
    """Split official energy in 0.000001 kWh units without rounding drift."""
    units = max(0, int(round(total * 1_000_000)))
    return [value / 1_000_000 for value in split_integer(units, count, rng)]


def split_duration_seconds(total_minutes: float, count: int, rng: random.Random) -> list[int]:
    """Split official duration in seconds; each generated session remains positive."""
    if count <= 0:
        return []
    seconds = int(round(total_minutes * 60))
    if seconds < count:
        raise ValueError("The session count exceeds the measured positive duration in seconds")
    baseline = [1] * count
    remainder = split_integer(seconds - count, count, rng)
    return [baseline[index] + remainder[index] for index in range(count)]


def write_synthetic_business_rows(
    *,
    station: dict[str, Any],
    hour_start: datetime,
    inferred_starts: int,
    energy_kwh: float,
    charging_minutes: float,
    price_fen_per_kwh: int,
    service_fee_fen_per_kwh: int,
    user_ids: list[int],
    pile_ids: list[int],
    orders_writer: csv.DictWriter,
    sessions_writer: csv.DictWriter,
    rng: random.Random,
    state: dict[str, int],
) -> int:
    """Attach official hourly totals to deterministic synthetic users/orders/sessions."""
    has_activity = energy_kwh > 0 or charging_minutes > 0 or inferred_starts > 0
    desired_count = min(station["capacity"], max(inferred_starts, 1)) if has_activity else 0
    measured_seconds = max(0, int(round(charging_minutes * 60)))
    order_count = min(desired_count, measured_seconds) if measured_seconds else desired_count
    energies = split_energy(energy_kwh, order_count, rng)
    if measured_seconds:
        duration_seconds = split_duration_seconds(charging_minutes, order_count, rng)
    else:
        # A source hour can contain energy/occupancy but no duration because the
        # aggregate fields were measured independently.  Keep the official metric
        # unchanged and record the smallest valid synthetic session explicitly.
        duration_seconds = [60] * order_count
        state["duration_imputed_seconds"] += sum(duration_seconds)
    for index in range(order_count):
        state["order_id"] += 1
        order_id = state["order_id"]
        order_no = f"URBANEV-SYN-{station['station_id']}-{hour_start:%Y%m%d%H}-{index + 1:03d}"
        start_at = hour_start + timedelta(minutes=min(55, int((index + 1) * 60 / (order_count + 1))))
        end_at = start_at + timedelta(seconds=duration_seconds[index])
        paid_at = end_at + timedelta(minutes=rng.randint(1, 15))
        user_id = rng.choice(user_ids)
        pile_id = rng.choice(pile_ids)
        amount_fen = int(round(energies[index] * (price_fen_per_kwh + service_fee_fen_per_kwh)))
        orders_writer.writerow({
            "order_id": order_id,
            "order_no": order_no,
            "user_id": user_id,
            "station_id": station["station_id"],
            "pile_id": pile_id,
            "status": "COMPLETED",
            "start_at": start_at.strftime("%Y-%m-%d %H:%M:%S"),
            "end_at": end_at.strftime("%Y-%m-%d %H:%M:%S"),
            # The frozen order contract stores integer minutes.  Session seconds below
            # preserve the finer official duration used for analytics.
            "charge_minutes": max(1, int(round(duration_seconds[index] / 60))),
            "energy_kwh": energies[index],
            "amount_fen": amount_fen,
            "paid_at": paid_at.strftime("%Y-%m-%d %H:%M:%S"),
            "created_at": start_at.strftime("%Y-%m-%d %H:%M:%S"),
        })
        sessions_writer.writerow({
            "source_session_key": order_no,
            "station_id": station["station_id"],
            "source_station_name": f"UrbanEV 深圳站点 {station['station_id']}",
            "start_at": start_at.strftime("%Y-%m-%d %H:%M:%S"),
            "end_at": end_at.strftime("%Y-%m-%d %H:%M:%S"),
            "duration_seconds": duration_seconds[index],
            "energy_kwh": energies[index],
        })
    return order_count


def invalid_five_minute_row(values: dict[str, float | None], capacity: int) -> bool:
    """Flag raw samples that cannot safely contribute to one hourly metric."""
    required = ("busy", "idle", "duration", "volume", "s_price", "e_price")
    if any(values[name] is None for name in required):
        return True
    assert all(values[name] is not None for name in required)
    if any(values[name] < 0 for name in required):
        return True
    return values["busy"] + values["idle"] > capacity


def process_station_file(
    path: Path,
    station: dict[str, Any],
    writer: csv.DictWriter,
    min_samples: int,
    user_ids: list[int],
    pile_ids: list[int],
    orders_writer: csv.DictWriter,
    sessions_writer: csv.DictWriter,
    rng: random.Random,
    state: dict[str, int],
) -> dict[str, Any]:
    """Aggregate one station's official 5-minute records to project station-hour rows."""
    by_hour: dict[datetime, list[dict[str, float | None]]] = defaultdict(list)
    invalid_timestamps = 0
    prices = {"e_price": [], "s_price": []}
    source_rows = 0
    for row in read_rows(path):
        source_rows += 1
        timestamp = parse_time(row.get("time", ""))
        if timestamp is None:
            invalid_timestamps += 1
            continue
        values = {name: to_float(row.get(name)) for name in ("busy", "idle", "duration", "volume", "s_price", "e_price")}
        by_hour[timestamp.replace(minute=0, second=0, microsecond=0)].append(values)
        for name in prices:
            value = values[name]
            if value is not None and value >= 0:
                prices[name].append(value)

    price_fen = int(round(statistics.median(prices["e_price"]) * 100)) if prices["e_price"] else 0
    service_fee_fen = int(round(statistics.median(prices["s_price"]) * 100)) if prices["s_price"] else 0
    previous_busy: float | None = None
    accepted_hours = 0
    rejected_hours = 0
    for hour_start in sorted(by_hour):
        samples = by_hour[hour_start]
        invalid = len(samples) < min_samples or any(
            invalid_five_minute_row(values, station["capacity"]) for values in samples
        )
        if invalid:
            rejected_hours += 1
            writer.writerow({
                "station_id": station["station_id"],
                "hour_start": hour_start.strftime("%Y-%m-%d %H:%M:%S"),
                "total_pile_count": station["capacity"],
                "session_starts": "",
                "energy_kwh": "",
                "charging_pile_minutes": "",
                "average_occupied_count": "",
                "average_available_count": "",
                "station_load": "",
                "source_type": "URBANEV",
            })
            previous_busy = None
            continue

        busy_values = [float(values["busy"]) for values in samples]
        idle_values = [float(values["idle"]) for values in samples]
        session_starts = 0
        for busy in busy_values:
            if previous_busy is not None:
                session_starts += max(0, int(round(busy - previous_busy)))
            previous_busy = busy
        average_busy = statistics.fmean(busy_values)
        average_idle = statistics.fmean(idle_values)
        energy_kwh = sum(float(values["volume"]) for values in samples)
        charging_minutes = sum(float(values["duration"]) for values in samples) * 60.0
        generated_orders = write_synthetic_business_rows(
            station=station,
            hour_start=hour_start,
            inferred_starts=session_starts,
            energy_kwh=energy_kwh,
            charging_minutes=charging_minutes,
            price_fen_per_kwh=price_fen,
            service_fee_fen_per_kwh=service_fee_fen,
            user_ids=user_ids,
            pile_ids=pile_ids,
            orders_writer=orders_writer,
            sessions_writer=sessions_writer,
            rng=rng,
            state=state,
        )
        writer.writerow({
            "station_id": station["station_id"],
            "hour_start": hour_start.strftime("%Y-%m-%d %H:%M:%S"),
            "total_pile_count": station["capacity"],
            "session_starts": generated_orders,
            "energy_kwh": round(energy_kwh, 6),
            "charging_pile_minutes": round(charging_minutes, 6),
            "average_occupied_count": round(average_busy, 6),
            "average_available_count": round(average_idle, 6),
            "station_load": round(average_busy / station["capacity"], 8),
            "source_type": "URBANEV",
        })
        accepted_hours += 1

    return {
        "source_rows": source_rows,
        "invalid_timestamps": invalid_timestamps,
        "accepted_hours": accepted_hours,
        "rejected_hours": rejected_hours,
        "price_fen_per_kwh": price_fen,
        "service_fee_fen_per_kwh": service_fee_fen,
    }


def count_rows(path: Path) -> int:
    """Count data rows in one generated CSV without loading it."""
    with path.open(encoding="utf-8") as handle:
        return max(0, sum(1 for _ in handle) - 1)


def prepare(args: argparse.Namespace) -> dict[str, Any]:
    """Run the complete external-source to project-Raw conversion."""
    datetime.strptime(args.business_date, "%Y-%m-%d")
    if args.max_stations < 0:
        raise ValueError("--max-stations must be zero or positive")
    if not 1 <= args.min_samples_per_hour <= 12:
        raise ValueError("--min-samples-per-hour must be between 1 and 12")
    if args.synthetic_users <= 0:
        raise ValueError("--synthetic-users must be positive")

    charge_dir, station_info_path, pile_info_path = find_station_layout(args.dataset_root.resolve())
    station_info = load_station_info(station_info_path)
    selected_ids = choose_stations(station_info, charge_dir, args.max_stations)
    if not selected_ids:
        raise ValueError("No station CSV filename matches station_information.csv")

    output = args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    selected = set(selected_ids)
    rng = random.Random(args.synthetic_seed)
    user_ids = write_synthetic_users(output / "users.csv", args.synthetic_users, rng)
    pile_rows, pile_ids_by_station = write_piles(output / "piles.csv", pile_info_path, selected)

    station_reports: dict[str, dict[str, Any]] = {}
    metrics_path = output / "station_hourly_metrics.csv"
    state = {"order_id": 0, "duration_imputed_seconds": 0}
    with (
        metrics_path.open("w", newline="", encoding="utf-8") as metrics_handle,
        (output / "orders.csv").open("w", newline="", encoding="utf-8") as orders_handle,
        (output / "sessions.csv").open("w", newline="", encoding="utf-8") as sessions_handle,
    ):
        writer = csv.DictWriter(metrics_handle, fieldnames=DATASET_HEADERS["station_hourly_metrics"])
        orders_writer = csv.DictWriter(orders_handle, fieldnames=DATASET_HEADERS["orders"])
        sessions_writer = csv.DictWriter(sessions_handle, fieldnames=DATASET_HEADERS["sessions"])
        writer.writeheader()
        orders_writer.writeheader()
        sessions_writer.writeheader()
        for station_id in selected_ids:
            station = {**station_info[station_id], "station_id": station_id}
            station_reports[station_id] = process_station_file(
                charge_dir / f"{station_id}.csv",
                station,
                writer,
                args.min_samples_per_hour,
                user_ids,
                pile_ids_by_station[station_id],
                orders_writer,
                sessions_writer,
                rng,
                state,
            )

    stations_path = output / "stations.csv"
    with stations_path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=DATASET_HEADERS["stations"])
        writer.writeheader()
        for station_id in selected_ids:
            station = station_info[station_id]
            report = station_reports[station_id]
            writer.writerow({
                "station_id": station_id,
                "station_no": f"URBANEV-{station_id}",
                "name": f"UrbanEV 深圳站点 {station_id}",
                "district": f"TAZ-{station['taz_id']}",
                "longitude": "" if station["longitude"] is None else station["longitude"],
                "latitude": "" if station["latitude"] is None else station["latitude"],
                "price_fen_per_kwh": report["price_fen_per_kwh"],
                "service_fee_fen_per_kwh": report["service_fee_fen_per_kwh"],
                "status": "NORMAL",
                "created_at": SOURCE_PERIOD_START,
            })

    datasets: dict[str, Any] = {}
    for dataset in DATASET_HEADERS:
        path = output / f"{dataset}.csv"
        datasets[dataset] = {
            "file": path.name,
            "rows": count_rows(path),
            "sha256": sha256sum(path),
        }
    report = {
        "source": "UrbanEV station-level 5-minute Raw",
        "doi": OFFICIAL_DOI,
        "selected_station_count": len(selected_ids),
        "synthetic_user_count": len(user_ids),
        "synthetic_order_count": state["order_id"],
        "synthetic_duration_imputed_seconds": state["duration_imputed_seconds"],
        "pile_rows": pile_rows,
        "source_five_minute_rows": sum(item["source_rows"] for item in station_reports.values()),
        "invalid_timestamp_rows": sum(item["invalid_timestamps"] for item in station_reports.values()),
        "accepted_hour_rows": sum(item["accepted_hours"] for item in station_reports.values()),
        "rejected_hour_rows": sum(item["rejected_hours"] for item in station_reports.values()),
        "min_samples_per_hour": args.min_samples_per_hour,
    }
    manifest = {
        "batch_id": args.batch_id,
        "business_date": args.business_date,
        "generated_at": datetime.now(timezone.utc).isoformat(timespec="seconds"),
        # Retained for compatibility with the existing immutable ODS manifest schema.
        "seed": args.synthetic_seed,
        "injection_rate": 0.0,
        "source_name": "UrbanEV",
        "source_doi": OFFICIAL_DOI,
        "source_mode": "official-station-raw-5min",
        "synthetic_dimensions": {
            "enabled": True,
            "seed": args.synthetic_seed,
            "fields": ["user_id", "order_id", "pile_assignment", "start_at", "end_at", "paid_at"],
            "invariants": [
                "official station-hour metrics remain unchanged",
                "synthetic order energy sums to station-hour energy_kwh at 0.000001 kWh precision",
                "for positive source duration, synthetic session seconds sum to rounded station-hour charging_pile_minutes x 60",
            ],
        },
        "adapter_report": report,
        "datasets": datasets,
    }
    (output / "manifest.json").write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    print(json.dumps(manifest, ensure_ascii=False, indent=2))
    return manifest


def main() -> None:
    prepare(parse_args())


if __name__ == "__main__":
    main()
