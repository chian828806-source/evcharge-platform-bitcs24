#include "predictionrepository.h"

#include <QJsonObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QDateTime>

namespace {
QJsonArray execute(QSqlQuery &query, QString *errorMessage)
{
    QJsonArray rows;
    if (!query.exec()) {
        if (errorMessage) *errorMessage = query.lastError().text();
        return rows;
    }
    while (query.next()) {
        rows.append(QJsonObject{
            {QStringLiteral("predictionId"), query.value(0).toLongLong()},
            {QStringLiteral("batchId"), query.value(1).toString()},
            {QStringLiteral("stationId"), query.value(2).toLongLong()},
            {QStringLiteral("stationName"), query.value(3).toString()},
            {QStringLiteral("predictionTime"), query.value(4).toString()},
            {QStringLiteral("horizon"), query.value(5).toString()},
            {QStringLiteral("predictedLoad"), query.value(6).toDouble()},
            {QStringLiteral("predictedAvailableCount"), query.value(7).toInt()},
            {QStringLiteral("peakLevel"), query.value(8).toString()},
            {QStringLiteral("modelName"), query.value(9).toString()},
            {QStringLiteral("mae"), query.value(10).toDouble()},
            {QStringLiteral("rmse"), query.value(11).toDouble()},
            {QStringLiteral("generatedAt"), query.value(12).toString()}
        });
    }
    return rows;
}
}

QJsonObject PredictionRepository::importBatch(QSqlDatabase &database,
                                               const QJsonObject &document,
                                               QString *errorMessage) const
{
    const QString batchId = document.value(QStringLiteral("batchId")).toString();
    const QJsonArray predictions = document.value(QStringLiteral("predictions")).toArray();
    const QString generatedAt = document.value(QStringLiteral("generatedAt")).toString(
        QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    if (!database.transaction()) { if (errorMessage) *errorMessage = database.lastError().text(); return {}; }
    QSqlQuery batch(database);
    batch.prepare(QStringLiteral("INSERT OR IGNORE INTO prediction_batch(batch_id,status,generated_at,created_at) VALUES(:id,'IMPORTED',:generated,:created)"));
    batch.bindValue(QStringLiteral(":id"), batchId);
    batch.bindValue(QStringLiteral(":generated"), generatedAt);
    batch.bindValue(QStringLiteral(":created"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    if (!batch.exec()) { database.rollback(); if (errorMessage) *errorMessage = batch.lastError().text(); return {}; }
    const bool duplicate = batch.numRowsAffected() == 0;
    if (duplicate) { database.rollback(); return QJsonObject{{QStringLiteral("batchId"), batchId}, {QStringLiteral("status"), QStringLiteral("already_imported")}, {QStringLiteral("inserted"), 0}, {QStringLiteral("duplicate"), true}}; }
    QSqlQuery insert(database);
    insert.prepare(QStringLiteral("INSERT INTO prediction(batch_id,station_id,prediction_time,horizon,predicted_load,predicted_available_count,peak_level,model_name,mae,rmse,generated_at,created_at) VALUES(:batch,:station,:time,:horizon,:load,:available,:peak,:model,:mae,:rmse,:generated,:created)"));
    int inserted = 0;
    for (const QJsonValue &value : predictions) {
        const QJsonObject item = value.toObject();
        insert.bindValue(QStringLiteral(":batch"), batchId);
        insert.bindValue(QStringLiteral(":station"), item.value(QStringLiteral("stationId")).toInteger());
        insert.bindValue(QStringLiteral(":time"), item.value(QStringLiteral("predictionTime")).toString());
        insert.bindValue(QStringLiteral(":horizon"), item.value(QStringLiteral("horizon")).toString());
        insert.bindValue(QStringLiteral(":load"), item.value(QStringLiteral("predictedLoad")).toDouble());
        insert.bindValue(QStringLiteral(":available"), item.value(QStringLiteral("predictedAvailableCount")).toInt());
        insert.bindValue(QStringLiteral(":peak"), item.value(QStringLiteral("peakLevel")).toString());
        insert.bindValue(QStringLiteral(":model"), item.value(QStringLiteral("modelName")).toString());
        insert.bindValue(QStringLiteral(":mae"), item.contains(QStringLiteral("mae")) ? item.value(QStringLiteral("mae")).toDouble() : QVariant());
        insert.bindValue(QStringLiteral(":rmse"), item.contains(QStringLiteral("rmse")) ? item.value(QStringLiteral("rmse")).toDouble() : QVariant());
        insert.bindValue(QStringLiteral(":generated"), item.value(QStringLiteral("generatedAt")).toString(generatedAt));
        insert.bindValue(QStringLiteral(":created"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
        if (!insert.exec()) { database.rollback(); if (errorMessage) *errorMessage = insert.lastError().text(); return {}; }
        ++inserted;
    }
    if (!database.commit()) { if (errorMessage) *errorMessage = database.lastError().text(); return {}; }
    return QJsonObject{{QStringLiteral("batchId"), batchId}, {QStringLiteral("status"), QStringLiteral("imported")}, {QStringLiteral("inserted"), inserted}, {QStringLiteral("duplicate"), false}};
}

namespace {
void bindCommon(QSqlQuery &query, const QString &horizon, int limit)
{
    if (!horizon.isEmpty()) query.bindValue(QStringLiteral(":horizon"), horizon);
    query.bindValue(QStringLiteral(":limit"), limit);
}
}

QJsonArray PredictionRepository::list(QSqlDatabase &database, qint64 stationId,
                                       const QString &horizon, int limit,
                                       QString *errorMessage) const
{
    QString sql = QStringLiteral(
        "SELECT p.id,p.batch_id,p.station_id,s.name,p.prediction_time,p.horizon,"
        "p.predicted_load,p.predicted_available_count,p.peak_level,p.model_name,"
        "p.mae,p.rmse,p.generated_at FROM prediction p JOIN charging_station s "
        "ON s.id=p.station_id WHERE 1=1");
    if (stationId > 0) sql += QStringLiteral(" AND p.station_id=:stationId");
    if (!horizon.isEmpty()) sql += QStringLiteral(" AND p.horizon=:horizon");
    sql += QStringLiteral(" ORDER BY p.prediction_time ASC, p.station_id ASC LIMIT :limit");
    QSqlQuery query(database); query.prepare(sql);
    if (stationId > 0) query.bindValue(QStringLiteral(":stationId"), stationId);
    bindCommon(query, horizon, limit);
    return execute(query, errorMessage);
}

QJsonArray PredictionRepository::recommendation(QSqlDatabase &database, const QString &horizon,
                                                int limit, QString *errorMessage) const
{
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "SELECT p.id,p.batch_id,p.station_id,s.name,p.prediction_time,p.horizon,"
        "p.predicted_load,p.predicted_available_count,p.peak_level,p.model_name,p.mae,p.rmse,p.generated_at "
        "FROM prediction p JOIN charging_station s ON s.id=p.station_id "
        "WHERE p.prediction_time >= datetime('now') AND p.predicted_available_count > 0 "
        "AND (:horizon='' OR p.horizon=:horizon) ORDER BY p.predicted_available_count DESC, p.predicted_load ASC LIMIT :limit"));
    bindCommon(query, horizon, limit);
    return execute(query, errorMessage);
}

QJsonArray PredictionRepository::warning(QSqlDatabase &database, const QString &horizon,
                                         int limit, QString *errorMessage) const
{
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "SELECT p.id,p.batch_id,p.station_id,s.name,p.prediction_time,p.horizon,"
        "p.predicted_load,p.predicted_available_count,p.peak_level,p.model_name,p.mae,p.rmse,p.generated_at "
        "FROM prediction p JOIN charging_station s ON s.id=p.station_id "
        "WHERE p.prediction_time >= datetime('now') AND p.predicted_load >= 0.7 "
        "AND (:horizon='' OR p.horizon=:horizon) ORDER BY p.predicted_load DESC, p.prediction_time ASC LIMIT :limit"));
    bindCommon(query, horizon, limit);
    return execute(query, errorMessage);
}
