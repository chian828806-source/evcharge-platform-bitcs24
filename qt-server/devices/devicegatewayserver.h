#pragma once
#include <QTcpServer>
class DatabaseManager;
class DeviceRegistry;
class DeviceControlService;
class DeviceGatewayServer : public QTcpServer
{
    Q_OBJECT
public:
    DeviceGatewayServer(DatabaseManager *databaseManager, DeviceRegistry *registry, DeviceControlService *control, QObject *parent=nullptr);
protected:
    void incomingConnection(qintptr socketDescriptor) override;
private:
    DatabaseManager *m_databaseManager; DeviceRegistry *m_registry; DeviceControlService *m_control;
};
