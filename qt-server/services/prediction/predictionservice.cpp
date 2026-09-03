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
