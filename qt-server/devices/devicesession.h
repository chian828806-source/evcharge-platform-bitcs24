#pragma once
#include <QObject>
#include <QJsonObject>
#include "shared/protocol/jsonlinecodec.h"
class QTcpSocket;
class DeviceRegistry;
class DeviceControlService;
class DatabaseManager;

class DeviceSession : public QObject
{
    Q_OBJECT
public:
    DeviceSession(QTcpSocket *socket, DatabaseManager *databaseManager, DeviceRegistry *registry, DeviceControlService *control, QObject *parent=nullptr);
    void send(const QJsonObject &message);
    void close();
private slots:
    void read();
    void disconnected();
private:
    void process(const QJsonObject &message);
    void hello(const QJsonObject &message);
    QTcpSocket *m_socket;
    DatabaseManager *m_databaseManager;
    DeviceRegistry *m_registry;
    DeviceControlService *m_control;
    JsonLineCodec m_codec;
    qint64 m_pileId=0;
};
