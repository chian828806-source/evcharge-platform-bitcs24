"""Validate the official UrbanEV station-Raw to EVCharge Raw adapter."""

from __future__ import annotations

import csv
import hashlib
import importlib.util
import io
import tempfile
import unittest
from contextlib import redirect_stdout
from datetime import datetime, timedelta
from pathlib import Path
from types import SimpleNamespace


MODULE_PATH = Path(__file__).resolve().parents[1] / "urbanev" / "prepare_raw.py"
SPEC = importlib.util.spec_from_file_location("prepare_urbanev_raw", MODULE_PATH)
prepare_urbanev_raw = importlib.util.module_from_spec(SPEC)
assert SPEC and SPEC.loader
SPEC.loader.exec_module(prepare_urbanev_raw)


def write_csv(path: Path, headers: list[str], rows: list[dict]) -> None:
    """Write a small fixture that follows the documented official header names."""
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=headers)
        writer.writeheader()
        writer.writerows(rows)


class UrbanEvAdapterTest(unittest.TestCase):
    """Use deterministic hand-written source rows; never call the random generator."""

    def build_fixture(self, root: Path) -> Path:
        station_root = root / "UrbanEVDataset" / "20220901-20230228_station-raw"
        write_csv(
            station_root / "station_information.csv",
            ["station_id", "longitude", "latitude", "slow_count", "fast_count", "charge_count", "TAZID"],
            [{"station_id": 1001, "longitude": 114.05, "latitude": 22.55,
              "slow_count": 4, "fast_count": 6, "charge_count": 10, "TAZID": 88}],
        )
        write_csv(
            station_root / "pile_rated_power.csv",
            ["pileNo", "power", "pileType", "station_id"],
            [
                {"pileNo": "P-DC", "power": 60, "pileType": "DC", "station_id": 1001},
                {"pileNo": "P-AC", "power": 7, "pileType": "AC", "station_id": 1001},
            ],
        )
        start = datetime(2022, 9, 1)
        samples = []
        for index in range(12):
            busy = 2 if index < 6 else 3
            samples.append({
                "time": (start + timedelta(minutes=index * 5)).strftime("%Y-%m-%d %H:%M"),
                "busy": busy, "idle": 10 - busy, "s_price": 0.4, "e_price": 0.8,
                "fast_busy": busy, "fast_idle": 6 - busy,
                "slow_busy": 0, "slow_idle": 4,
                "duration": busy / 12, "volume": 1.0,
            })
        # Nine samples are deliberately incomplete; the adapter must keep an auditable rejected hour.
        for index in range(9):
            samples.append({
                "time": (start + timedelta(hours=1, minutes=index * 5)).strftime("%Y-%m-%d %H:%M"),
                "busy": 1, "idle": 9, "s_price": 0.4, "e_price": 0.8,
                "fast_busy": 1, "fast_idle": 5,
                "slow_busy": 0, "slow_idle": 4,
                "duration": 1 / 12, "volume": 0.5,
            })
        write_csv(
            station_root / "charge_5min" / "1001.csv",
            ["time", "busy", "idle", "s_price", "e_price", "fast_busy", "fast_idle",
             "slow_busy", "slow_idle", "duration", "volume"],
            samples,
        )
        return root / "UrbanEVDataset"

    def test_official_raw_keeps_metrics_and_adds_seeded_business_rows(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = self.build_fixture(root)
            output = root / "output"
            with redirect_stdout(io.StringIO()):
                manifest = prepare_urbanev_raw.prepare(SimpleNamespace(
                    dataset_root=source,
                    output_dir=output,
                    batch_id="URBANEV-TEST",
                    business_date="2023-02-28",
                    max_stations=0,
                    min_samples_per_hour=10,
                    synthetic_users=3,
                    synthetic_seed=24,
                ))

            self.assertEqual(manifest["source_mode"], "official-station-raw-5min")
            self.assertEqual(manifest["seed"], 24)
            self.assertTrue(manifest["synthetic_dimensions"]["enabled"])
            self.assertEqual(manifest["adapter_report"]["accepted_hour_rows"], 1)
            self.assertEqual(manifest["adapter_report"]["rejected_hour_rows"], 1)
            self.assertEqual(manifest["datasets"]["users"]["rows"], 3)
            self.assertEqual(manifest["datasets"]["orders"]["rows"], 1)
            self.assertEqual(manifest["datasets"]["sessions"]["rows"], 1)

            with (output / "station_hourly_metrics.csv").open(encoding="utf-8") as handle:
                metrics = list(csv.DictReader(handle))
            self.assertEqual(len(metrics), 2)
            self.assertEqual(metrics[0]["source_type"], "URBANEV")
            self.assertEqual(float(metrics[0]["energy_kwh"]), 12.0)
            self.assertAlmostEqual(float(metrics[0]["average_occupied_count"]), 2.5)
            self.assertAlmostEqual(float(metrics[0]["station_load"]), 0.25)
            self.assertEqual(int(metrics[0]["session_starts"]), 1)
            self.assertEqual(metrics[1]["station_load"], "")

            with (output / "orders.csv").open(encoding="utf-8") as handle:
                orders = list(csv.DictReader(handle))
            with (output / "users.csv").open(encoding="utf-8") as handle:
                users = list(csv.DictReader(handle))
            self.assertTrue(orders[0]["order_no"].startswith("URBANEV-SYN-"))
            self.assertIn(orders[0]["user_id"], {row["user_id"] for row in users})
            self.assertAlmostEqual(sum(float(row["energy_kwh"]) for row in orders), 12.0)
            with (output / "sessions.csv").open(encoding="utf-8") as handle:
                sessions = list(csv.DictReader(handle))
            self.assertEqual(sum(int(row["duration_seconds"]) for row in sessions), 9000)

            with (output / "piles.csv").open(encoding="utf-8") as handle:
                piles = list(csv.DictReader(handle))
            self.assertEqual([row["type"] for row in piles], ["SLOW", "FAST"])

    def test_same_seed_reproduces_every_business_csv(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = self.build_fixture(root)
            digests = []
            for run in ("first", "second"):
                output = root / run
                with redirect_stdout(io.StringIO()):
                    prepare_urbanev_raw.prepare(SimpleNamespace(
                        dataset_root=source,
                        output_dir=output,
                        batch_id="URBANEV-REPRODUCIBLE",
                        business_date="2023-02-28",
                        max_stations=0,
                        min_samples_per_hour=10,
                        synthetic_users=5,
                        synthetic_seed=20260916,
                    ))
                digests.append({
                    name: hashlib.sha256((output / f"{name}.csv").read_bytes()).hexdigest()
                    for name in ("users", "stations", "piles", "orders", "sessions", "station_hourly_metrics")
                })
            self.assertEqual(digests[0], digests[1])

    def test_zone_aggregate_is_not_accepted_as_raw(self):
        with tempfile.TemporaryDirectory() as directory:
            zone = Path(directory) / "20220901-20230228_zone-cleaned-aggregated" / "charge_5min"
            zone.mkdir(parents=True)
            with self.assertRaisesRegex(FileNotFoundError, "zone-cleaned"):
                prepare_urbanev_raw.find_station_layout(Path(directory))

    def test_station_without_pile_rows_is_skipped_before_limit_is_applied(self):
        """An incomplete high-capacity station must not displace a complete station."""
        with tempfile.TemporaryDirectory() as directory:
            charge_dir = Path(directory) / "charge_5min"
            charge_dir.mkdir()
            for station_id in ("1001", "1002"):
                (charge_dir / f"{station_id}.csv").touch()
            stations = {
                "1001": {"capacity": 100},
                "1002": {"capacity": 20},
            }
            selected = prepare_urbanev_raw.choose_stations(
                stations, charge_dir, {"1002"}, limit=1
            )
            self.assertEqual(selected, ["1002"])


if __name__ == "__main__":
    unittest.main()
