# 开发规范

## 1. Spring Boot 分层规范

后端统一采用：

```text
Controller
    ↓
Service
    ↓
Mapper
    ↓
MySQL
```

### 1.1 Controller

Controller 只负责：

- 接收 HTTP 请求
- 参数校验
- 调用 Service
- 返回结果

Controller 中禁止：

- 编写复杂业务逻辑
- 直接操作 Mapper
- 编写 SQL
- 直接修改数据库

错误示例：

```java
@PostMapping("/charge")
public Result charge(...) {
    User user = userMapper.selectById(...);

    if (...) {
        ...
    }

    chargerMapper.updateById(...);
    orderMapper.insert(...);
}
```

正确示例：

```java
@PostMapping("/charge")
public Result<?> startCharging(@RequestBody StartChargingDTO dto) {
    return Result.success(chargingService.startCharging(dto));
}
```

### 1.2 Service

所有核心业务逻辑必须放在 Service。

例如开始充电必须在 Service 中完成：

```text
检查用户
↓
检查未完成订单
↓
检查电桩状态
↓
创建订单
↓
修改电桩状态
```

涉及多个数据库操作的核心业务，应使用事务：

```java
@Transactional
public OrderVO startCharging(StartChargingDTO dto) {
    ...
}
```

重点事务包括：

- 开始充电
- 结束充电
- 钱包结算
- 用户充值
- 新增电站和电桩

### 1.3 Mapper

Mapper 只负责数据库访问。

简单 CRUD 优先使用 MyBatis-Plus：

```java
public interface UserMapper extends BaseMapper<User> {
}
```

复杂统计允许自定义 SQL，例如：

- 近 7 日营收
- 近 30 日营收
- 今日营收
- 电桩状态统计

禁止在多个位置重复编写相同 SQL。

## 2. Entity / DTO / VO 规范

三个概念必须严格区分。

### 2.1 Entity

Entity 对应数据库表，例如：

```text
User
Station
Charger
ChargingOrder
```

Entity 原则上只用于数据库层。

### 2.2 DTO

DTO 表示客户端传给后端的数据，例如：

```text
LoginDTO
RechargeDTO
CreateStationDTO
StartChargingDTO
FinishChargingDTO
```

客户端没有提交的字段，不应该为了方便全部放进 Entity。

### 2.3 VO

VO 表示后端返回给客户端的数据，例如：

```text
UserVO
StationVO
ChargerVO
RevenueVO
PredictionVO
```

使用 VO 可以避免把数据库内部字段直接暴露给客户端。

## 3. 错误处理规范

统一使用全局异常处理：

```text
BusinessException
GlobalExceptionHandler
```

业务层示例：

```java
throw new BusinessException(4001, "用户存在未完成订单");
```

禁止：

```java
return "失败";
```

也不要只使用：

```java
System.out.println("出错了");
```

## 4. 状态值规范

禁止在代码中出现无法理解的数字状态值：

```text
0
1
2
3
```

例如电桩状态应统一定义：

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

Java 示例：

```java
public enum ChargerStatus {
    AVAILABLE,
    CHARGING,
    FAULT,
    OFFLINE
}
```

同理：

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

## 5. 时间规范

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

避免不同模块各自使用不同格式。

## 6. 金额规范

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

## 7. Qt 开发规范

类名使用 PascalCase，例如：

```text
LoginWindow
StationPage
ChargingPage
OrderPage
ApiClient
```

函数名使用 camelCase，例如：

```text
loadStations()
startCharging()
updateUserInfo()
```

成员变量统一使用：

```text
m_userId
m_stationList
m_networkManager
```

推荐 UI 类和业务逻辑适度分离。

不要把以下内容全部写进一个按钮槽函数：

```text
网络请求
JSON 解析
页面绘制
业务判断
```

## 8. Qt 网络请求规范

Qt 两个客户端尽量共同维护一致的网络层设计。

推荐使用：

```text
ApiClient
```

负责：

```text
GET
POST
PUT
DELETE
```

业务页面不得重复实现 HTTP 底层请求。

页面示例：

```text
StationPage
```

调用：

```text
ApiClient::getStations(...)
```

而不是页面自己重新创建一套 `NetworkManager`。

## 9. Web 开发规范

所有业务数据必须来自 API。

ECharts 图表配置和 API 请求代码适度分离。

推荐目录：

```text
web-dashboard/

├── index.html
├── css/
├── js/
│   ├── api.js
│   ├── charts.js
│   └── main.js
└── assets/
```

其中：

- `api.js` 负责访问后端。
- `charts.js` 负责图表配置。

## 10. ML 模块规范

推荐目录：

```text
ml/

├── data/
├── models/
├── src/
│   ├── preprocess.py
│   ├── train.py
│   ├── predict.py
│   └── database.py
├── requirements.txt
└── README.md
```

禁止把训练代码、测试代码、数据清洗和绘图全部堆在一个 `.py` 文件。

模型输出至少统一包含：

```text
stationId
predictionTime
horizon
predictedLoad
predictedAvailableCount
```

并写入统一 `prediction` 表。

## 11. 功能优先级

项目使用三级优先级。

### P0

核心功能，没有就无法完成项目，例如：

- 登录
- 查询充电站
- 查看电桩
- 充电
- 订单结算
- 管理员管理
- 数据库
- 后端 API

### P1

任务书明确要求的重要功能，例如：

- 地图导航
- 营收趋势
- 用户冻结
- 大数据大屏
- ML 负荷预测

### P2

加分和优化功能，例如：

- 更漂亮动画
- 智能推荐
- 负荷预警
- 预约
- 多模型比较
- UI 主题切换

原则：P0 未完成前不得大规模投入 P2。

## 12. Definition of Done

一个功能不能以“代码写完了”作为完成标准。

必须同时满足：

```text
代码完成
+
能够运行
+
接口联通
+
数据正确
+
基本异常处理
+
已提交 Git
+
已合并 develop
```

才能标记为 Done。

## 13. 每日敏捷开发规范

项目采用“一天一个小迭代”。

每天开始前进行约 10 分钟同步，每人只说明：

```text
昨天完成了什么
今天完成什么
现在有什么阻塞
```

禁止长时间开会。

每天结束前进行一次集成。

标准：

```text
每天结束时 develop 必须处于能够运行的状态。
```

## 14. 禁止事项

项目期间禁止：

1. 未讨论自行修改数据库结构。
2. 未通知自行修改 API。
3. 客户端直接操作核心数据库。
4. main 分支直接开发。
5. 提交无法编译的代码。
6. Agent 大规模重写已有模块。
7. 为一个简单功能随意添加新框架。
8. 最后一天才第一次联调。
9. 只在个人电脑能够运行。
10. ML、大屏等模块使用完全脱离系统的假数据作为最终成果。
