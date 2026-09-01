# SQLite 数据库规范

本文档定义 SQLite、QtSql 和 `QSQLITE` 的使用规范。

## 1. 基本原则

SQLite 是 V1 主业务数据库。PC 服务与管理端通过 QtSql 的 `QSQLITE` 驱动访问数据库。

用户端不得直接访问 SQLite。

## 2. 数据库文件

推荐路径：

```text
database/evcharge.db
```

正式建表脚本：

```text
database/schema.sql
```

演示数据脚本：

```text
database/init_data.sql
```

## 3. 核心表

V1 至少包含：

- `user`
- `admin`
- `charging_station`
- `charging_pile`
- `charging_order`
- `recharge_record`
- `prediction`
- `operation_log`

## 4. 字段类型

SQLite 推荐类型：

| 数据 | 类型 |
| --- | --- |
| 主键 | `INTEGER PRIMARY KEY AUTOINCREMENT` |
| 字符串 | `TEXT` |
| 金额 | `INTEGER`，单位为分 |
| 电量 | `REAL` |
| 时间 | `TEXT`，格式 `yyyy-MM-dd HH:mm:ss` |
| 状态 | `TEXT` |

## 5. 命名规范

表名和字段名使用 snake_case。

Qt/C++ 属性和 JSON 字段使用 camelCase。
Socket / WebSocket 消息字段使用 camelCase。

示例：

```text
price_fen_per_kwh -> priceFenPerKwh
created_at -> createdAt
```

## 6. 金额规范

所有金额按分保存：

```sql
balance_fen INTEGER NOT NULL DEFAULT 0
amount_fen INTEGER NOT NULL DEFAULT 0
price_fen_per_kwh INTEGER NOT NULL
```

界面展示时转换为元。

## 7. 状态枚举

用户状态：

```text
NORMAL
FROZEN
```

电桩状态：

```text
AVAILABLE
RESERVED
CHARGING
FAULT
OFFLINE
RESTARTING
```

订单状态：

```text
CREATED
CHARGING
PENDING_PAYMENT
COMPLETED
CANCELLED
```

## 8. 关键约束

必须保证：

- `user.phone` 唯一；
- `admin.username` 唯一；
- `charging_pile.pile_no` 唯一或在站点内唯一；
- 同一用户不能同时存在多个活动订单；
- 同一电桩不能同时被多个 `CREATED` 或 `CHARGING` 订单占用。
- `CREATED` 订单对应电桩状态应为 `RESERVED`。

SQLite 对复杂条件唯一索引支持有限，实现时可由 Service 层加事务和查询保护。

## 9. 索引建议

```sql
CREATE INDEX idx_user_phone ON user(phone);
CREATE INDEX idx_order_user_status ON charging_order(user_id, status);
CREATE INDEX idx_order_pile_status ON charging_order(pile_id, status);
CREATE INDEX idx_pile_station_status ON charging_pile(station_id, status);
CREATE INDEX idx_prediction_station_time ON prediction(station_id, prediction_time);
```

## 10. Qt 查询示例

```cpp
QSqlQuery query(db);
query.prepare("SELECT id, phone, nickname FROM user WHERE phone = :phone");
query.bindValue(":phone", phone);
query.exec();
```

必须使用参数绑定，禁止拼接用户输入 SQL。

## 11. 多线程访问

规则：

- 每个线程使用独立连接名；
- 不跨线程共享 `QSqlDatabase`；
- 写操作集中处理；
- 数据库操作失败时记录 `lastError()`。

## 12. 预测结果

`prediction` 至少支持：

```text
station_id
prediction_time
horizon
predicted_load
predicted_available_count
peak_level
created_at
```

`predicted_load` 使用统一站点负荷口径：

```text
stationLoad = chargingPileMinutes / (totalPileCount × windowMinutes)
```

字段值保存为 0 到 1 的 `REAL`。

## 13. 种子数据

`init_data.sql` 应包含默认管理员、示例用户、示例站点、示例电桩、历史订单和预测结果。

默认管理员账号为 `admin / 123456`，密码字段不得存明文。

## 14. 变更流程

表结构变更必须同步修改：

- `database/schema.sql`
- `database/init_data.sql`
- `docs/04-DATABASE.md`
- 对应 Repository / Model
- `docs/03-API.md`
