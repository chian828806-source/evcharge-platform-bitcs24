#include "devicegatewayserver.h"
#include "devicesession.h"
#include <QTcpSocket>
DeviceGatewayServer::DeviceGatewayServer(DatabaseManager *d,DeviceRegistry *r,DeviceControlService *c,QObject *p):QTcpServer(p),m_databaseManager(d),m_registry(r),m_control(c){}
void DeviceGatewayServer::incomingConnection(qintptr fd){auto *s=new QTcpSocket(this);if(!s->setSocketDescriptor(fd)){s->deleteLater();return;}new DeviceSession(s,m_databaseManager,m_registry,m_control,this);}
