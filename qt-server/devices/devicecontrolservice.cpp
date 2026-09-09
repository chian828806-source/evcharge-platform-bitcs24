#include "devicecontrolservice.h"
#include "deviceregistry.h"
#include "devicesession.h"
#include "deviceprotocol.h"
#include "database/databasemanager.h"
#include "repositories/operationlogrepository.h"
#include "repositories/orderrepository.h"
#include "repositories/pilerepository.h"
#include "services/user/chargingprogress.h"
#include <QDateTime>
#include <QTimer>
#include <QUuid>

static QString deviceNow() { return QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")); }
DeviceControlService::DeviceControlService(DatabaseManager *db,DeviceRegistry *r,QObject *p):QObject(p),m_databaseManager(db),m_registry(r){}
bool DeviceControlService::send(qint64 pileId,const QString &command,const QJsonObject &payload,QString *commandId)
{ auto *s=m_registry->session(pileId);if(!s)return false;const QString id=QUuid::createUuid().toString(QUuid::WithoutBraces);if(commandId)*commandId=id;s->send({{QStringLiteral("type"),DeviceProtocol::Command},{QStringLiteral("commandId"),id},{QStringLiteral("command"),command},{QStringLiteral("pileId"),pileId},{QStringLiteral("payload"),payload}});return true; }
bool DeviceControlService::startCharging(qint64 pileId,qint64 orderId){return send(pileId,DeviceProtocol::StartCharging,{{QStringLiteral("orderId"),orderId}});}
bool DeviceControlService::stopCharging(qint64 pileId,qint64 orderId){return send(pileId,DeviceProtocol::StopCharging,{{QStringLiteral("orderId"),orderId}});}
bool DeviceControlService::validateHello(qint64 pileId,const QString &pileNo,double *ratedPower,QString *status)
{ QSqlDatabase db;QString error;if(!m_databaseManager->database(&db,&error))return false;PileRepository piles(db);return piles.deviceHello(pileId,pileNo,ratedPower,status); }
void DeviceControlService::recordHeartbeat(qint64 pileId)
{ QSqlDatabase db;QString error;if(!m_databaseManager->database(&db,&error))return;PileRepository(db).updateHeartbeat(pileId,deviceNow()); }
bool DeviceControlService::restart(qint64 pileId,qint64 adminId,const QString &restore)
{ if(m_pendingRestartCommands.contains(pileId))return false;m_restartAdmins[pileId]=adminId;m_restoreStatuses[pileId]=restore;QString commandId;if(!send(pileId,DeviceProtocol::Restart,{},&commandId)){finishRestart(pileId,{},false,QStringLiteral("device unavailable"),true);return false;}m_pendingRestartCommands[pileId]=commandId;QTimer::singleShot(5000,this,[this,pileId,commandId]{finishRestart(pileId,commandId,false,QStringLiteral("device ACK timeout"));});return true; }
void DeviceControlService::handleAck(qint64 pileId,const QJsonObject &ack)
{ const QString commandId=ack.value(QStringLiteral("commandId")).toString();if(ack.value(QStringLiteral("command")).toString()!=DeviceProtocol::Restart||m_pendingRestartCommands.value(pileId)!=commandId)return;finishRestart(pileId,commandId,ack.value(QStringLiteral("success")).toBool(),ack.value(QStringLiteral("message")).toString()); }
void DeviceControlService::finishRestart(qint64 pileId,const QString &commandId,bool ok,const QString &message,bool unreachable)
{ if(!m_restartAdmins.contains(pileId))return;if(!commandId.isEmpty()&&m_pendingRestartCommands.value(pileId)!=commandId)return;m_pendingRestartCommands.remove(pileId);const qint64 admin=m_restartAdmins.take(pileId);m_restoreStatuses.remove(pileId);QSqlDatabase db;QString error;if(!m_databaseManager->database(&db,&error)||!db.transaction())return;PileRepository piles(db);OperationLogRepository log(db);const QString after=ok?QStringLiteral("AVAILABLE"):(unreachable?QStringLiteral("OFFLINE"):QStringLiteral("FAULT"));if(!piles.compareAndSetStatus(pileId,QStringLiteral("RESTARTING"),after,deviceNow())||!log.add(admin,QStringLiteral("PILE_RESTART"),QStringLiteral("PILE"),pileId,QStringLiteral("RESTARTING"),after,message,deviceNow())||!db.commit()){db.rollback();return;}if(ok)m_registry->clearFault(pileId);emit restartFinished(pileId,ok); }
static void interruptForDevice(DatabaseManager *manager,qint64 pileId,const QString &target,const QString &reason)
{ QSqlDatabase db;QString error;if(!manager->database(&db,&error)||!db.transaction())return;PileRepository piles(db);const auto pile=piles.deviceState(pileId);if(!pile||pile->status==target||pile->status==QStringLiteral("RESTARTING")){db.rollback();return;}bool ok=true;const QString now=deviceNow();OrderRepository orders;
 if(pile->status==QStringLiteral("CHARGING")&&pile->currentOrderId>0){const auto order=orders.findById(db,pile->currentOrderId,&error);if(!order||order->status!=QStringLiteral("CHARGING"))ok=false;else{const auto progress=chargingProgressAt(*order,QDateTime::currentDateTime());bool stopped=false;ok=orders.stopOrder(db,order->orderId,now,progress.chargeMinutes,progress.energyKwh,progress.amountFen,&stopped,&error)&&stopped;}}
 else if(pile->status==QStringLiteral("RESERVED")&&pile->currentOrderId>0){bool cancelled=false;const QString cancelReason=target==QStringLiteral("FAULT")?QStringLiteral("设备故障自动取消"):QStringLiteral("设备离线自动取消");ok=orders.cancelOrder(db,pile->currentOrderId,now,cancelReason,&cancelled,&error)&&cancelled;}
 ok=ok&&piles.transitionDeviceState(pileId,pile->status,target,now,pile->currentOrderId);OperationLogRepository log(db);ok=ok&&log.add(std::nullopt,target==QStringLiteral("FAULT")?QStringLiteral("DEVICE_FAULT"):QStringLiteral("DEVICE_OFFLINE"),QStringLiteral("PILE"),pileId,pile->status,target,reason,now);if(ok)db.commit();else db.rollback(); }
void DeviceControlService::handleFault(qint64 pileId,const QString &code,const QString &message){m_registry->fault(pileId,code,message);interruptForDevice(m_databaseManager,pileId,QStringLiteral("FAULT"),code+QStringLiteral(": ")+message);}
void DeviceControlService::handleOffline(qint64 pileId){interruptForDevice(m_databaseManager,pileId,QStringLiteral("OFFLINE"),QStringLiteral("设备离线自动取消/中断"));}
