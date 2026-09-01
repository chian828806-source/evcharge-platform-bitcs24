-- ============================================================================
-- EVCharge 新能源汽车智能充电服务与运营平台
-- SQLite 正式建表脚本 (V1 基线，对齐 docs/04-DATABASE.md 新版契约)
--
-- 约定（docs/04-DATABASE.md 第 2 节）：
--   1. 金额一律以"分"为单位存 INTEGER；电量 kWh / 功率 kW / 比率用 REAL；
--   2. 时间统一存 TEXT，格式 'yyyy-MM-dd HH:mm:ss'（本地时间）；
--   3. 业务表统一含 created_at / updated_at 通用字段（2.3 节）；
--   4. 唯一性统一用第 6 节的 UNIQUE INDEX 表达，不在列上重复声明；
--   5. 活动订单互斥（BR-003/BR-004）由 Service 层事务保护（第 6 节说明）；
--   6. 本脚本可重复执行：先 DROP 再 CREATE，会清空全部业务数据。
-- ============================================================================

PRAGMA foreign_keys = ON;   -- SQLite 默认关闭外键，每次连接需重新开启

-- ----------------------------------------------------------------------------
-- 1. 用户表 user —— 车主账号、资料、钱包余额和状态（04 文档 5.1）
-- ----------------------------------------------------------------------------
DROP TABLE IF EXISTS user;
CREATE TABLE user (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,   -- 用户ID (对外 userId)
    phone         TEXT    NOT NULL,                    -- 11位手机号，登录凭证，唯一(idx_user_phone)
    nickname      TEXT    NOT NULL,                    -- 昵称 2~20 字符，自动注册默认"用户+后四位"
    avatar_path   TEXT,                                -- 头像相对路径，空 = 默认灰色头像
    balance_fen   INTEGER NOT NULL DEFAULT 0,          -- 钱包余额(分)，约束 >= 0
    status        TEXT    NOT NULL DEFAULT 'NORMAL',   -- NORMAL 正常 / FROZEN 冻结
    last_login_at TEXT,                                -- 最近登录时间(7.1 登录事务更新)
    created_at    TEXT    NOT NULL,                    -- 注册时间
    updated_at    TEXT    NOT NULL,                    -- 更新时间
    CHECK (balance_fen >= 0),
    CHECK (status IN ('NORMAL', 'FROZEN'))
);

-- ----------------------------------------------------------------------------
-- 2. 管理员表 admin —— 管理员账号（04 文档 5.2 / SEC-001）
--    默认账号 admin/123456，只存加盐哈希；Socket 响应禁止返回 password_hash
-- ----------------------------------------------------------------------------
DROP TABLE IF EXISTS admin;
CREATE TABLE admin (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,   -- 管理员ID (adminId)
    username      TEXT    NOT NULL,                    -- 登录名，唯一(idx_admin_username)
    password_hash TEXT    NOT NULL,                    -- 哈希，格式 'salt:hexdigest'，不存明文
    display_name  TEXT    NOT NULL,                    -- 展示名称
    status        TEXT    NOT NULL DEFAULT 'NORMAL',   -- NORMAL / FROZEN
    last_login_at TEXT,                                -- 最近登录时间
    created_at    TEXT    NOT NULL,
    updated_at    TEXT    NOT NULL,
    CHECK (status IN ('NORMAL', 'FROZEN'))
);

-- ----------------------------------------------------------------------------
-- 3. 充电站表 charging_station —— 站点信息、地理坐标、默认电价（04 文档 5.3）
--    新增站点时由服务层按输入数量自动生成电桩(FR-A-009)
-- ----------------------------------------------------------------------------
DROP TABLE IF EXISTS charging_station;
CREATE TABLE charging_station (
    id                      INTEGER PRIMARY KEY AUTOINCREMENT,  -- 站点ID (stationId)
    station_no              TEXT    NOT NULL,          -- 站点编号，唯一(idx_station_no)
    name                    TEXT    NOT NULL,          -- 站点名称
    address                 TEXT    NOT NULL,          -- 详细地址
    district                TEXT,                      -- 区域/行政区(用户端下拉定位, FR-U-006)
    longitude               REAL    NOT NULL,          -- 经度(腾讯地图 GCJ-02)
    latitude                REAL    NOT NULL,          -- 纬度
    price_fen_per_kwh       INTEGER NOT NULL,          -- 默认电价，分/kWh
    service_fee_fen_per_kwh INTEGER NOT NULL DEFAULT 0,-- 默认服务费，分/kWh
    status                  TEXT    NOT NULL DEFAULT 'NORMAL', -- NORMAL / DISABLED 停用
    created_at              TEXT    NOT NULL,
    updated_at              TEXT    NOT NULL,
    CHECK (price_fen_per_kwh >= 0),
    CHECK (status IN ('NORMAL', 'DISABLED'))
);

-- ----------------------------------------------------------------------------
-- 4. 充电桩表 charging_pile —— 桩信息、实时状态、累计统计（04 文档 5.4）
--    状态机(3.3)：AVAILABLE -> RESERVED -> CHARGING -> AVAILABLE
--                 AVAILABLE -> RESERVED -> AVAILABLE (订单取消, BR-005)
--    current_order_id：占用指针，仅 RESERVED/CHARGING 时填写，释放后置空；
--    不加外键以避免与 charging_order 相互引用，一致性由 Service 层事务维护
-- ----------------------------------------------------------------------------
DROP TABLE IF EXISTS charging_pile;
CREATE TABLE charging_pile (
    id                   INTEGER PRIMARY KEY AUTOINCREMENT,  -- 电桩ID (pileId)
    station_id           INTEGER NOT NULL REFERENCES charging_station(id),  -- 所属站点
    pile_no              TEXT    NOT NULL,          -- 桩编号，站内唯一(idx_pile_station_no)
    type                 TEXT    NOT NULL,          -- FAST 快充 / SLOW 慢充
    power_kw             REAL    NOT NULL,          -- 额定功率 kW
    status               TEXT    NOT NULL DEFAULT 'AVAILABLE',  -- 6 态见 04 文档 3.3
    current_order_id     INTEGER,                   -- 当前占用订单ID(占用时填写)
    total_charge_count   INTEGER NOT NULL DEFAULT 0,-- 累计完成充电次数(结算时累加, 7.6)
    total_charge_minutes INTEGER NOT NULL DEFAULT 0,-- 累计充电分钟数
    total_energy_kwh     REAL    NOT NULL DEFAULT 0,-- 累计充电量 kWh
    last_heartbeat_at    TEXT,                      -- 扩展设备心跳时间(OPTIONAL)
    created_at           TEXT    NOT NULL,
    updated_at           TEXT    NOT NULL,
    CHECK (type IN ('FAST', 'SLOW')),
    CHECK (status IN ('AVAILABLE', 'RESERVED', 'CHARGING', 'FAULT', 'OFFLINE', 'RESTARTING'))
);

-- ----------------------------------------------------------------------------
-- 5. 充电订单表 charging_order —— 预约/充电/停止/结算/取消全过程（04 文档 5.5）
--    状态机(3.4)：CREATED -> CHARGING -> PENDING_PAYMENT -> COMPLETED
--                 CREATED -> CANCELLED
--                 PENDING_PAYMENT -> PENDING_PAYMENT (余额不足，不重复扣款)
--    price/service_fee 为下单时快照，防止后续调价影响历史订单；
--    占用规则：CREATED/CHARGING 占用电桩；其余状态不占用
-- ----------------------------------------------------------------------------
DROP TABLE IF EXISTS charging_order;
CREATE TABLE charging_order (
    id                      INTEGER PRIMARY KEY AUTOINCREMENT,  -- 订单ID (orderId)
    order_no                TEXT    NOT NULL,          -- 订单编号，唯一(idx_order_no)
    user_id                 INTEGER NOT NULL REFERENCES user(id),
    station_id              INTEGER NOT NULL REFERENCES charging_station(id),  -- 冗余站点，便于统计
    pile_id                 INTEGER NOT NULL REFERENCES charging_pile(id),
    status                  TEXT    NOT NULL DEFAULT 'CREATED',  -- 5 态见 04 文档 3.4
    price_fen_per_kwh       INTEGER NOT NULL,          -- 下单时电价快照 分/kWh
    service_fee_fen_per_kwh INTEGER NOT NULL DEFAULT 0,-- 下单时服务费快照 分/kWh
    start_at                TEXT,                      -- 开始充电时间(7.4 写入)
    end_at                  TEXT,                      -- 停止充电时间(7.5 写入)
    charge_minutes          INTEGER NOT NULL DEFAULT 0,-- 充电分钟数
    energy_kwh              REAL    NOT NULL DEFAULT 0,-- 充电量 kWh
    amount_fen              INTEGER NOT NULL DEFAULT 0,-- 应付金额(分)，计费公式见 7.5
    paid_at                 TEXT,                      -- 结算时间(7.6 写入，营收统计口径)
    cancelled_at            TEXT,                      -- 取消时间(7.3 写入)
    cancel_reason           TEXT,                      -- 取消原因
    created_at              TEXT    NOT NULL,
    updated_at              TEXT    NOT NULL,
    CHECK (status IN ('CREATED', 'CHARGING', 'PENDING_PAYMENT', 'COMPLETED', 'CANCELLED')),
    CHECK (energy_kwh >= 0),
    CHECK (amount_fen >= 0),
    CHECK (charge_minutes >= 0)
);

-- ----------------------------------------------------------------------------
-- 6. 充值流水表 recharge_record —— 钱包充值流水（04 文档 5.6）
--    充值成功时 user.balance_fen 与本表必须在同一事务写入(7.7)
-- ----------------------------------------------------------------------------
DROP TABLE IF EXISTS recharge_record;
CREATE TABLE recharge_record (
    id                INTEGER PRIMARY KEY AUTOINCREMENT,  -- 充值ID (rechargeId)
    record_no         TEXT    NOT NULL,          -- 流水号，唯一(idx_recharge_no)
    user_id           INTEGER NOT NULL REFERENCES user(id),
    amount_fen        INTEGER NOT NULL,          -- 充值金额(分)，约束 > 0
    balance_after_fen INTEGER NOT NULL,          -- 充值后余额快照(分)，对账用
    status            TEXT    NOT NULL DEFAULT 'SUCCESS', -- SUCCESS / FAILED
    remark            TEXT,                      -- 备注
    created_at        TEXT    NOT NULL,
    CHECK (amount_fen > 0),
    CHECK (status IN ('SUCCESS', 'FAILED'))
);

-- ----------------------------------------------------------------------------
-- 7. 预测结果表 prediction —— ML 输出，供推荐/预警/大屏（04 文档 5.7）
--    负荷统一口径(8.6)：stationLoad = chargingPileMinutes / (totalPileCount*windowMinutes)
--    只统计 CHARGING 占用时长；predicted_load 取值 0~1
-- ----------------------------------------------------------------------------
DROP TABLE IF EXISTS prediction;
CREATE TABLE prediction (
    id                        INTEGER PRIMARY KEY AUTOINCREMENT,  -- 预测ID (predictionId)
    station_id                INTEGER NOT NULL REFERENCES charging_station(id),
    prediction_time           TEXT    NOT NULL,       -- 被预测的目标时间点
    horizon                   TEXT    NOT NULL,       -- 预测窗口：1h / 6h / 24h
    predicted_load            REAL    NOT NULL,       -- 站点负荷 0~1
    predicted_available_count INTEGER,                -- 预测空闲桩数(FR-ML-003, 非负)
    peak_level                TEXT,                   -- 高峰等级：LOW / MEDIUM / HIGH
    model_name                TEXT,                   -- 模型名称
    mae                       REAL,                   -- 可选评价指标(FR-ML-002 验收)
    rmse                      REAL,                   -- 可选评价指标
    generated_at              TEXT    NOT NULL,       -- 预测生成时间
    created_at                TEXT    NOT NULL,       -- 入库时间
    CHECK (predicted_load >= 0 AND predicted_load <= 1),
    CHECK (horizon IN ('1h', '6h', '24h')),
    CHECK (peak_level IN ('LOW', 'MEDIUM', 'HIGH'))
);

-- ----------------------------------------------------------------------------
-- 8. 操作日志表 operation_log —— 管理员操作/远程重启/冻结解冻留痕（04 文档 5.8）
--    远程重启必须记录管理员、电桩、操作前后状态和结果消息(FR-IOT-001)
-- ----------------------------------------------------------------------------
DROP TABLE IF EXISTS operation_log;
CREATE TABLE operation_log (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,  -- 日志ID (logId)
    admin_id      INTEGER REFERENCES admin(id),      -- 操作管理员，系统动作可为空
    action        TEXT    NOT NULL,                   -- ADMIN_LOGIN / USER_FREEZE / USER_UNFREEZE /
                                                      --   STATION_CREATE / PILE_RESTART /
                                                      --   PILE_STATUS_CHANGE / ORDER_STATE_CHANGE /
                                                      --   SYSTEM_ERROR
    target_type   TEXT    NOT NULL,                   -- USER / STATION / PILE / ORDER / SYSTEM
    target_id     INTEGER,                            -- 目标ID
    before_status TEXT,                               -- 操作前状态
    after_status  TEXT,                               -- 操作后状态
    result        TEXT    NOT NULL DEFAULT 'SUCCESS', -- SUCCESS / FAILED
    message       TEXT,                               -- 结果说明
    created_at    TEXT    NOT NULL,
    CHECK (result IN ('SUCCESS', 'FAILED'))
);

-- ============================================================================
-- 索引（04 文档第 6 节：唯一性约束 + 高频查询覆盖）
-- ============================================================================
CREATE UNIQUE INDEX idx_user_phone           ON user(phone);
CREATE UNIQUE INDEX idx_admin_username       ON admin(username);
CREATE UNIQUE INDEX idx_station_no           ON charging_station(station_no);
CREATE UNIQUE INDEX idx_pile_station_no      ON charging_pile(station_id, pile_no);
CREATE UNIQUE INDEX idx_order_no             ON charging_order(order_no);
CREATE UNIQUE INDEX idx_recharge_no          ON recharge_record(record_no);

CREATE INDEX idx_station_district        ON charging_station(district);
CREATE INDEX idx_pile_station_status     ON charging_pile(station_id, status);
CREATE INDEX idx_order_user_status       ON charging_order(user_id, status);      -- 活动订单检查 BR-003
CREATE INDEX idx_order_pile_status       ON charging_order(pile_id, status);      -- 单桩互斥 BR-004
CREATE INDEX idx_order_station_created   ON charging_order(station_id, created_at);
CREATE INDEX idx_order_status_created    ON charging_order(status, created_at);   -- 营收/趋势统计
CREATE INDEX idx_recharge_user_created   ON recharge_record(user_id, created_at);
CREATE INDEX idx_prediction_station_time ON prediction(station_id, prediction_time);
CREATE INDEX idx_operation_target_created ON operation_log(target_type, target_id, created_at);
