-- 旧数据库增量升级：不会删除或重建现有业务表与数据。
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS user_station_favorite (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id    INTEGER NOT NULL REFERENCES user(id),
    station_id INTEGER NOT NULL REFERENCES charging_station(id),
    created_at TEXT    NOT NULL,
    UNIQUE(user_id, station_id)
);

CREATE INDEX IF NOT EXISTS idx_favorite_user_created
    ON user_station_favorite(user_id, created_at DESC);
