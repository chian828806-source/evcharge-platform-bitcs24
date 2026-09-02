"""构造无未来泄漏的多站点时序特征和预测目标。"""

from __future__ import annotations

from collections.abc import Iterable
from zoneinfo import ZoneInfo

import numpy as np
import pandas as pd


HORIZONS = (1, 6, 24)
REQUIRED_HISTORY_COLUMNS = {
    "timestamp",
    "station_id",
    "station_load",
    "session_starts",
    "energy_kwh",
}


def load_history(path: str) -> pd.DataFrame:
    """读取小时历史并执行字段、范围和连续性校验。"""

    history = pd.read_csv(path)
    return validate_history(history)


def validate_history(history: pd.DataFrame) -> pd.DataFrame:
    """返回按站点和 UTC 时间排序的历史副本。"""

    missing = REQUIRED_HISTORY_COLUMNS.difference(history.columns)
    if missing:
        raise ValueError(f"history is missing columns: {sorted(missing)}")

    result = history.copy()
    result["timestamp"] = pd.to_datetime(result["timestamp"], utc=True, errors="raise")
    result["station_id"] = pd.to_numeric(
        result["station_id"], errors="raise"
    ).astype(int)
    numeric_columns = ["station_load", "session_starts", "energy_kwh"]
    for column in numeric_columns:
        result[column] = pd.to_numeric(result[column], errors="raise")
    if not result["station_load"].between(0, 1).all():
        raise ValueError("station_load must be between 0 and 1")
    if (result[["session_starts", "energy_kwh"]] < 0).any().any():
        raise ValueError("session_starts and energy_kwh must be non-negative")

    capacity_column = (
        "total_pile_count"
        if "total_pile_count" in result.columns
        else "source_port_count"
    )
    if capacity_column not in result.columns:
        raise ValueError("history requires total_pile_count or source_port_count")
    result["capacity"] = pd.to_numeric(
        result[capacity_column], errors="raise"
    ).astype(int)
    if (result["capacity"] <= 0).any():
        raise ValueError("station capacity must be positive")

    result = result.sort_values(["station_id", "timestamp"]).reset_index(drop=True)
    if result.duplicated(["station_id", "timestamp"]).any():
        raise ValueError("history contains duplicate station-hour rows")
    for station_id, group in result.groupby("station_id"):
        gaps = group["timestamp"].diff().dropna()
        if not (gaps == pd.Timedelta(hours=1)).all():
            raise ValueError(f"station {station_id} history is not continuous hourly data")
        if group["capacity"].nunique() != 1:
            raise ValueError(f"station {station_id} capacity changes inside one batch")
    return result


def station_feature_names(station_ids: Iterable[int]) -> list[str]:
    """按稳定顺序返回站点 one-hot 特征名。"""

    return [f"station_{station_id}" for station_id in sorted(set(station_ids))]


def model_feature_names(station_ids: Iterable[int]) -> list[str]:
    """返回训练与预测共同使用的完整特征顺序。"""

    return [
        "load_now",
        "load_24h_ago",
        "load_168h_ago",
        "mean_24",
        "std_24",
        "mean_168",
        "std_168",
        "starts_now",
        "energy_now",
        "hour_sin",
        "hour_cos",
        "dow_sin",
        "dow_cos",
        "month_sin",
        "month_cos",
        *station_feature_names(station_ids),
    ]


def _calendar_features(
    target_time: pd.Series, calendar_timezone: str
) -> pd.DataFrame:
    """从已知的目标时间生成周期日历特征。"""

    local = target_time.dt.tz_convert(ZoneInfo(calendar_timezone))
    return pd.DataFrame(
        {
            "hour_sin": np.sin(2 * np.pi * local.dt.hour / 24),
            "hour_cos": np.cos(2 * np.pi * local.dt.hour / 24),
            "dow_sin": np.sin(2 * np.pi * local.dt.dayofweek / 7),
            "dow_cos": np.cos(2 * np.pi * local.dt.dayofweek / 7),
            "month_sin": np.sin(2 * np.pi * (local.dt.month - 1) / 12),
            "month_cos": np.cos(2 * np.pi * (local.dt.month - 1) / 12),
        },
        index=target_time.index,
    )


def _base_features(
    group: pd.DataFrame,
    horizon: int,
    calendar_timezone: str,
) -> pd.DataFrame:
    """为一个站点构造只依赖预测时已知历史的基础特征。"""

    load = group["station_load"].astype(float)
    result = pd.DataFrame(
        {
            "origin_time": group["timestamp"],
            "target_time": group["timestamp"] + pd.to_timedelta(horizon, unit="h"),
            "station_id": group["station_id"].astype(int),
            "capacity": group["capacity"].astype(int),
            "load_now": load,
            "load_24h_ago": load.shift(24),
            "load_168h_ago": load.shift(168),
            "mean_24": load.rolling(24, min_periods=24).mean(),
            "std_24": load.rolling(24, min_periods=24).std().fillna(0),
            "mean_168": load.rolling(168, min_periods=168).mean(),
            "std_168": load.rolling(168, min_periods=168).std().fillna(0),
            "starts_now": group["session_starts"].astype(float),
            "energy_now": group["energy_kwh"].astype(float),
        }
    )
    calendar = _calendar_features(result["target_time"], calendar_timezone)
    return pd.concat([result, calendar], axis=1)


def build_supervised(
    history: pd.DataFrame,
    horizon: int,
    calendar_timezone: str,
    station_ids: Iterable[int] | None = None,
) -> pd.DataFrame:
    """构造未来窗口平均负荷目标和同周季节基线。"""

    if horizon not in HORIZONS:
        raise ValueError(f"unsupported horizon: {horizon}")
    source = validate_history(history)
    expected_stations = sorted(
        set(station_ids) if station_ids is not None else source["station_id"].unique()
    )
    parts: list[pd.DataFrame] = []
    for station_id, group in source.groupby("station_id", sort=True):
        group = group.reset_index(drop=True)
        load = group["station_load"].astype(float)
        frame = _base_features(group, horizon, calendar_timezone)

        # 目标是预测原点之后 horizon 个完整小时的平均负荷。
        future = pd.concat(
            [load.shift(-offset) for offset in range(1, horizon + 1)], axis=1
        )
        frame["target"] = future.mean(axis=1, skipna=False)

        # 同周基线使用与未来窗口逐小时对应的七天前观测。
        seasonal = pd.concat(
            [load.shift(168 - offset) for offset in range(1, horizon + 1)],
            axis=1,
        )
        frame["seasonal_prediction"] = seasonal.mean(axis=1, skipna=False)
        for expected in expected_stations:
            frame[f"station_{expected}"] = float(expected == station_id)
        parts.append(frame.dropna())

    result = pd.concat(parts, ignore_index=True)
    if result.empty:
        raise ValueError("history is too short to create lag_168 features")
    return result


def build_inference(
    history: pd.DataFrame,
    horizon: int,
    calendar_timezone: str,
    station_ids: Iterable[int],
) -> pd.DataFrame:
    """为每个站点最近一个完整小时构造一行预测特征。"""

    source = validate_history(history)
    expected_stations = sorted(set(station_ids))
    rows: list[pd.DataFrame] = []
    for station_id in expected_stations:
        group = source[source["station_id"] == station_id].reset_index(drop=True)
        if len(group) < 168:
            raise ValueError(f"station {station_id} requires at least 168 hours")
        frame = _base_features(group, horizon, calendar_timezone)
        row = frame.iloc[[-1]].copy()
        load = group["station_load"].astype(float)
        seasonal_values = [
            load.iloc[len(load) - 1 + offset - 168]
            for offset in range(1, horizon + 1)
        ]
        row["seasonal_prediction"] = float(np.mean(seasonal_values))
        for expected in expected_stations:
            row[f"station_{expected}"] = float(expected == station_id)
        rows.append(row)
    return pd.concat(rows, ignore_index=True)
