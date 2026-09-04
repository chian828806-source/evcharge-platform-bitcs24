/*
 * 功能：实现STATION_LIST_NEARBY和STATION_DETAIL_GET的协议参数检查。
 */
#include "stationhandler.h"

#include "services/user/stationservice.h"
#include "shared/protocol/errorcodes.h"

#include <QJsonArray>

namespace {

bool isInteger(double value)
{
    return value == static_cast<double>(static_cast<qint64>(value));
}

ResponseMessage invalidPayload(const QString &requestId, const QString &message)
{
    return ResponseMessage::error(requestId, ErrorCodes::InvalidSocketMessage, message);
}

}

StationHandler::StationHandler(StationService *stationService)
    : m_stationService(stationService)
{
}

ResponseMessage StationHandler::listNearby(const RequestMessage &request,
                                           const SessionContext &)
{
    const QJsonValue longitudeValue = request.payload.value(QStringLiteral("longitude"));
    const QJsonValue latitudeValue = request.payload.value(QStringLiteral("latitude"));
    if (!longitudeValue.isDouble() || !latitudeValue.isDouble()) {
        return invalidPayload(request.requestId,
                              QStringLiteral("longitude and latitude must be numbers"));
    }

    QString district;
    const QJsonValue districtValue = request.payload.value(QStringLiteral("district"));
    if (!districtValue.isUndefined() && !districtValue.isNull()) {
        if (!districtValue.isString()) {
            return invalidPayload(request.requestId,
                                  QStringLiteral("district must be a string"));
        }
        district = districtValue.toString();
    }

    int limit = 20;
    const QJsonValue limitValue = request.payload.value(QStringLiteral("limit"));
    if (!limitValue.isUndefined() && !limitValue.isNull()) {
        if (!limitValue.isDouble() || !isInteger(limitValue.toDouble())) {
            return invalidPayload(request.requestId, QStringLiteral("limit must be an integer"));
        }
        limit = limitValue.toInt();
    }
    if (!m_stationService) {
        return ResponseMessage::error(request.requestId, ErrorCodes::InternalError,
                                      QStringLiteral("station module is unavailable"));
    }

    const auto result = m_stationService->listNearby(
        longitudeValue.toDouble(), latitudeValue.toDouble(), district, limit);
    if (!result.ok) {
        return ResponseMessage::error(request.requestId, result.code, result.message);
    }
    QJsonArray stations;
    for (const StationInfo &station : result.value) {
        stations.append(station.toJson());
    }
    return ResponseMessage::success(request.requestId, {
        {QStringLiteral("stations"), stations}
    });
}

ResponseMessage StationHandler::detailGet(const RequestMessage &request,
                                          const SessionContext &)
{
    const QJsonValue stationIdValue = request.payload.value(QStringLiteral("stationId"));
    if (!stationIdValue.isDouble() || !isInteger(stationIdValue.toDouble())
        || stationIdValue.toDouble() <= 0.0) {
        return invalidPayload(request.requestId,
                              QStringLiteral("stationId must be a positive integer"));
    }
    if (!m_stationService) {
        return ResponseMessage::error(request.requestId, ErrorCodes::InternalError,
                                      QStringLiteral("station module is unavailable"));
    }

    const auto result = m_stationService->detail(
        static_cast<qint64>(stationIdValue.toDouble()));
    if (!result.ok) {
        return ResponseMessage::error(request.requestId, result.code, result.message);
    }
    QJsonArray piles;
    for (const ChargingPileInfo &pile : result.value.piles) {
        piles.append(pile.toJson());
    }
    return ResponseMessage::success(request.requestId, {
        {QStringLiteral("station"), result.value.station.toJson()},
        {QStringLiteral("piles"), piles}
    });
}

ResponseMessage StationHandler::recommendation(const RequestMessage &request,
                                               const SessionContext &)
{
    const QJsonValue longitudeValue = request.payload.value(QStringLiteral("longitude"));
    const QJsonValue latitudeValue = request.payload.value(QStringLiteral("latitude"));
    if (!longitudeValue.isDouble() || !latitudeValue.isDouble()) {
        return invalidPayload(request.requestId,
                              QStringLiteral("longitude and latitude must be numbers"));
    }
    int limit = 5;
    const QJsonValue limitValue = request.payload.value(QStringLiteral("limit"));
    if (!limitValue.isUndefined()) {
        if (!limitValue.isDouble() || !isInteger(limitValue.toDouble())) {
            return invalidPayload(request.requestId, QStringLiteral("limit must be an integer"));
        }
        limit = limitValue.toInt();
    }
    QString horizon = QStringLiteral("1h");
    const QJsonValue horizonValue = request.payload.value(QStringLiteral("horizon"));
    if (!horizonValue.isUndefined()) {
        if (!horizonValue.isString()) {
            return invalidPayload(request.requestId, QStringLiteral("horizon must be a string"));
        }
        horizon = horizonValue.toString();
    }
    if (!m_stationService) {
        return ResponseMessage::error(request.requestId, ErrorCodes::InternalError,
                                      QStringLiteral("station module is unavailable"));
    }
    const auto result = m_stationService->recommendations(
        longitudeValue.toDouble(), latitudeValue.toDouble(), limit, horizon);
    if (!result.ok) {
        return ResponseMessage::error(request.requestId, result.code, result.message);
    }
    QJsonArray stations;
    for (const StationInfo &station : result.value) {
        stations.append(station.toJson());
    }
    return ResponseMessage::success(request.requestId, {
        {QStringLiteral("stations"), stations}
    });
}

void StationHandler::geocode(const RequestMessage &request, const SessionContext &,
                             MessageDispatcher::ResponseCallback callback)
{
    const QJsonValue addressValue = request.payload.value(QStringLiteral("address"));
    const QJsonValue districtValue = request.payload.value(QStringLiteral("district"));
    if (!addressValue.isString() || addressValue.toString().trimmed().isEmpty()
        || (!districtValue.isUndefined() && !districtValue.isNull()
            && !districtValue.isString())) {
        callback(invalidPayload(request.requestId,
                                QStringLiteral("address must be non-empty and district must be a string")));
        return;
    }
    if (!m_stationService) {
        callback(ResponseMessage::error(request.requestId, ErrorCodes::InternalError,
                                        QStringLiteral("station module is unavailable")));
        return;
    }
    m_stationService->geocode(districtValue.toString(), addressValue.toString(),
        [requestId = request.requestId, callback](ServiceResult<MapGeocodeResult> result) {
            if (!result.ok) {
                callback(ResponseMessage::error(requestId, result.code, result.message));
                return;
            }
            callback(ResponseMessage::success(requestId, {
                {QStringLiteral("formattedAddress"), result.value.formattedAddress},
                {QStringLiteral("longitude"), result.value.longitude},
                {QStringLiteral("latitude"), result.value.latitude}
            }));
        });
}

void StationHandler::routePlan(const RequestMessage &request, const SessionContext &,
                               MessageDispatcher::ResponseCallback callback)
{
    const QJsonValue originLongitude = request.payload.value(QStringLiteral("originLongitude"));
    const QJsonValue originLatitude = request.payload.value(QStringLiteral("originLatitude"));
    const QJsonValue destinationLongitude = request.payload.value(QStringLiteral("destinationLongitude"));
    const QJsonValue destinationLatitude = request.payload.value(QStringLiteral("destinationLatitude"));
    const QJsonValue mode = request.payload.value(QStringLiteral("mode"));
    const auto validCoordinate = [](const QJsonValue &longitude, const QJsonValue &latitude) {
        return longitude.isDouble() && latitude.isDouble()
            && longitude.toDouble() >= -180.0 && longitude.toDouble() <= 180.0
            && latitude.toDouble() >= -90.0 && latitude.toDouble() <= 90.0;
    };
    const QString normalizedMode = mode.toString().trimmed().toUpper();
    if (!validCoordinate(originLongitude, originLatitude)
        || !validCoordinate(destinationLongitude, destinationLatitude)
        || !mode.isString()
        || (normalizedMode != QStringLiteral("DRIVING")
            && normalizedMode != QStringLiteral("WALKING"))) {
        callback(invalidPayload(request.requestId,
            QStringLiteral("origin/destination coordinates and mode are invalid")));
        return;
    }
    if (!m_stationService) {
        callback(ResponseMessage::error(request.requestId, ErrorCodes::InternalError,
                                        QStringLiteral("station module is unavailable")));
        return;
    }
    m_stationService->planRoute(originLongitude.toDouble(), originLatitude.toDouble(),
        destinationLongitude.toDouble(), destinationLatitude.toDouble(), normalizedMode,
        [requestId = request.requestId, callback](ServiceResult<MapRoutePlanResult> result) {
            if (!result.ok) {
                callback(ResponseMessage::error(requestId, result.code, result.message));
                return;
            }
            QJsonArray points;
            for (const MapRoutePoint &point : result.value.polyline) {
                points.append(QJsonObject{
                    {QStringLiteral("longitude"), point.longitude},
                    {QStringLiteral("latitude"), point.latitude}
                });
            }
            callback(ResponseMessage::success(requestId, {
                {QStringLiteral("mode"), result.value.mode},
                {QStringLiteral("distanceMeters"), result.value.distanceMeters},
                {QStringLiteral("durationMinutes"), result.value.durationMinutes},
                {QStringLiteral("polyline"), points}
            }));
        });
}
