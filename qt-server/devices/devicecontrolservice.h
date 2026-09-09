#pragma once
#include <QObject>
#include <QHash>
#include <QJsonObject>
class DeviceRegistry;
class DatabaseManager;

class DeviceControlService : public QObject
{
    Q_OBJECT
public:
    DeviceControlService(DatabaseManager *databaseManager, DeviceRegistry *registry, QObject *parent=nullptr);
    bool send(qint64 pileId, const QString &command, const QJsonObject &payload = {});
    bool startCharging(qint64 pileId, qint64 orderId);
    bool stopCharging(qint64 pileId, qint64 orderId);
    bool restart(qint64 pileId, qint64 adminId, const QString &restoreStatus);
    void handleAck(qint64 pileId, const QJsonObject &ack);
    void handleFault(qint64 pileId, const QString &code, const QString &message);
    void handleOffline(qint64 pileId);
signals:
    void restartFinished(qint64 pileId, bool success);
private:
    void finishRestart(qint64 pileId, bool success, const QString &message);
    DatabaseManager *m_databaseManager;
    DeviceRegistry *m_registry;
    QHash<QString,qint64> m_commandPiles;
    QHash<qint64,qint64> m_restartAdmins;
    QHash<qint64,QString> m_restoreStatuses;
};
