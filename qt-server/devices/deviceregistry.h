#pragma once

#include <QDateTime>
#include <QHash>
#include <QJsonObject>
#include <QObject>

class DeviceSession;

struct DeviceRuntimeInfo {
    qint64 pileId = 0;
    DeviceSession *session = nullptr;
    bool managed = false;
    QDateTime lastHeartbeat;
    QJsonObject telemetry;
    QString faultCode;
    QString faultMessage;
};

class DeviceRegistry : public QObject
{
    Q_OBJECT
public:
    explicit DeviceRegistry(QObject *parent = nullptr);
    void registerSession(qint64 pileId, DeviceSession *session);
    void unregisterSession(DeviceSession *session);
    void heartbeat(qint64 pileId);
    void telemetry(qint64 pileId, const QJsonObject &telemetry);
    void fault(qint64 pileId, const QString &code, const QString &message);
    void clearFault(qint64 pileId);
    bool isManaged(qint64 pileId) const;
    bool isOnline(qint64 pileId) const;
    DeviceSession *session(qint64 pileId) const;
    QJsonObject liveFields(qint64 pileId) const;
    QList<qint64> expiredPiles() const;
signals:
    void becameOffline(qint64 pileId);
private:
    QHash<qint64, DeviceRuntimeInfo> m_devices;
};
