/*
 * 功能：实现预测结果的最新记录查询。
 */
#include "predictionrepository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

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
