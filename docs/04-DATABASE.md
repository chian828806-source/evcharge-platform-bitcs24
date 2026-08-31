# 数据库规范

本文档定义数据库命名、核心表、事务、迁移、种子数据和变更流程。正式字段以 `database/schema.sql` 为准。

## 1. 基本原则

主业务数据库建议采用 MySQL 8.x。

所有业务模块共享同一主数据库。

Qt 用户端、Qt 管理端和 Web 大屏不得直接修改主业务数据库。Qt 双客户端必须通过服务端 TCP Socket 业务协议访问数据，Web 大屏通过 REST API 访问数据。

任务书中的 QSQLite 是否为硬性要求仍需确认。若老师要求体现 SQLite，建议仅用于 Qt 本地缓存或配置存储。

## 2. 核心数据对象

V1 至少包含：

- `user`
- `admin`
- `charging_station`
- `charging_pile`
- `charging_order`
- `recharge_record`
- `prediction`
- `device_log`
- `operation_log`

说明：

- 表名优先使用完整业务名，如 `charging_station`、`charging_pile`。
- 若后续决定使用简写 `station`、`charger`，必须在 schema 冻结前统一。

## 3. 命名规范

数据库表使用 snake_case。

数据库字段使用 snake_case。

Java 属性和 JSON 字段使用 camelCase。

示例：

```text
database: price_per_kwh
Java: pricePerKwh
JSON: pricePerKwh
```

## 4. 主键与关联

主键统一：

```text
BIGINT id
```

外键字段命名：

```text
user_id
station_id
pile_id
order_id
```

是否在数据库层建立外键约束，由数据库设计阶段统一确认；即使不建物理外键，也必须在业务层保证关联一致性。

## 5. 时间字段

时间字段统一使用：

```text
DATETIME
```

常用字段：

```text
created_at
updated_at
start_time
end_time
prediction_time
last_heartbeat_at
```

接口时间格式：

```text
yyyy-MM-dd HH:mm:ss
```

## 6. 金额字段

金额禁止使用浮点类型。

MySQL 使用：

```text
DECIMAL(10,2)
```

Java 使用：

```text
BigDecimal
```

字段示例：

```text
balance DECIMAL(10,2)
amount DECIMAL(10,2)
price_per_kwh DECIMAL(10,2)
```

## 7. 枚举字段

枚举必须全项目统一。

用户状态：

```text
NORMAL
FROZEN
```

电桩状态：

```text
AVAILABLE
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

设备状态：

```text
ONLINE
OFFLINE
FAULT
RESTARTING
```

数据库中可保存小写字符串或整数枚举，但 V1 建议保存可读字符串，降低联调成本。

## 8. 关键唯一性与索引

必须保证：

- `user.phone` 唯一。
- `admin.username` 唯一。
- `charging_pile.pile_no` 唯一或在站点内唯一。
- 同一用户不能同时存在多个活动订单。
- 同一电桩不能同时存在多个活动订单。

建议索引：

- `user.phone`
- `charging_order.user_id`
- `charging_order.pile_id`
- `charging_order.status`
- `charging_order.start_time`
- `charging_pile.station_id`
- `charging_pile.status`
- `prediction.station_id`
- `prediction.prediction_time`
- `device_log.device_id`

## 9. 事务规则

以下操作必须使用事务：

- 手机号自动注册
- 用户充值
- 创建充电订单
- 开始充电
- 停止充电
- 钱包结算
- 新增站点并自动生成电桩
- 管理员冻结 / 解冻用户
- 远程重启状态更新

结算必须保证：

```text
订单金额
用户余额扣减
订单状态
结算时间
```

同时成功或同时失败。

## 10. 预测结果

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

ML 结果必须进入数据库或通过后端 API 进入系统，不得只保存为本地临时图表。

## 11. 设备日志

`device_log` 至少应支持记录：

- 设备上线
- 设备离线
- 心跳
- 状态变化
- 充电遥测
- 远程重启
- ACK
- 命令超时
- 故障

具体字段在 schema 设计阶段冻结。

## 12. 种子数据

`database/init_data.sql` 应提供演示所需基础数据：

- 默认管理员 `admin / 123456`
- 示例用户
- 示例充电站
- 示例充电桩
- 示例历史订单
- 示例充值记录
- 示例预测结果

管理员密码不得明文存储，种子数据中应保存 hash。

## 13. 数据库迁移

数据库结构由：

```text
database/schema.sql
```

作为唯一正式版本。

V1 可不引入 Flyway / Liquibase，但每次结构修改必须同步更新：

- `database/schema.sql`
- `database/init_data.sql`
- `docs/04-DATABASE.md`
- 对应 Entity
- 相关接口文档

## 14. 变更流程

任何表结构变动必须：

```text
提出修改
↓
确认影响模块
↓
组长确认
↓
修改 schema.sql
↓
修改文档和代码
↓
通知相关成员
```

禁止个人本地私自增加字段后直接编码。
