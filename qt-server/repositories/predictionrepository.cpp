/*
 * 功能：实现预测结果的最新记录查询和展示记录查询。
 */
#include "predictionrepository.h"

#include <QJsonObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {

QJsonArray execute(QSqlQuery &query, QString *errorMessage)
{
    QJsonArray rows;
    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
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

void bindCommon(QSqlQuery &query, const QString &horizon, int limit)
{
    if (!horizon.isEmpty()) {
        query.bindValue(QStringLiteral(":horizon"), horizon);
    }
    query.bindValue(QStringLiteral(":limit"), limit);
}

}

QHash<qint64, PredictionInfo> PredictionRepository::listLatestByHorizon(
    QSqlDatabase &database, const QString &horizon, QString *errorMessage) const
{
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "SELECT id, station_id, horizon, predicted_load, predicted_available_count, "
        "prediction_time FROM prediction "
        "WHERE horizon = :horizon AND id IN ("
        "SELECT MAX(id) FROM prediction WHERE horizon = :horizon GROUP BY station_id)"));
    query.bindValue(QStringLiteral(":horizon"), horizon);
    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return {};
    }
    QHash<qint64, PredictionInfo> predictions;
    while (query.next()) {
        PredictionInfo prediction;
        prediction.stationId = query.value(1).toLongLong();
        prediction.horizon = query.value(2).toString();
        prediction.predictedLoad = query.value(3).toDouble();
        prediction.predictedAvailablePileCount = query.value(4).toInt();
        prediction.predictionTime = query.value(5).toString();
        predictions.insert(prediction.stationId, prediction);
    }
    return predictions;
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
    if (stationId > 0) {
        sql += QStringLiteral(" AND p.station_id=:stationId");
    }
    if (!horizon.isEmpty()) {
        sql += QStringLiteral(" AND p.horizon=:horizon");
    }
    sql += QStringLiteral(" ORDER BY p.prediction_time ASC, p.station_id ASC LIMIT :limit");
    QSqlQuery query(database);
    query.prepare(sql);
    if (stationId > 0) {
        query.bindValue(QStringLiteral(":stationId"), stationId);
    }
    bindCommon(query, horizon, limit);
    return execute(query, errorMessage);
}

QJsonArray PredictionRepository::recommendation(QSqlDatabase &database,
                                                const QString &horizon, int limit,
                                                QString *errorMessage) const
{
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "SELECT p.id,p.batch_id,p.station_id,s.name,p.prediction_time,p.horizon,"
        "p.predicted_load,p.predicted_available_count,p.peak_level,p.model_name,"
        "p.mae,p.rmse,p.generated_at FROM prediction p JOIN charging_station s ON s.id=p.station_id "
        "WHERE p.prediction_time >= datetime('now') AND p.predicted_available_count > 0 "
        "AND (:horizon='' OR p.horizon=:horizon) "
        "ORDER BY p.predicted_available_count DESC, p.predicted_load ASC LIMIT :limit"));
    bindCommon(query, horizon, limit);
    return execute(query, errorMessage);
}

QJsonArray PredictionRepository::warning(QSqlDatabase &database, const QString &horizon,
                                          int limit, QString *errorMessage) const
{
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "SELECT p.id,p.batch_id,p.station_id,s.name,p.prediction_time,p.horizon,"
        "p.predicted_load,p.predicted_available_count,p.peak_level,p.model_name,"
        "p.mae,p.rmse,p.generated_at FROM prediction p JOIN charging_station s ON s.id=p.station_id "
        "WHERE p.prediction_time >= datetime('now') AND p.predicted_load >= 0.7 "
        "AND (:horizon='' OR p.horizon=:horizon) "
        "ORDER BY p.predicted_load DESC, p.prediction_time ASC LIMIT :limit"));
    bindCommon(query, horizon, limit);
    return execute(query, errorMessage);
}
