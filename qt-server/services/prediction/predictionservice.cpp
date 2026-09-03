#include "predictionservice.h"
#include "database/databasemanager.h"
#include "repositories/predictionrepository.h"
#include "shared/protocol/errorcodes.h"
#include <functional>
#include <QSqlDatabase>

PredictionService::PredictionService(DatabaseManager *databaseManager,
                                     PredictionRepository *repository)
    : m_databaseManager(databaseManager), m_repository(repository) {}

namespace {
ServiceResult<QJsonArray> run(DatabaseManager *manager, PredictionRepository *repo,
                              const std::function<QJsonArray(QSqlDatabase &, QString *)> &fn)
{
    QSqlDatabase database; QString error;
    if (!manager || !repo || !manager->database(&database, &error))
        return ServiceResult<QJsonArray>::failure(ErrorCodes::DatabaseError, error);
    QJsonArray value = fn(database, &error);
    if (!error.isEmpty()) return ServiceResult<QJsonArray>::failure(ErrorCodes::DatabaseError, error);
    return ServiceResult<QJsonArray>::success(value);
}
}

ServiceResult<QJsonArray> PredictionService::list(qint64 stationId, const QString &horizon, int limit) const
{ return run(m_databaseManager, m_repository, [=](QSqlDatabase &db, QString *e){ return m_repository->list(db, stationId, horizon, limit, e); }); }
ServiceResult<QJsonArray> PredictionService::recommendation(const QString &horizon, int limit) const
{ return run(m_databaseManager, m_repository, [=](QSqlDatabase &db, QString *e){ return m_repository->recommendation(db, horizon, limit, e); }); }
ServiceResult<QJsonArray> PredictionService::warning(const QString &horizon, int limit) const
{ return run(m_databaseManager, m_repository, [=](QSqlDatabase &db, QString *e){ return m_repository->warning(db, horizon, limit, e); }); }

ServiceResult<QJsonObject> PredictionService::importBatch(const QJsonObject &document) const
{
    QSqlDatabase database; QString error;
    if (!m_databaseManager || !m_repository || !m_databaseManager->database(&database, &error))
        return ServiceResult<QJsonObject>::failure(ErrorCodes::DatabaseError, error);
    const QJsonObject result = m_repository->importBatch(database, document, &error);
    return error.isEmpty() ? ServiceResult<QJsonObject>::success(result)
                           : ServiceResult<QJsonObject>::failure(ErrorCodes::DatabaseError, error);
}
