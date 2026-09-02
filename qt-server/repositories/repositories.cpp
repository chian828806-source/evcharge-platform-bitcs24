#include "repositories.h"
#include <QHash>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <utility>

RepositoryBase::RepositoryBase(QSqlDatabase database) : m_database(std::move(database)) {}
QString RepositoryBase::lastError() const { return m_lastError; }
void RepositoryBase::clearError() const { m_lastError.clear(); }

QJsonObject AdminRepository::findByUsername(const QString &username) const
{
    m_lastError.clear();
    QSqlQuery q(m_database); q.prepare(QStringLiteral("SELECT id,password_hash,display_name,status FROM admin WHERE username=:username"));
    q.bindValue(QStringLiteral(":username"), username);
    if (!q.exec()) { m_lastError = q.lastError().text(); return {}; }
    if (!q.next()) return {};
    return {{QStringLiteral("adminId"), q.value(0).toLongLong()}, {QStringLiteral("passwordHash"), q.value(1).toString()}, {QStringLiteral("displayName"), q.value(2).toString()}, {QStringLiteral("status"), q.value(3).toString()}};
}

bool AdminRepository::updateLastLogin(qint64 id, const QString &now) const
{
    m_lastError.clear();
    QSqlQuery q(m_database); q.prepare(QStringLiteral("UPDATE admin SET last_login_at=:now,updated_at=:now WHERE id=:id"));
    q.bindValue(QStringLiteral(":now"), now); q.bindValue(QStringLiteral(":id"), id);
    if (!q.exec()) m_lastError = q.lastError().text(); return q.isActive();
}

QJsonObject OrderRepository::revenueSummary(const QDate &today) const
{
    m_lastError.clear();
    const QString dayStart = today.toString(QStringLiteral("yyyy-MM-dd 00:00:00"));
    const QString dayEnd = today.addDays(1).toString(QStringLiteral("yyyy-MM-dd 00:00:00"));
    const QString monthStart = QDate(today.year(), today.month(), 1).toString(QStringLiteral("yyyy-MM-dd 00:00:00"));
    QSqlQuery q(m_database); q.prepare(QStringLiteral("SELECT COALESCE(SUM(CASE WHEN paid_at>=:dayStart AND paid_at<:dayEnd THEN amount_fen ELSE 0 END),0),COALESCE(SUM(CASE WHEN paid_at>=:monthStart AND paid_at<:dayEnd THEN amount_fen ELSE 0 END),0),COALESCE(SUM(amount_fen),0) FROM charging_order WHERE status='COMPLETED'"));
    q.bindValue(QStringLiteral(":dayStart"), dayStart); q.bindValue(QStringLiteral(":dayEnd"), dayEnd); q.bindValue(QStringLiteral(":monthStart"), monthStart);
    if (!q.exec() || !q.next()) { m_lastError = q.lastError().text(); return {}; }
    return {{QStringLiteral("todayRevenueFen"), q.value(0).toLongLong()}, {QStringLiteral("monthRevenueFen"), q.value(1).toLongLong()}, {QStringLiteral("totalRevenueFen"), q.value(2).toLongLong()}};
}

QJsonArray OrderRepository::revenueTrend(const QDate &first, int days) const
{
    m_lastError.clear();
    QSqlQuery q(m_database); q.prepare(QStringLiteral("SELECT substr(paid_at,1,10),COALESCE(SUM(amount_fen),0) FROM charging_order WHERE status='COMPLETED' AND paid_at>=:start GROUP BY substr(paid_at,1,10)"));
    q.bindValue(QStringLiteral(":start"), first.toString(QStringLiteral("yyyy-MM-dd 00:00:00")));
    if (!q.exec()) { m_lastError = q.lastError().text(); return {}; }
    QHash<QString,qint64> values; while(q.next()) values.insert(q.value(0).toString(),q.value(1).toLongLong());
    QJsonArray result; for(int i=0;i<days;++i){const QString date=first.addDays(i).toString(Qt::ISODate);result.append(QJsonObject{{QStringLiteral("date"),date},{QStringLiteral("revenueFen"),values.value(date)}});} return result;
}

QJsonArray PileRepository::statusSummary(int *total) const
{
    m_lastError.clear();
    QSqlQuery q(m_database); if(!q.exec(QStringLiteral("SELECT status,COUNT(*) FROM charging_pile GROUP BY status"))){m_lastError=q.lastError().text();return{};}
    QHash<QString,int> counts; int sum=0; while(q.next()){counts.insert(q.value(0).toString(),q.value(1).toInt());sum+=q.value(1).toInt();} if(total)*total=sum;
    QJsonArray result; for(const QString &s:{QStringLiteral("AVAILABLE"),QStringLiteral("RESERVED"),QStringLiteral("CHARGING"),QStringLiteral("FAULT"),QStringLiteral("OFFLINE"),QStringLiteral("RESTARTING")}){const int c=counts.value(s);result.append(QJsonObject{{QStringLiteral("status"),s},{QStringLiteral("count"),c},{QStringLiteral("ratio"),sum?double(c)/sum:0.0}});} return result;
}

QJsonArray PileRepository::list(qint64 stationId) const
{
    m_lastError.clear();
    QSqlQuery q(m_database); QString sql=QStringLiteral("SELECT p.id,p.pile_no,s.id,s.name,p.type,p.power_kw,p.status,p.total_charge_count,p.total_charge_minutes FROM charging_pile p JOIN charging_station s ON s.id=p.station_id"); if(stationId>0)sql+=QStringLiteral(" WHERE s.id=:stationId"); sql+=QStringLiteral(" ORDER BY s.station_no,p.pile_no"); q.prepare(sql); if(stationId>0)q.bindValue(QStringLiteral(":stationId"),stationId); if(!q.exec()){m_lastError=q.lastError().text();return{};}
    QJsonArray result; while(q.next())result.append(QJsonObject{{QStringLiteral("pileId"),q.value(0).toLongLong()},{QStringLiteral("pileNo"),q.value(1).toString()},{QStringLiteral("stationId"),q.value(2).toLongLong()},{QStringLiteral("stationName"),q.value(3).toString()},{QStringLiteral("type"),q.value(4).toString()},{QStringLiteral("powerKw"),q.value(5).toDouble()},{QStringLiteral("status"),q.value(6).toString()},{QStringLiteral("totalChargeCount"),q.value(7).toInt()},{QStringLiteral("totalChargeMinutes"),q.value(8).toInt()}}); return result;
}

QString PileRepository::status(qint64 id,bool *found) const {QSqlQuery q(m_database);q.prepare(QStringLiteral("SELECT status FROM charging_pile WHERE id=:id"));q.bindValue(QStringLiteral(":id"),id);if(!q.exec()){m_lastError=q.lastError().text();if(found)*found=false;return{};}const bool ok=q.next();if(found)*found=ok;return ok?q.value(0).toString():QString();}
bool PileRepository::compareAndSetStatus(qint64 id,const QString &before,const QString &after,const QString &now) const {QSqlQuery q(m_database);q.prepare(QStringLiteral("UPDATE charging_pile SET status=:after,updated_at=:now WHERE id=:id AND status=:before"));q.bindValue(QStringLiteral(":after"),after);q.bindValue(QStringLiteral(":now"),now);q.bindValue(QStringLiteral(":id"),id);q.bindValue(QStringLiteral(":before"),before);if(!q.exec()){m_lastError=q.lastError().text();return false;}return q.numRowsAffected()==1;}
bool PileRepository::createForStation(qint64 stationId,int count,const QString &now) const {for(int n=1;n<=count;++n){QSqlQuery q(m_database);q.prepare(QStringLiteral("INSERT INTO charging_pile(station_id,pile_no,type,power_kw,status,created_at,updated_at) VALUES(:stationId,:no,:type,:power,'AVAILABLE',:now,:now)"));q.bindValue(QStringLiteral(":stationId"),stationId);q.bindValue(QStringLiteral(":no"),QStringLiteral("P%1").arg(n,3,10,QLatin1Char('0')));q.bindValue(QStringLiteral(":type"),n%2?QStringLiteral("FAST"):QStringLiteral("SLOW"));q.bindValue(QStringLiteral(":power"),n%2?60.0:7.0);q.bindValue(QStringLiteral(":now"),now);if(!q.exec()){m_lastError=q.lastError().text();return false;}}return true;}

QJsonArray StationRepository::list() const {QSqlQuery q(m_database);if(!q.exec(QStringLiteral("SELECT s.id,s.station_no,s.name,s.address,s.longitude,s.latitude,COUNT(p.id),COALESCE(SUM(CASE WHEN p.status<>'OFFLINE' THEN 1 ELSE 0 END),0) FROM charging_station s LEFT JOIN charging_pile p ON p.station_id=s.id GROUP BY s.id ORDER BY s.station_no"))){m_lastError=q.lastError().text();return{};}QJsonArray r;while(q.next()){const int c=q.value(6).toInt();r.append(QJsonObject{{QStringLiteral("stationId"),q.value(0).toLongLong()},{QStringLiteral("stationNo"),q.value(1).toString()},{QStringLiteral("name"),q.value(2).toString()},{QStringLiteral("address"),q.value(3).toString()},{QStringLiteral("longitude"),q.value(4).toDouble()},{QStringLiteral("latitude"),q.value(5).toDouble()},{QStringLiteral("pileCount"),c},{QStringLiteral("onlineRate"),c?q.value(7).toDouble()/c:0.0}});}return r;}
qint64 StationRepository::create(const QJsonObject &v,const QString &no,const QString &now) const {QSqlQuery q(m_database);q.prepare(QStringLiteral("INSERT INTO charging_station(station_no,name,address,longitude,latitude,price_fen_per_kwh,service_fee_fen_per_kwh,status,created_at,updated_at) VALUES(:no,:name,:address,:longitude,:latitude,:price,0,'NORMAL',:now,:now)"));q.bindValue(QStringLiteral(":no"),no);q.bindValue(QStringLiteral(":name"),v.value(QStringLiteral("name")).toString().trimmed());q.bindValue(QStringLiteral(":address"),v.value(QStringLiteral("address")).toString().trimmed());q.bindValue(QStringLiteral(":longitude"),v.value(QStringLiteral("longitude")).toDouble());q.bindValue(QStringLiteral(":latitude"),v.value(QStringLiteral("latitude")).toDouble());q.bindValue(QStringLiteral(":price"),v.value(QStringLiteral("priceFenPerKwh")).toInt(120));q.bindValue(QStringLiteral(":now"),now);if(!q.exec()){m_lastError=q.lastError().text();return 0;}return q.lastInsertId().toLongLong();}

QJsonArray UserRepository::list(const QString &input) const {QString k=input.trimmed();k.replace('\\',QStringLiteral("\\\\"));k.replace('%',QStringLiteral("\\%"));k.replace('_',QStringLiteral("\\_"));QSqlQuery q(m_database);q.prepare(QStringLiteral("SELECT id,phone,nickname,balance_fen,created_at,status FROM user WHERE phone LIKE :keyword ESCAPE '\\' ORDER BY id"));q.bindValue(QStringLiteral(":keyword"),QStringLiteral("%")+k+QStringLiteral("%"));if(!q.exec()){m_lastError=q.lastError().text();return{};}QJsonArray r;while(q.next())r.append(QJsonObject{{QStringLiteral("userId"),q.value(0).toLongLong()},{QStringLiteral("phone"),q.value(1).toString()},{QStringLiteral("nickname"),q.value(2).toString()},{QStringLiteral("balanceFen"),q.value(3).toLongLong()},{QStringLiteral("createdAt"),q.value(4).toString()},{QStringLiteral("status"),q.value(5).toString()}});return r;}
QJsonObject UserRepository::findById(qint64 id) const {QSqlQuery q(m_database);q.prepare(QStringLiteral("SELECT phone,status FROM user WHERE id=:id"));q.bindValue(QStringLiteral(":id"),id);if(!q.exec()){m_lastError=q.lastError().text();return{};}if(!q.next())return{};return{{QStringLiteral("phone"),q.value(0).toString()},{QStringLiteral("status"),q.value(1).toString()}};}
bool UserRepository::compareAndSetStatus(qint64 id,const QString &before,const QString &after,const QString &now) const {QSqlQuery q(m_database);q.prepare(QStringLiteral("UPDATE user SET status=:after,updated_at=:now WHERE id=:id AND status=:before"));q.bindValue(QStringLiteral(":after"),after);q.bindValue(QStringLiteral(":now"),now);q.bindValue(QStringLiteral(":id"),id);q.bindValue(QStringLiteral(":before"),before);if(!q.exec()){m_lastError=q.lastError().text();return false;}return q.numRowsAffected()==1;}

bool OperationLogRepository::add(qint64 adminId,const QString &action,const QString &targetType,qint64 targetId,const QString &before,const QString &after,const QString &message,const QString &now) const {QSqlQuery q(m_database);q.prepare(QStringLiteral("INSERT INTO operation_log(admin_id,action,target_type,target_id,before_status,after_status,result,message,created_at) VALUES(:adminId,:action,:targetType,:targetId,:before,:after,'SUCCESS',:message,:now)"));q.bindValue(QStringLiteral(":adminId"),adminId);q.bindValue(QStringLiteral(":action"),action);q.bindValue(QStringLiteral(":targetType"),targetType);q.bindValue(QStringLiteral(":targetId"),targetId);q.bindValue(QStringLiteral(":before"),before.isEmpty()?QVariant():before);q.bindValue(QStringLiteral(":after"),after.isEmpty()?QVariant():after);q.bindValue(QStringLiteral(":message"),message);q.bindValue(QStringLiteral(":now"),now);if(!q.exec()){m_lastError=q.lastError().text();return false;}return true;}
