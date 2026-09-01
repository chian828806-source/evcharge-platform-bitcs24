# 远程重启与设备扩展规范

## 1. 定位

任务书明确要求管理端能够模拟发送远程重启指令。V1 必做范围是远程重启模拟，不要求完整设备网关、心跳、遥测或串口协议。

完整设备接入可作为 OPTIONAL 扩展，不能替代 Qt 用户端与 Qt/C++ PC 服务端之间的 Socket 主业务通信。

## 2. V1 必做范围

远程重启流程：

```text
管理员选择电桩
        ↓
点击远程重启
        ↓
PC 服务与管理端记录操作
        ↓
电桩状态变为 RESTARTING
        ↓
模拟处理完成
        ↓
返回 AVAILABLE 或原可用状态
```

## 3. 状态

```text
AVAILABLE
RESERVED
CHARGING
FAULT
OFFLINE
RESTARTING
```

远程重启只允许作用于存在的电桩。对于 `RESERVED` 或 `CHARGING` 状态电桩，V1 可拒绝重启并提示“电桩正在使用中”。

## 4. Socket 主业务消息

远程重启作为管理端主业务消息，定义在 `docs/03-API.md`：

```text
ADMIN_PILE_RESTART
```

示例：

```json
{
  "requestId": "REQ-RESTART-001",
  "type": "ADMIN_PILE_RESTART",
  "sessionId": "ADMIN-S-001",
  "payload": {
    "pileId": 1
  }
}
```

## 5. 日志

远程重启必须记录：

- 管理员；
- 电桩；
- 操作时间；
- 操作前状态；
- 操作后状态；
- 结果消息。

## 6. OPTIONAL 完整设备接入

如时间允许，可扩展：

- Device Simulator；
- TCP Socket 或 Serial；
- HELLO；
- HEARTBEAT；
- STATUS；
- TELEMETRY；
- RESTART；
- ACK。

扩展协议进入开发前必须单独评审，不得影响 V1 必做功能。

## 7. V1代码边界

远程重启必须沿用现有分层，不得把状态修改直接写进Socket入口：

~~~text
管理界面或ADMIN_PILE_RESTART Handler
  → RestartService
  → PileRepository / OperationLogRepository
  → RestartSimulator
  → 状态结果信号
  → 管理界面刷新 / WebSocket数据刷新
~~~

| 模块 | 负责 |
| --- | --- |
| 网络Handler | 校验Session角色、读取pileId、调用RestartService、映射公共错误码 |
| RestartService | 判断电桩是否存在、当前状态是否允许重启、组织状态变化 |
| Repository | 在事务中更新charging_pile并写operation_log |
| RestartSimulator | 使用定时器模拟重启完成，不阻塞UI或Socket线程 |
| WebSocket模块 | 接收业务层提供的数据并推送已有大屏topic |

网络Handler不得直接执行UPDATE语句；Repository不得向Socket写JSON。

## 8. 并发与状态一致性

远程重启需要防止两个管理员同时操作同一电桩：

1. Service读取当前电桩状态；
2. 只允许符合docs/00-SRS-V1.0.md规则的状态进入RESTARTING；
3. 状态更新和“开始重启”日志放在同一数据库事务；
4. 第二个请求看到RESTARTING后必须拒绝重复启动；
5. 模拟完成后更新最终状态并记录结果日志；
6. 数据库失败时返回5001，不能先向界面报告成功。

RESERVED或CHARGING电桩的处理必须按本文件第3节执行，不允许网络层自行改变规则。

## 9. 模拟器实现规范

V1使用QTimer或独立Worker模拟耗时，不允许在UI线程、Socket读取回调或Service中使用sleep阻塞。

RestartSimulator至少接收：

- 电桩ID；
- 重启前状态；
- 本次操作的上下文。

完成后通过Qt信号返回：

- 电桩ID；
- 是否成功；
- 最终状态；
- 结果消息。

具体延迟时间属于演示配置，不写死在业务判断中。完整设备Simulator、HELLO、HEARTBEAT和ACK仍属于OPTIONAL，不能为了扩展而改变ADMIN_PILE_RESTART现有契约。

## 10. 验收标准

V1远程重启至少验证：

1. 管理员Session可以发起合法请求；
2. 普通用户或无效Session不能发起管理操作；
3. 不存在的电桩返回4202；
4. RESERVED或CHARGING电桩不会被错误重启；
5. 合法电桩进入RESTARTING并在模拟结束后恢复；
6. 并发重复请求只启动一个模拟任务；
7. 开始和完成结果都写入operation_log；
8. 数据库失败不会返回成功；
9. 重启模拟不阻塞Socket、UI和其他业务；
10. 管理界面和Web大屏最终能看到新的电桩状态。
