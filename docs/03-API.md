# API 规范

## 1. 基本原则

系统所有客户端原则上通过统一后端 API 访问业务数据，不直接修改核心数据库。

API、数据库字段和公共状态定义属于项目公共契约。

未经讨论不得自行修改：

- API URL
- HTTP Method
- 请求字段
- 响应字段
- 公共状态值
- 数据库表名
- 数据库字段

如确需修改，应先确认影响范围，再同步修改相关文档与代码。

## 2. API 路径规范

统一前缀：

```text
/api
```

用户相关：

```text
/api/users
```

电站：

```text
/api/stations
```

电桩：

```text
/api/chargers
```

订单：

```text
/api/orders
```

管理员：

```text
/api/admin
```

统计：

```text
/api/statistics
```

预测：

```text
/api/predictions
```

## 3. REST API 命名规范

查询列表：

```http
GET /api/stations
```

查询单个：

```http
GET /api/stations/{id}
```

新增：

```http
POST /api/stations
```

修改：

```http
PUT /api/stations/{id}
```

删除：

```http
DELETE /api/stations/{id}
```

业务动作可以使用：

```http
POST /api/users/{id}/recharge
POST /api/chargers/{id}/restart
POST /api/orders/{id}/finish
```

禁止设计：

```text
/getStation
/getAllStation
/addStation
/doDeleteStation
/changeStationInformation
```

## 4. JSON 命名规范

所有 API JSON 字段统一使用 camelCase。

示例：

```json
{
  "stationId": 1,
  "stationName": "中关村充电站",
  "availableCount": 6,
  "totalCount": 10,
  "pricePerKwh": 1.5
}
```

禁止混用：

```text
station_id
stationId
StationID
stationID
```

## 5. API 统一返回结构

所有后端 API 使用统一格式：

```json
{
  "code": 200,
  "message": "success",
  "data": {}
}
```

成功示例：

```json
{
  "code": 200,
  "message": "success",
  "data": {
    "userId": 1,
    "nickname": "用户1234"
  }
}
```

失败示例：

```json
{
  "code": 4001,
  "message": "用户存在未完成订单",
  "data": null
}
```

后端统一实现：

```java
Result<T>
```

禁止每个 Controller 自己设计返回格式。

## 6. 接口文档规范

每个 API 至少描述：

```text
功能
URL
Method
请求参数
响应参数
成功示例
失败情况
```

示例：

```text
功能：
手机号登录/自动注册

POST /api/users/login

Request:
{
    "phone": "13800138000"
}

Response:
{
    "code": 200,
    "message": "success",
    "data": {
        "userId": 1,
        "nickname": "用户8000",
        "balance": 0.00
    }
}
```

SpringDoc Swagger 用于实时接口测试。

`docs/03-API.md` 用于团队确定正式接口协议。

## 7. API 修改流程

一旦接口进入联调阶段，任何人不得直接修改：

```text
URL
HTTP Method
请求字段
响应字段
状态含义
```

确需修改时：

```text
提出修改
↓
确认影响模块
↓
组长确认
↓
修改 API 文档
↓
修改代码
↓
通知相关成员
```

必须做到：

```text
先改文档，再改代码。
```
