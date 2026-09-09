#include "devicecontrolservice.h"
#include "deviceregistry.h"
#include "devicesession.h"
#include "deviceprotocol.h"
#include "database/databasemanager.h"
#include "repositories/pilerepository.h"
#include "repositories/operationlogrepository.h"
#include <QDateTime>
#include <QTimer>
#include <QUuid>
#include <QSqlQuery>
#include <QSqlError>
#include <QtMath>

static QString deviceNow(){return QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));}
DeviceControlService::DeviceControlService(DatabaseManager *db,DeviceRegistry *r,QObject *p):QObject(p),m_databaseManager(db),m_registry(r){}
bool DeviceControlService::send(qint64 pileId,const QString &command,const QJsonObject &payload){ auto *s=m_registry->session(pileId); if(!s)return false; const QString id=QUuid::createUuid().toString(QUuid::WithoutBraces); m_commandPiles[id]=pileId; QJsonObject m={{QStringLiteral("type"),DeviceProtocol::Command},{QStringLiteral("commandId"),id},{QStringLiteral("command"),command},{QStringLiteral("pileId"),pileId},{QStringLiteral("payload"),payload}}; s->send(m); return true; }
bool DeviceControlService::startCharging(qint64 pileId,qint64 orderId){return send(pileId,DeviceProtocol::StartCharging,{{QStringLiteral("orderId"),orderId}});}
bool DeviceControlService::stopCharging(qint64 pileId,qint64 orderId){return send(pileId,DeviceProtocol::StopCharging,{{QStringLiteral("orderId"),orderId}});}
bool DeviceControlService::restart(qint64 pileId,qint64 adminId,const QString &restore){m_restartAdmins[pileId]=adminId;m_restoreStatuses[pileId]=restore; if(!send(pileId,DeviceProtocol::Restart)){finishRestart(pileId,false,QStringLiteral("device unavailable"));return false;} QTimer::singleShot(5000,this,[this,pileId]{if(m_restartAdmins.contains(pileId))finishRestart(pileId,false,QStringLiteral("device ACK timeout"));}); return true;}
void DeviceControlService::handleAck(qint64 pileId,const QJsonObject &ack){if(ack.value(QStringLiteral("command")).toString()==DeviceProtocol::Restart && m_restartAdmins.contains(pileId))finishRestart(pileId,ack.value(QStringLiteral("success")).toBool(),ack.value(QStringLiteral("message")).toString());}
void DeviceControlService::finishRestart(qint64 pileId,bool ok,const QString &message){if(!m_restartAdmins.contains(pileId))return; const qint64 admin=m_restartAdmins.take(pileId);m_restoreStatuses.remove(pileId); QSqlDatabase db;QString e;if(!m_databaseManager->database(&db,&e)||!db.transaction())return;PileRepository piles(db);OperationLogRepository log(db);const QString after=ok?QStringLiteral("AVAILABLE"):QStringLiteral("FAULT");if(!piles.compareAndSetStatus(pileId,QStringLiteral("RESTARTING"),after,deviceNow())||!log.add(admin,QStringLiteral("PILE_RESTART"),QStringLiteral("PILE"),pileId,QStringLiteral("RESTARTING"),after,message,deviceNow())||!db.commit()){db.rollback();return;}if(ok)m_registry->clearFault(pileId);emit restartFinished(pileId,ok);}
static void interruptForDevice(DatabaseManager *manager, qint64 pileId, const QString &target,
                               const QString &reason)
{
    QSqlDatabase db; QString error;
    if (!manager->database(&db, &error) || !db.transaction()) return;
    QSqlQuery pile(db);
    pile.prepare(QStringLiteral("SELECT status,current_order_id FROM charging_pile WHERE id=:id"));
    pile.bindValue(QStringLiteral(":id"), pileId);
    if (!pile.exec() || !pile.next()) { db.rollback(); return; }
    const QString before=pile.value(0).toString(); const qint64 orderId=pile.value(1).toLongLong();
    if (before==target || before==QStringLiteral("RESTARTING")) { db.rollback(); return; }
    const QString now=deviceNow(); bool ok=true;
    if (before==QStringLiteral("CHARGING") && orderId>0) {
        QSqlQuery order(db); order.prepare(QStringLiteral("SELECT start_at,price_fen_per_kwh,service_fee_fen_per_kwh FROM charging_order WHERE id=:id AND status='CHARGING'")); order.bindValue(QStringLiteral(":id"),orderId);
        if (!order.exec() || !order.next()) ok=false; else {
            const QDateTime started=QDateTime::fromString(order.value(0).toString(),QStringLiteral("yyyy-MM-dd HH:mm:ss"));
            const int minutes=qMax(0,started.secsTo(QDateTime::currentDateTime())/60);
            const double energy=0.0; const qint64 amount=qRound64(energy*(order.value(1).toLongLong()+order.value(2).toLongLong()));
            QSqlQuery update(db); update.prepare(QStringLiteral("UPDATE charging_order SET status='PENDING_PAYMENT',end_at=:now,charge_minutes=:minutes,energy_kwh=:energy,amount_fen=:amount,updated_at=:now WHERE id=:id AND status='CHARGING'")); update.bindValue(QStringLiteral(":now"),now);update.bindValue(QStringLiteral(":minutes"),minutes);update.bindValue(QStringLiteral(":energy"),energy);update.bindValue(QStringLiteral(":amount"),amount);update.bindValue(QStringLiteral(":id"),orderId);ok=update.exec()&&update.numRowsAffected()==1;
        }
    } else if (before==QStringLiteral("RESERVED") && orderId>0) {
        QSqlQuery cancel(db);cancel.prepare(QStringLiteral("UPDATE charging_order SET status='CANCELLED',cancelled_at=:now,cancel_reason=:reason,updated_at=:now WHERE id=:id AND status='CREATED'"));cancel.bindValue(QStringLiteral(":now"),now);cancel.bindValue(QStringLiteral(":reason"),reason);cancel.bindValue(QStringLiteral(":id"),orderId);ok=cancel.exec()&&cancel.numRowsAffected()==1;
    }
    QSqlQuery updatePile(db);updatePile.prepare(QStringLiteral("UPDATE charging_pile SET status=:target,current_order_id=NULL,updated_at=:now WHERE id=:id AND status=:before"));updatePile.bindValue(QStringLiteral(":target"),target);updatePile.bindValue(QStringLiteral(":now"),now);updatePile.bindValue(QStringLiteral(":id"),pileId);updatePile.bindValue(QStringLiteral(":before"),before);ok=ok&&updatePile.exec()&&updatePile.numRowsAffected()==1;
    OperationLogRepository log(db);ok=ok&&log.add(0,target==QStringLiteral("FAULT")?QStringLiteral("DEVICE_FAULT"):QStringLiteral("DEVICE_OFFLINE"),QStringLiteral("PILE"),pileId,before,target,reason,now);
    if (ok) db.commit(); else db.rollback();
}
void DeviceControlService::handleFault(qint64 pileId,const QString &code,const QString &message){m_registry->fault(pileId,code,message);interruptForDevice(m_databaseManager,pileId,QStringLiteral("FAULT"),code+QStringLiteral(": ")+message);}
void DeviceControlService::handleOffline(qint64 pileId){interruptForDevice(m_databaseManager,pileId,QStringLiteral("OFFLINE"),QStringLiteral("设备离线自动取消/中断"));}
