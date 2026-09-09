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
#include <QSqlQuery>
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
bool DeviceControlService::completeHello(qint64 pileId,const QString &pileNo,double *ratedPower,QString *status)
{ QSqlDatabase db;QString error;if(!m_databaseManager->database(&db,&error)||!db.transaction())return false;PileRepository piles(db);QString current;if(!piles.deviceHello(pileId,pileNo,ratedPower,&current)){db.rollback();return false;}if(current==QStringLiteral("OFFLINE")){const auto state=piles.deviceState(pileId);OperationLogRepository log(db);if(!state||state->currentOrderId>0||!piles.compareAndSetStatus(pileId,QStringLiteral("OFFLINE"),QStringLiteral("AVAILABLE"),deviceNow())||!log.add(std::nullopt,QStringLiteral("DEVICE_RECONNECTED"),QStringLiteral("PILE"),pileId,QStringLiteral("OFFLINE"),QStringLiteral("AVAILABLE"),QStringLiteral("设备重连恢复"),deviceNow())||!db.commit()){db.rollback();return false;}current=QStringLiteral("AVAILABLE");}else if(!db.commit()){db.rollback();return false;}if(status)*status=current;return true; }
void DeviceControlService::recordHeartbeat(qint64 pileId)
{ QSqlDatabase db;QString error;if(!m_databaseManager->database(&db,&error))return;PileRepository(db).updateHeartbeat(pileId,deviceNow()); }
bool DeviceControlService::restart(qint64 pileId,qint64 adminId,const QString &restore)
{ if(m_pendingRestartCommands.contains(pileId))return false;m_restartAdmins[pileId]=adminId;m_restoreStatuses[pileId]=restore;QString commandId;if(!send(pileId,DeviceProtocol::Restart,{},&commandId)){finishRestart(pileId,{},false,QStringLiteral("device unavailable"),true);return false;}m_pendingRestartCommands[pileId]=commandId;QTimer::singleShot(5000,this,[this,pileId,commandId]{finishRestart(pileId,commandId,false,QStringLiteral("device ACK timeout"));});return true; }
void DeviceControlService::handleAck(qint64 pileId,const QJsonObject &ack)
{ const QString commandId=ack.value(QStringLiteral("commandId")).toString();if(ack.value(QStringLiteral("command")).toString()!=DeviceProtocol::Restart||m_pendingRestartCommands.value(pileId)!=commandId)return;finishRestart(pileId,commandId,ack.value(QStringLiteral("success")).toBool(),ack.value(QStringLiteral("message")).toString()); }
void DeviceControlService::finishRestart(qint64 pileId,const QString &commandId,bool ok,const QString &message,bool unreachable)
{ if(!m_restartAdmins.contains(pileId))return;if(!commandId.isEmpty()&&m_pendingRestartCommands.value(pileId)!=commandId)return;const qint64 admin=m_restartAdmins.value(pileId);QSqlDatabase db;QString error;if(!m_databaseManager->database(&db,&error)||!db.transaction())return;PileRepository piles(db);OperationLogRepository log(db);const QString after=ok?QStringLiteral("AVAILABLE"):(unreachable?QStringLiteral("OFFLINE"):QStringLiteral("FAULT"));if(!piles.compareAndSetStatus(pileId,QStringLiteral("RESTARTING"),after,deviceNow())||!log.add(admin,QStringLiteral("PILE_RESTART"),QStringLiteral("PILE"),pileId,QStringLiteral("RESTARTING"),after,message,deviceNow())||!db.commit()){db.rollback();return;}m_pendingRestartCommands.remove(pileId);m_restartAdmins.remove(pileId);m_restoreStatuses.remove(pileId);if(ok)m_registry->clearFault(pileId);emit restartFinished(pileId,ok); }
static void interruptForDevice(DatabaseManager *manager,qint64 pileId,const QString &target,const QString &reason)
{ QSqlDatabase db;QString error;if(!manager->database(&db,&error)||!db.transaction())return;PileRepository piles(db);const auto pile=piles.deviceState(pileId);if(!pile||pile->status==target||pile->status==QStringLiteral("RESTARTING")){db.rollback();return;}bool ok=true;const QString now=deviceNow();OrderRepository orders;
 if(pile->status==QStringLiteral("CHARGING")&&pile->currentOrderId>0){const auto order=orders.findById(db,pile->currentOrderId,&error);if(!order||order->status!=QStringLiteral("CHARGING"))ok=false;else{const auto progress=chargingProgressAt(*order,QDateTime::currentDateTime());bool stopped=false;ok=orders.stopOrder(db,order->orderId,now,progress.chargeMinutes,progress.energyKwh,progress.amountFen,&stopped,&error)&&stopped;}}
 else if(pile->status==QStringLiteral("RESERVED")&&pile->currentOrderId>0){const auto order=orders.findById(db,pile->currentOrderId,&error);bool cancelled=false;const QString cancelReason=target==QStringLiteral("FAULT")?QStringLiteral("设备故障自动取消"):QStringLiteral("设备离线自动取消");ok=order&&orders.cancelOrder(db,pile->currentOrderId,now,cancelReason,&cancelled,&error)&&cancelled;if(ok&&order->couponId>0){QSqlQuery coupon(db);coupon.prepare(QStringLiteral("UPDATE coupon SET status='AVAILABLE', order_id=NULL WHERE id=:couponId AND order_id=:orderId AND status='LOCKED'"));coupon.bindValue(QStringLiteral(":couponId"),order->couponId);coupon.bindValue(QStringLiteral(":orderId"),order->orderId);ok=coupon.exec()&&coupon.numRowsAffected()==1;}}
 ok=ok&&piles.transitionDeviceState(pileId,pile->status,target,now,pile->currentOrderId);OperationLogRepository log(db);ok=ok&&log.add(std::nullopt,target==QStringLiteral("FAULT")?QStringLiteral("DEVICE_FAULT"):QStringLiteral("DEVICE_OFFLINE"),QStringLiteral("PILE"),pileId,pile->status,target,reason,now);if(ok)db.commit();else db.rollback(); }
void DeviceControlService::handleFault(qint64 pileId,const QString &code,const QString &message){m_registry->fault(pileId,code,message);interruptForDevice(m_databaseManager,pileId,QStringLiteral("FAULT"),code+QStringLiteral(": ")+message);}
void DeviceControlService::handleOffline(qint64 pileId){interruptForDevice(m_databaseManager,pileId,QStringLiteral("OFFLINE"),QStringLiteral("设备离线自动取消/中断"));}
