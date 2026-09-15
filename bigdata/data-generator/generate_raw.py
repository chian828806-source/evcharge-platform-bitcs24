#!/usr/bin/env python3
"""Generate reproducible EVCharge Raw CSV datasets with auditable DQ defects."""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import random
from collections import Counter, defaultdict
from datetime import datetime, timedelta
from pathlib import Path
from typing import Any, Callable


DATASETS: dict[str, list[str]] = {
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


def timestamp(value: datetime) -> str:
    return value.strftime("%Y-%m-%d %H:%M:%S")


def sha256sum(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def load_config(path: Path) -> dict[str, Any]:
    with path.open(encoding="utf-8") as handle:
        return json.load(handle)


def write_csv(path: Path, headers: list[str], rows: list[dict[str, Any]]) -> None:
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=headers, extrasaction="ignore")
        writer.writeheader()
        writer.writerows(rows)


def clone(row: dict[str, Any], **updates: Any) -> dict[str, Any]:
    copied = dict(row)
    copied.update(updates)
    return copied


def append_injections(
    dataset: str,
    rows: list[dict[str, Any]],
    count: int,
    factories: list[tuple[str, Callable[[dict[str, Any]], dict[str, Any]]]],
) -> tuple[list[dict[str, Any]], Counter[str]]:
    """Append one defective copy per factory invocation without mutating valid records."""
    injected: Counter[str] = Counter()
    for index in range(count):
        rule_id, factory = factories[index % len(factories)]
        source = rows[index % len(rows)]
        rows.append(factory(source))
        injected[rule_id] += 1
    return rows, injected


def generate(config: dict[str, Any], output_dir: Path) -> dict[str, Any]:
    rng = random.Random(int(config["seed"]))
    business_date = datetime.strptime(config["business_date"], "%Y-%m-%d")
    scale = config["scale"]
    rate = float(config["injection_rate"])
    if not 0.01 <= rate <= 0.03:
        raise ValueError("injection_rate must be between 0.01 and 0.03")

    districts = ["甘井子区", "沙河口区", "西岗区", "中山区", "旅顺口区"]
    output_dir.mkdir(parents=True, exist_ok=True)

    users = [
        {
            "user_id": index,
            "status": "FROZEN" if index % 31 == 0 else "NORMAL",
            "created_at": timestamp(business_date - timedelta(days=90 + index % 180, hours=index % 24)),
        }
        for index in range(1, int(scale["users"]) + 1)
    ]

    stations = []
    for index in range(1, int(scale["stations"]) + 1):
        stations.append(
            {
                "station_id": index,
                "station_no": f"ST{index:03d}",
                "name": f"EVCharge {districts[(index - 1) % len(districts)]} {index}号站",
                "district": districts[(index - 1) % len(districts)],
                "longitude": f"{121.50 + rng.random() * 0.22:.6f}",
                "latitude": f"{38.82 + rng.random() * 0.18:.6f}",
                "price_fen_per_kwh": rng.choice([88, 98, 108, 120, 132]),
                "service_fee_fen_per_kwh": rng.choice([10, 15, 20, 25]),
                "status": "DISABLED" if index % 29 == 0 else "NORMAL",
                "created_at": timestamp(business_date - timedelta(days=360 + index)),
            }
        )

    piles = []
    pile_id = 1
    piles_per_station = int(scale["piles_per_station"])
    for station in stations:
        for within_station in range(1, piles_per_station + 1):
            pile_type = "FAST" if within_station % 3 else "SLOW"
            piles.append(
                {
                    "pile_id": pile_id,
                    "station_id": station["station_id"],
                    "pile_no": f"{station['station_no']}-P{within_station:02d}",
                    "type": pile_type,
                    "power_kw": 120.0 if pile_type == "FAST" else 7.0,
                    "status": rng.choices(
                        ["AVAILABLE", "RESERVED", "CHARGING", "FAULT", "OFFLINE", "RESTARTING"],
                        weights=[55, 8, 20, 6, 6, 5],
                        k=1,
                    )[0],
                    "total_charge_count": rng.randint(30, 900),
                    "total_charge_minutes": rng.randint(4_000, 80_000),
                    "total_energy_kwh": f"{rng.uniform(600, 12000):.3f}",
                    "updated_at": timestamp(business_date + timedelta(hours=rng.randrange(24))),
                }
            )
            pile_id += 1

    piles_by_station: dict[int, list[dict[str, Any]]] = defaultdict(list)
    for pile in piles:
        piles_by_station[int(pile["station_id"])].append(pile)

    orders = []
    for index in range(1, int(scale["orders"]) + 1):
        station = rng.choice(stations)
        pile = rng.choice(piles_by_station[int(station["station_id"])])
        created_at = business_date - timedelta(days=rng.randrange(14), minutes=rng.randrange(24 * 60))
        status = rng.choices(
            ["COMPLETED", "PENDING_PAYMENT", "CHARGING", "CREATED", "CANCELLED"],
            weights=[70, 8, 7, 8, 7],
            k=1,
        )[0]
        start_at = end_at = paid_at = ""
        charge_minutes = 0
        energy_kwh = 0.0
        amount_fen = 0
        if status != "CREATED" and status != "CANCELLED":
            start = created_at + timedelta(minutes=rng.randint(5, 40))
            charge_minutes = rng.randint(10, 120)
            energy_kwh = round(charge_minutes * float(pile["power_kw"]) / 60 * rng.uniform(0.45, 0.82), 3)
            start_at = timestamp(start)
            if status != "CHARGING":
                end = start + timedelta(minutes=charge_minutes)
                end_at = timestamp(end)
                amount_fen = int(energy_kwh * (int(station["price_fen_per_kwh"]) + int(station["service_fee_fen_per_kwh"])))
                if status == "COMPLETED":
                    paid_at = timestamp(end + timedelta(minutes=rng.randint(1, 15)))
        orders.append(
            {
                "order_id": index,
                "order_no": f"ORD{business_date:%Y%m%d}{index:06d}",
                "user_id": rng.randint(1, int(scale["users"])),
                "station_id": station["station_id"],
                "pile_id": pile["pile_id"],
                "status": status,
                "start_at": start_at,
                "end_at": end_at,
                "charge_minutes": charge_minutes,
                "energy_kwh": f"{energy_kwh:.3f}",
                "amount_fen": amount_fen,
                "paid_at": paid_at,
                "created_at": timestamp(created_at),
            }
        )

    sessions = []
    for index in range(1, int(scale["sessions"]) + 1):
        station = rng.choice(stations)
        start = business_date - timedelta(days=rng.randrange(14), hours=rng.randrange(24), minutes=rng.randrange(60))
        duration = rng.randint(600, 10_800)
        end = start + timedelta(seconds=duration)
        sessions.append(
            {
                "source_session_key": f"EXT-{business_date:%Y%m%d}-{index:06d}",
                "station_id": station["station_id"],
                "source_station_name": station["name"],
                "start_at": timestamp(start),
                "end_at": timestamp(end),
                "duration_seconds": duration,
                "energy_kwh": f"{rng.uniform(3.0, 80.0):.3f}",
            }
        )

    metrics = []
    metric_days = int(scale["metric_days"])
    for day_offset in range(metric_days):
        for hour in range(24):
            hour_start = business_date - timedelta(days=metric_days - 1 - day_offset) + timedelta(hours=hour)
            for station in stations:
                total = piles_per_station
                average_occupied = round(rng.uniform(0.0, total * 0.8), 3)
                average_available = round(max(0.0, total - average_occupied - rng.uniform(0.0, 0.4)), 3)
                minutes = round(average_occupied * 60, 3)
                metrics.append(
                    {
                        "station_id": station["station_id"],
                        "hour_start": timestamp(hour_start),
                        "total_pile_count": total,
                        "session_starts": rng.randint(0, 8),
                        "energy_kwh": f"{rng.uniform(0.0, 110.0):.3f}",
                        "charging_pile_minutes": minutes,
                        "average_occupied_count": average_occupied,
                        "average_available_count": average_available,
                        "station_load": f"{average_occupied / total:.4f}",
                        "source_type": "BUSINESS",
                    }
                )

    injection_counts: dict[str, Counter[str]] = {}
    users, injection_counts["users"] = append_injections(
        "users", users, max(1, round(len(users) * rate)),
        [
            ("DQ-001", lambda row: clone(row, user_id="")),
            ("DQ-001", lambda row: clone(row)),
            ("DQ-002", lambda row: clone(row, created_at="")),
            ("DQ-005", lambda row: clone(row, status="UNKNOWN")),
        ],
    )
    stations, injection_counts["stations"] = append_injections(
        "stations", stations, max(1, round(len(stations) * rate)),
        [
            ("DQ-002", lambda row: clone(row, longitude="not-a-number")),
            ("DQ-003", lambda row: clone(row, price_fen_per_kwh=-1)),
            ("DQ-005", lambda row: clone(row, status="ARCHIVED")),
        ],
    )
    piles, injection_counts["piles"] = append_injections(
        "piles", piles, max(1, round(len(piles) * rate)),
        [
            ("DQ-001", lambda row: clone(row)),
            ("DQ-003", lambda row: clone(row, power_kw=-7)),
            ("DQ-005", lambda row: clone(row, type="ULTRA")),
            ("DQ-006", lambda row: clone(row, station_id=999999)),
        ],
    )
    orders, injection_counts["orders"] = append_injections(
        "orders", orders, max(1, round(len(orders) * rate)),
        [
            ("DQ-001", lambda row: clone(row)),
            ("DQ-003", lambda row: clone(row, energy_kwh=-1.0)),
            ("DQ-004", lambda row: clone(row, start_at=f"{business_date:%Y-%m-%d} 12:00:00", end_at=f"{business_date:%Y-%m-%d} 11:00:00")),
            ("DQ-005", lambda row: clone(row, status="BROKEN")),
            ("DQ-006", lambda row: clone(row, station_id=999999, pile_id=999999)),
        ],
    )
    sessions, injection_counts["sessions"] = append_injections(
        "sessions", sessions, max(1, round(len(sessions) * rate)),
        [
            ("DQ-001", lambda row: clone(row)),
            ("DQ-003", lambda row: clone(row, duration_seconds=-1)),
            ("DQ-004", lambda row: clone(row, start_at=f"{business_date:%Y-%m-%d} 12:00:00", end_at=f"{business_date:%Y-%m-%d} 11:00:00")),
            ("DQ-006", lambda row: clone(row, station_id=999999)),
        ],
    )
    metrics, injection_counts["station_hourly_metrics"] = append_injections(
        "station_hourly_metrics", metrics, max(1, round(len(metrics) * rate)),
        [
            ("DQ-001", lambda row: clone(row)),
            ("DQ-003", lambda row: clone(row, station_load=1.5)),
            ("DQ-005", lambda row: clone(row, source_type="MANUAL")),
            ("DQ-006", lambda row: clone(row, station_id=999999)),
        ],
    )

    rows_by_dataset = {
        "users": users,
        "stations": stations,
        "piles": piles,
        "orders": orders,
        "sessions": sessions,
        "station_hourly_metrics": metrics,
    }
    manifest: dict[str, Any] = {
        "batch_id": config["batch_id"],
        "business_date": config["business_date"],
        "seed": config["seed"],
        "injection_rate": rate,
        "generated_at": timestamp(datetime.now()),
        "datasets": {},
    }
    for dataset, rows in rows_by_dataset.items():
        file_path = output_dir / f"{dataset}.csv"
        write_csv(file_path, DATASETS[dataset], rows)
        injected = sum(injection_counts[dataset].values())
        manifest["datasets"][dataset] = {
            "file": file_path.name,
            "valid_rows": len(rows) - injected,
            "injected_rows": injected,
            "total_rows": len(rows),
            "sha256": sha256sum(file_path),
            "injections": dict(sorted(injection_counts[dataset].items())),
        }

    with (output_dir / "manifest.json").open("w", encoding="utf-8") as handle:
        json.dump(manifest, handle, ensure_ascii=False, indent=2)
        handle.write("\n")
    return manifest


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", type=Path, default=Path(__file__).with_name("generator_config.json"))
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--batch-id", help="Override config batch_id")
    parser.add_argument("--business-date", help="Override config business_date, YYYY-MM-DD")
    parser.add_argument("--seed", type=int, help="Override config seed")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    config = load_config(args.config)
    if args.batch_id:
        config["batch_id"] = args.batch_id
    if args.business_date:
        datetime.strptime(args.business_date, "%Y-%m-%d")
        config["business_date"] = args.business_date
    if args.seed is not None:
        config["seed"] = args.seed
    manifest = generate(config, args.output_dir)
    print(json.dumps({"batch_id": manifest["batch_id"], "datasets": manifest["datasets"]}, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
