#include "predictionhandler.h"
#include "services/prediction/predictionservice.h"
#include "shared/protocol/errorcodes.h"

namespace {
ResponseMessage invalid(const RequestMessage &r, const QString &m) { return ResponseMessage::error(r.requestId, ErrorCodes::InvalidSocketMessage, m); }
bool horizonOk(const QString &h) { return h.isEmpty() || h == QStringLiteral("1h") || h == QStringLiteral("6h") || h == QStringLiteral("24h"); }
bool limitOk(const QJsonValue &v) { return v.isUndefined() || (v.isDouble() && v.toInt() >= 1 && v.toInt() <= 100 && v.toDouble() == v.toInt()); }
int limit(const QJsonObject &p) { return p.value(QStringLiteral("limit")).isUndefined() ? 20 : p.value(QStringLiteral("limit")).toInt(); }
QString horizon(const QJsonObject &p) { return p.value(QStringLiteral("horizon")).toString(); }
}
PredictionHandler::PredictionHandler(PredictionService *service) : m_service(service) {}
ResponseMessage PredictionHandler::list(const RequestMessage &r, const SessionContext &)
{
    auto station = r.payload.value(QStringLiteral("stationId"));
    if (!station.isUndefined() && (!station.isDouble() || station.toInt() <= 0 || station.toDouble() != station.toInt())) return invalid(r, QStringLiteral("stationId must be a positive integer"));
    if (!limitOk(r.payload.value(QStringLiteral("limit"))) || !horizonOk(horizon(r.payload))) return invalid(r, QStringLiteral("invalid prediction filter"));
    auto result = m_service->list(station.isUndefined() ? 0 : station.toInteger(), horizon(r.payload), limit(r.payload));
    return result.ok ? ResponseMessage::success(r.requestId, {{QStringLiteral("predictions"), result.value}}) : ResponseMessage::error(r.requestId, result.code, result.message);
}
ResponseMessage PredictionHandler::warning(const RequestMessage &r, const SessionContext &)
{
    if (!limitOk(r.payload.value(QStringLiteral("limit"))) || !horizonOk(horizon(r.payload))) return invalid(r, QStringLiteral("invalid prediction filter"));
    auto result = m_service->warning(horizon(r.payload), limit(r.payload));
    return result.ok ? ResponseMessage::success(r.requestId, {{QStringLiteral("predictions"), result.value}}) : ResponseMessage::error(r.requestId, result.code, result.message);
}
