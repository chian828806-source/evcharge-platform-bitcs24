# 数据库规范

## 1. 基本原则

统一采用 MySQL。

所有业务模块共享同一数据库。

数据库负责存储和维护平台核心业务数据，包括但不限于：

- 用户信息
- 管理员信息
- 充电站信息
- 充电桩信息
- 充电订单
- 用户充值记录
- 设备运行记录
- 机器学习预测结果

数据库结构一经确定，如需修改必须经过组内讨论。

## 2. 核心数据表

数据库中至少包含：

- `user`
- `admin`
- `station`
- `charger`
- `charging_order`
- `recharge_record`
- `prediction`
- `charger_log`

具体字段将在项目设计书确认后统一设计并冻结。

## 3. 数据库命名规范

数据库表使用 snake_case。

示例：

```text
charging_order
recharge_record
charger_log
```

数据库字段使用 snake_case。

示例：

```text
user_id
station_id
created_at
price_per_kwh
```

Java 属性使用 camelCase。

示例：

```text
userId
stationId
createdAt
pricePerKwh
```

## 4. 主键规范

统一使用：

```text
BIGINT
```

主键字段统一命名为：

```text
id
```

除非存在特殊理由，不使用：

```text
user_id
```

作为 `user` 表本身的主键字段。

统一示例：

```text
user.id
station.id
charger.id
charging_order.id
```

其他表通过以下字段进行关联：

```text
user_id
station_id
charger_id
```

## 5. 时间字段规范

所有数据库时间字段统一使用：

```text
DATETIME
```

命名示例：

```text
created_at
updated_at
start_time
end_time
```

前后端接口统一：

```text
yyyy-MM-dd HH:mm:ss
```

## 6. 金额字段规范

金额涉及：

- 钱包余额
- 充电价格
- 订单金额
- 充值金额

禁止使用 `float`。

Java 使用：

```text
BigDecimal
```

MySQL 使用：

```text
DECIMAL(10,2)
```

字段示例：

```text
balance DECIMAL(10,2)
amount DECIMAL(10,2)
price_per_kwh DECIMAL(10,2)
```

## 7. 状态值规范

禁止在代码中出现无法理解的数字状态值：

```text
0
1
2
3
```

电桩状态应统一定义：

```text
AVAILABLE
CHARGING
FAULT
OFFLINE
```

数据库可以保存：

```text
available
charging
fault
offline
```

或者保存整数枚举，但必须全项目统一。

用户状态：

```text
NORMAL
FROZEN
```

订单状态：

```text
CHARGING
COMPLETED
CANCELLED
```

## 8. 机器学习预测结果字段

模型输出至少统一包含：

```text
stationId
predictionTime
horizon
predictedLoad
predictedAvailableCount
```

预测结果应写入统一 `prediction` 表。

## 9. 数据库修改流程

数据库结构由：

```text
database/schema.sql
```

作为唯一正式版本。

禁止个人出现以下情况：

```text
我本地加了一个字段，但是忘了告诉大家。
```

任何表结构变动必须同步修改：

```text
schema.sql
docs/04-DATABASE.md
对应 Entity
相关 API
```
