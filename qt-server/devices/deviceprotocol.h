#pragma once

#include <QString>

namespace DeviceProtocol {
inline const QString Hello = QStringLiteral("DEVICE_HELLO");
inline const QString HelloAck = QStringLiteral("DEVICE_HELLO_ACK");
inline const QString Heartbeat = QStringLiteral("DEVICE_HEARTBEAT");
inline const QString Telemetry = QStringLiteral("DEVICE_TELEMETRY");
inline const QString Status = QStringLiteral("DEVICE_STATUS");
inline const QString Ack = QStringLiteral("DEVICE_ACK");
inline const QString Command = QStringLiteral("DEVICE_COMMAND");
inline const QString StartCharging = QStringLiteral("START_CHARGING");
inline const QString StopCharging = QStringLiteral("STOP_CHARGING");
inline const QString Restart = QStringLiteral("RESTART");
inline constexpr int HeartbeatIntervalSeconds = 2;
inline constexpr int HeartbeatTimeoutSeconds = 6;
}
