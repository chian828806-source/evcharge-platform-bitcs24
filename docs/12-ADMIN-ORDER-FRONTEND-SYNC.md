# 管理端订单页面接口说明

## 1. 功能范围

管理端新增只读“订单管理”页面，用于查看各用户的充电订单。页面支持手机号关键词、
订单状态和分页查询，不提供取消、退款或修改订单状态。

消息类型：`ADMIN_ORDER_LIST`。除登录外，请求必须携带 Admin Session。

## 2. 请求

```json
{
  "requestId": "REQ-唯一值",
  "type": "ADMIN_ORDER_LIST",
  "sessionId": "S-管理员登录后返回",
  "payload": {
    "page": 1,
    "pageSize": 20,
    "phoneKeyword": "1380",
    "status": "COMPLETED"
  }
}
```

| 字段 | 规则 |
| --- | --- |
| `page` | 可选，默认 1，正整数 |
| `pageSize` | 可选，默认 20，范围 1～50 |
| `phoneKeyword` | 可选，支持手机号部分匹配；空字符串表示全部用户 |
| `status` | 可选；空字符串表示全部状态 |

`status` 可选值：`CREATED`、`CHARGING`、`PENDING_PAYMENT`、`COMPLETED`、
`CANCELLED`。

## 3. 响应

```json
{
  "items": [
    {
      "orderId": 101,
      "orderNo": "O-20260908-001",
      "userId": 1,
      "userPhone": "13800138000",
      "userNickname": "测试用户",
      "stationId": 2,
      "stationName": "软件园智慧充电站",
      "pileId": 7,
      "pileNo": "P001",
      "powerKw": 60.0,
      "status": "COMPLETED",
      "priceFenPerKwh": 120,
      "serviceFeeFenPerKwh": 0,
      "totalPriceFenPerKwh": 120,
      "startAt": "2026-09-08 10:00:00",
      "endAt": "2026-09-08 10:30:00",
      "chargeMinutes": 30,
      "chargeSeconds": 1800,
      "energyKwh": 20.5,
      "amountFen": 2460,
      "createdAt": "2026-09-08 09:58:00"
    }
  ],
  "page": 1,
  "pageSize": 20,
  "total": 1
}
```

金额字段单位为分，UI 显示时除以 100；`energyKwh` 单位为 kWh。没有站点、电桩或时间
文本时，相应字段可能为 `null`。

## 4. UI 对接要求

- 表格显示订单号、用户手机号/昵称、站点、电桩、状态、电量、时长、金额和创建时间；
- 提供手机号输入、状态下拉框、查询、清空、上一页和下一页；
- 默认 `page=1`、`pageSize=20`，筛选条件变化后回到第 1 页；
- 请求期间显示“正在加载…”，失败或超时后保留原列表并显示重试提示；
- 以 `requestId` 记录请求对应的页码和筛选条件，只更新与当前条件一致的响应；
- `items=[]` 显示“当前条件下暂无订单”；
- `code=4003` 返回登录页，`code=4401` 提示筛选条件无效，`code=5001` 提示查询失败。

## 5. 与用户端接口的区别

`USER_ORDER_LIST` 只能查询当前 User Session 对应用户的订单；管理端必须使用
`ADMIN_ORDER_LIST`。前端不要在请求中自行传 `userId`，按用户查询统一使用
`phoneKeyword`。
