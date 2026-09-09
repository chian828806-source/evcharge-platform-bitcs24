#include "devicesession.h"
#include "deviceregistry.h"
#include "devicecontrolservice.h"
#include "deviceprotocol.h"
#include "database/databasemanager.h"
#include "shared/protocol/jsonlinecodec.h"
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <cmath>
#include <limits>

DeviceSession::DeviceSession(QTcpSocket *s,DatabaseManager *db,DeviceRegistry *r,DeviceControlService *c,QObject *p):QObject(p),m_socket(s),m_databaseManager(db),m_registry(r),m_control(c){s->setParent(this);connect(s,&QTcpSocket::readyRead,this,&DeviceSession::read);connect(s,&QTcpSocket::disconnected,this,&DeviceSession::disconnected);}
void DeviceSession::send(const QJsonObject &m){if(m_socket->state()==QAbstractSocket::ConnectedState)m_socket->write(JsonLineCodec::encode(m));}
void DeviceSession::close(){m_socket->disconnectFromHost();}
void DeviceSession::read(){bool overflow=false;for(const QByteArray &line:m_codec.append(m_socket->readAll(),&overflow)){QJsonParseError e;const auto d=QJsonDocument::fromJson(line,&e);if(e.error!=QJsonParseError::NoError||!d.isObject()){close();return;}process(d.object());}if(overflow)close();}
void DeviceSession::process(const QJsonObject &m){const QString type=m.value(QStringLiteral("type")).toString();if(!m_pileId){if(type!=DeviceProtocol::Hello){close();return;}hello(m);return;}if(m.value(QStringLiteral("pileId")).toVariant().toLongLong()!=m_pileId){close();return;}if(type==DeviceProtocol::Heartbeat){m_registry->heartbeat(m_pileId);QSqlDatabase db;QString e;if(m_databaseManager->database(&db,&e)){QSqlQuery q(db);q.prepare(QStringLiteral("UPDATE charging_pile SET last_heartbeat_at=:n WHERE id=:id"));q.bindValue(QStringLiteral(":n"),QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));q.bindValue(QStringLiteral(":id"),m_pileId);q.exec();}return;}if(type==DeviceProtocol::Telemetry){const QStringList ns={QStringLiteral("powerKw"),QStringLiteral("voltageV"),QStringLiteral("currentA"),QStringLiteral("temperatureC"),QStringLiteral("sessionEnergyKwh")};for(const QString &n:ns){const double v=m.value(n).toDouble(std::numeric_limits<double>::quiet_NaN());if(!std::isfinite(v)||v<0||v>100000)return;}m_registry->telemetry(m_pileId,m);return;}if(type==DeviceProtocol::Status && m.value(QStringLiteral("status")).toString()==QStringLiteral("FAULT")){m_control->handleFault(m_pileId,m.value(QStringLiteral("faultCode")).toString(),m.value(QStringLiteral("faultMessage")).toString());return;}if(type==DeviceProtocol::Ack){m_control->handleAck(m_pileId,m);return;}}
void DeviceSession::hello(const QJsonObject &m){if(m.value(QStringLiteral("protocolVersion")).toString()!=QStringLiteral("1.0")){close();return;}const qint64 id=m.value(QStringLiteral("pileId")).toVariant().toLongLong();const QString no=m.value(QStringLiteral("pileNo")).toString();QSqlDatabase db;QString e;if(id<=0||!m_databaseManager->database(&db,&e)){close();return;}QSqlQuery q(db);q.prepare(QStringLiteral("SELECT pile_no,power_kw,status FROM charging_pile WHERE id=:id"));q.bindValue(QStringLiteral(":id"),id);if(!q.exec()||!q.next()||q.value(0).toString()!=no){close();return;}m_pileId=id;m_registry->registerSession(id,this);send({{QStringLiteral("type"),DeviceProtocol::HelloAck},{QStringLiteral("messageId"),m.value(QStringLiteral("messageId"))},{QStringLiteral("pileId"),id},{QStringLiteral("heartbeatIntervalSeconds"),DeviceProtocol::HeartbeatIntervalSeconds},{QStringLiteral("ratedPowerKw"),q.value(1).toDouble()},{QStringLiteral("expectedStatus"),q.value(2).toString()}});}
void DeviceSession::disconnected(){if(m_pileId)m_registry->unregisterSession(this);deleteLater();}
