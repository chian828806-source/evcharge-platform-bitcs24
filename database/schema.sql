-- ============================================================================
-- EVCharge 新能源汽车智能充电服务与运营平台
-- SQLite 正式建表脚本 (V1 基线)
--
-- 依据文档：
--   docs/00-SRS-V1.0.md   (第 6 节 数据需求 / BR-001~BR-006 业务规则)
--   docs/04-DATABASE.md   (字段类型 / 命名 / 状态枚举 / 索引规范)
--
-- 约定：
--   1. 金额一律以"分"为单位存 INTEGER，界面展示时再转为元；
--   2. 时间统一存 TEXT，格式 'yyyy-MM-dd HH:mm:ss'（本地时间）；
--   3. 状态字段一律用 docs/04-DATABASE.md 第 7 节定义的枚举字符串；
--   4. 所有表名与字段名为 snake_case，
--      Qt/C++ 属性与 Socket/WebSocket 消息字段映射为 camelCase
--      (如 price_fen_per_kwh -> priceFenPerKwh, created_at -> createdAt)；
--   5. 本脚本可重复执行：先 DROP 再 CREATE，
--      注意会清空 evcharge.db 中全部业务数据。
-- ============================================================================

PRAGMA foreign_keys = ON;   -- SQLite 默认关闭外键约束，每次连接需重新开启

-- ----------------------------------------------------------------------------
-- 1. 用户表 user
--    对应需求 FR-U-001~005 (手机号免密登录/自动注册、资料维护、头像、钱包)
--    对应规则 BR-001 手机号唯一
-- ----------------------------------------------------------------------------
DROP TABLE IF EXISTS user;
CREATE TABLE user (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,  -- 用户ID
    phone           TEXT    NOT NULL UNIQUE,            -- 手机号(11位)，登录凭证，BR-001 唯一
    nickname        TEXT    NOT NULL,                   -- 昵称，自动注册时默认"用户+手机号后4位"
    avatar_path     TEXT,                               -- 头像文件相对路径，空则使用默认灰色头像
    balance_fen     INTEGER NOT NULL DEFAULT 0,         -- 钱包余额，单位：分
    status          TEXT    NOT NULL DEFAULT 'NORMAL',  -- 用户状态：NORMAL正常 / FROZEN冻结
    created_at      TEXT    NOT NULL                    -- 注册时间 yyyy-MM-dd HH:mm:ss
);

-- ----------------------------------------------------------------------------
-- 2. 管理员表 admin
--    对应需求 FR-A-001 (管理员登录，默认账号 admin/123456)
--    安全要求：禁止明文存储，使用加盐 SHA-256，格式 'salt:hexdigest'
-- ----------------------------------------------------------------------------
DROP TABLE IF EXISTS admin;
CREATE TABLE admin (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,  -- 管理员ID
    username        TEXT    NOT NULL UNIQUE,            -- 登录账号
    password_hash   TEXT    NOT NULL,                   -- 盐值:SHA-256哈希，格式 'evcharge:<hexdigest>'
    created_at      TEXT    NOT NULL                    -- 创建时间 yyyy-MM-dd HH:mm:ss
);

-- ----------------------------------------------------------------------------
-- 3. 充电站表 charging_station
--    对应需求 FR-U-006~008 (附近站点查询、距离排序)、FR-A-007~009 (站点管理)
--    经纬度用于：客户端按距离排序、腾讯地图导航、ML 负荷预测聚合维度
-- ----------------------------------------------------------------------------
DROP TABLE IF EXISTS charging_station;
CREATE TABLE charging_station (
    id                  INTEGER PRIMARY KEY AUTOINCREMENT,  -- 站点ID
    name                TEXT    NOT NULL,                   -- 站名
    address             TEXT    NOT NULL,                   -- 详细地址
    longitude           REAL    NOT NULL,                   -- 经度（腾讯地图 GCJ-02 坐标）
    latitude            REAL    NOT NULL,                   -- 纬度
    price_fen_per_kwh   INTEGER NOT NULL,                   -- 充电单价，单位：分/度
    status              TEXT    NOT NULL DEFAULT 'OPEN',    -- 站点状态：OPEN营业 / CLOSED关闭(扩展用)
    created_at          TEXT    NOT NULL                    -- 创建时间 yyyy-MM-dd HH:mm:ss
);

-- ----------------------------------------------------------------------------
-- 4. 充电桩表 charging_pile
--    对应需求 FR-U-009~010 (桩详情)、FR-A-004~006 (状态统计/列表/远程重启)
--    电桩 6 态：AVAILABLE空闲 / RESERVED已预约 / CHARGING充电中 /
--              FAULT故障 / OFFLINE离线 / RESTARTING重启中
--    状态流转（SRS BR-005）：
--      AVAILABLE -> RESERVED -> CHARGING -> AVAILABLE
--      AVAILABLE -> RESERVED -> AVAILABLE (订单取消)
-- ----------------------------------------------------------------------------
DROP TABLE IF EXISTS charging_pile;
CREATE TABLE charging_pile (
    id                  INTEGER PRIMARY KEY AUTOINCREMENT,  -- 电桩ID
    station_id          INTEGER NOT NULL REFERENCES charging_station(id),  -- 所属站点
    pile_no             TEXT    NOT NULL UNIQUE,            -- 电桩编号，全局唯一(如 ZT01-P01)
    pile_type           TEXT    NOT NULL DEFAULT 'FAST',    -- 类型：FAST快充 / SLOW慢充
    power_kw            REAL    NOT NULL,                   -- 额定功率 kW
    status              TEXT    NOT NULL DEFAULT 'AVAILABLE',  -- 电桩状态(枚举见上)
    total_charge_count  INTEGER NOT NULL DEFAULT 0,         -- 累计充电次数(管理端列表展示)
    total_charge_seconds INTEGER NOT NULL DEFAULT 0,        -- 累计充电时长，单位：秒
    created_at          TEXT    NOT NULL                    -- 创建时间 yyyy-MM-dd HH:mm:ss
);

-- ----------------------------------------------------------------------------
-- 5. 充电订单表 charging_order
--    对应需求 FR-C-001~008 (充电全流程)、FR-A-002 (营收统计)
--    订单 5 态：CREATED已创建 / CHARGING充电中 / PENDING_PAYMENT待结算 /
--              COMPLETED已完成 / CANCELLED已取消
--    状态转换（SRS BR-005）：
--      CREATED -> CHARGING -> PENDING_PAYMENT -> COMPLETED
--      CREATED -> CANCELLED
--      PENDING_PAYMENT -> PENDING_PAYMENT (余额不足保持原态，禁止重复扣款)
--    活动订单定义 FR-C-001：状态 ∈ {CREATED, CHARGING, PENDING_PAYMENT}
-- ----------------------------------------------------------------------------
DROP TABLE IF EXISTS charging_order;
CREATE TABLE charging_order (
    id                  INTEGER PRIMARY KEY AUTOINCREMENT,  -- 订单ID
    order_no            TEXT    NOT NULL UNIQUE,            -- 订单号，业务唯一标识(如 OD20260901120000001)
    user_id             INTEGER NOT NULL REFERENCES user(id),              -- 下单用户
    pile_id             INTEGER NOT NULL REFERENCES charging_pile(id),     -- 选定的电桩
    station_id          INTEGER NOT NULL REFERENCES charging_station(id),  -- 冗余站点ID，便于统计
    status              TEXT    NOT NULL DEFAULT 'CREATED', -- 订单状态(枚举见上)
    price_fen_per_kwh   INTEGER NOT NULL,                   -- 下单时冻结的单价 分/度，防止后续调价影响历史订单
    start_time          TEXT,                               -- 开始充电时间，CREATED阶段为空
    end_time            TEXT,                               -- 结束充电时间，停止充电时写入
    duration_seconds    INTEGER,                            -- 充电时长(秒) = end_time - start_time
    energy_kwh          REAL,                               -- 充电电量(度)，模拟值 = 功率 × 时长
    amount_fen          INTEGER,                            -- 应收金额(分) = energy_kwh × price_fen_per_kwh
    created_at          TEXT    NOT NULL,                   -- 下单时间 yyyy-MM-dd HH:mm:ss
    settled_at          TEXT                                -- 结算完成时间，COMPLETED时写入；防止重复结算依据之一
);

-- ----------------------------------------------------------------------------
-- 6. 钱包充值记录表 recharge_record
--    对应需求 FR-U-005 (模拟支付充值，余额实时更新)
--    充电流水用途：用户充值历史展示、对账
-- ----------------------------------------------------------------------------
DROP TABLE IF EXISTS recharge_record;
CREATE TABLE recharge_record (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,  -- 流水ID
    user_id         INTEGER NOT NULL REFERENCES user(id),   -- 充值用户
    amount_fen      INTEGER NOT NULL,                   -- 本次充值金额，单位：分
    balance_after_fen INTEGER NOT NULL,                 -- 充值后余额快照(分)，便于对账
    created_at      TEXT    NOT NULL                    -- 充值时间 yyyy-MM-dd HH:mm:ss
);

-- ----------------------------------------------------------------------------
-- 7. 预测结果表 prediction
--    对应需求 FR-ML-002~004、FR-D-003 (大屏展示预测)、FR-ML-005 (用户推荐)
--    负荷统一口径（SRS FR-ML-002，ML/大屏/推荐必须一致）：
--      stationLoad = chargingPileMinutes / (totalPileCount × windowMinutes)
--    predicted_load 取值 0~1 的 REAL，展示时可乘 100 转百分比
-- ----------------------------------------------------------------------------
DROP TABLE IF EXISTS prediction;
CREATE TABLE prediction (
    id                      INTEGER PRIMARY KEY AUTOINCREMENT,  -- 预测记录ID
    station_id              INTEGER NOT NULL REFERENCES charging_station(id),  -- 预测目标站点
    horizon                 TEXT    NOT NULL,       -- 预测窗口：1H / 6H / 24H
    prediction_time         TEXT    NOT NULL,       -- 预测目标时间点 yyyy-MM-dd HH:mm:ss
    predicted_load          REAL,                   -- 预测站点负荷 0~1（口径见上）
    predicted_available_count INTEGER,              -- 预测空闲桩数量，非负整数（FR-ML-003）
    peak_level              INTEGER,                -- 高峰等级 0~3：0低谷/1平段/2高峰/3尖峰（FR-ML-004）
    created_at              TEXT    NOT NULL        -- 本条预测生成时间 yyyy-MM-dd HH:mm:ss
);

-- ----------------------------------------------------------------------------
-- 8. 操作日志表 operation_log
--    对应需求 FR-IOT-001 (远程重启指令/结果必须留痕)、管理员操作审计
-- ----------------------------------------------------------------------------
DROP TABLE IF EXISTS operation_log;
CREATE TABLE operation_log (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,  -- 日志ID
    admin_id        INTEGER REFERENCES admin(id),       -- 操作管理员，系统自动操作时为空
    op_type         TEXT    NOT NULL,                   -- 操作类型：ADMIN_LOGIN / PILE_RESTART /
                                                        --   USER_FREEZE / USER_UNFREEZE / STATION_CREATE 等
    target_type     TEXT,                               -- 操作对象类型：PILE / USER / STATION / SYSTEM
    target_id       INTEGER,                            -- 操作对象ID
    detail          TEXT,                               -- 结果描述，如"重启成功，电桩状态 RESTARTING->AVAILABLE"
    created_at      TEXT    NOT NULL                    -- 操作时间 yyyy-MM-dd HH:mm:ss
);

-- ============================================================================
-- 索引（docs/04-DATABASE.md 第 9 节推荐）
-- 覆盖最高频查询：登录、活动订单检查、单桩互斥、站点空闲数、大屏预测
-- ============================================================================
CREATE INDEX idx_user_phone              ON user(phone);
CREATE INDEX idx_order_user_status       ON charging_order(user_id, status);
CREATE INDEX idx_order_pile_status       ON charging_order(pile_id, status);
CREATE INDEX idx_pile_station_status     ON charging_pile(station_id, status);
CREATE INDEX idx_prediction_station_time ON prediction(station_id, prediction_time);

-- 补充索引：营收/趋势统计按完成时间过滤 COMPLETED 订单（FR-A-002/003、FR-D-001）
CREATE INDEX idx_order_status_settled    ON charging_order(status, settled_at);
