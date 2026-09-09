#include "favoritehandler.h"

#include "services/user/favoriteservice.h"
#include "shared/protocol/errorcodes.h"

#include <QJsonArray>

namespace {
bool integerValue(const QJsonValue &value)
{
    return value.isDouble()
        && value.toDouble() == static_cast<double>(static_cast<qint64>(value.toDouble()));
}
}

FavoriteHandler::FavoriteHandler(FavoriteService *service) : m_service(service) {}

ResponseMessage FavoriteHandler::toggle(const RequestMessage &request,
                                         const SessionContext &context)
{
    const QJsonValue stationId = request.payload.value(QStringLiteral("stationId"));
    if (!integerValue(stationId) || stationId.toDouble() <= 0) {
        return ResponseMessage::error(request.requestId, ErrorCodes::InvalidSocketMessage,
                                      QStringLiteral("stationId must be a positive integer"));
    }
    if (!m_service) {
        return ResponseMessage::error(request.requestId, ErrorCodes::InternalError,
                                      QStringLiteral("favorite module is unavailable"));
    }
    const auto result = m_service->toggle(context.principalId,
                                          static_cast<qint64>(stationId.toDouble()));
    if (!result.ok) {
        return ResponseMessage::error(request.requestId, result.code, result.message);
    }
    return ResponseMessage::success(request.requestId, {
        {QStringLiteral("stationId"), result.value.stationId},
        {QStringLiteral("isFavorite"), result.value.isFavorite}
    });
}

ResponseMessage FavoriteHandler::list(const RequestMessage &request,
                                       const SessionContext &context)
{
    const QJsonValue longitude = request.payload.value(QStringLiteral("longitude"));
    const QJsonValue latitude = request.payload.value(QStringLiteral("latitude"));
    const bool hasLongitude = !longitude.isUndefined() && !longitude.isNull();
    const bool hasLatitude = !latitude.isUndefined() && !latitude.isNull();
    if (hasLongitude != hasLatitude
        || (hasLongitude && (!longitude.isDouble() || !latitude.isDouble()))) {
        return ResponseMessage::error(request.requestId, ErrorCodes::InvalidSocketMessage,
            QStringLiteral("longitude and latitude must both be numbers when provided"));
    }
    if (!m_service) {
        return ResponseMessage::error(request.requestId, ErrorCodes::InternalError,
                                      QStringLiteral("favorite module is unavailable"));
    }
    const auto result = m_service->list(context.principalId, hasLongitude,
        longitude.toDouble(), latitude.toDouble());
    if (!result.ok) {
        return ResponseMessage::error(request.requestId, result.code, result.message);
    }
    QJsonArray stations;
    for (const StationInfo &station : result.value) stations.append(station.toJson());
    return ResponseMessage::success(request.requestId,
                                    {{QStringLiteral("stations"), stations}});
}
