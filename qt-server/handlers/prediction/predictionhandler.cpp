#include "predictionhandler.h"
#include "services/prediction/predictionservice.h"
#include "shared/protocol/errorcodes.h"
#include <QRegularExpression>

namespace {
ResponseMessage invalid(const RequestMessage &r, const QString &m) { return ResponseMessage::error(r.requestId, ErrorCodes::InvalidSocketMessage, m); }
bool horizonOk(const QString &h) { return h.isEmpty() || h == QStringLiteral("1h") || h == QStringLiteral("6h") || h == QStringLiteral("24h"); }
bool importDocumentOk(const QJsonObject &d, QString *error)
{
    if (d.value(QStringLiteral("schemaVersion")).toString() != QStringLiteral("1.0") || d.value(QStringLiteral("batchId")).toString().trimmed().isEmpty()) { if (error) *error = QStringLiteral("schemaVersion and batchId are required"); return false; }
    const auto values = d.value(QStringLiteral("predictions"));
    if (!values.isArray() || values.toArray().isEmpty()) { if (error) *error = QStringLiteral("predictions must be a non-empty array"); return false; }
    for (const auto &value : values.toArray()) {
        const auto item = value.toObject();
        const auto load = item.value(QStringLiteral("predictedLoad"));
        const auto station = item.value(QStringLiteral("stationId"));
        const auto available = item.value(QStringLiteral("predictedAvailableCount"));
        const auto horizonValue = item.value(QStringLiteral("horizon")).toString();
        if (!station.isDouble() || station.toDouble() != station.toInteger() || station.toInteger() <= 0 || item.value(QStringLiteral("predictionTime")).toString().isEmpty() || !(horizonValue == QStringLiteral("1h") || horizonValue == QStringLiteral("6h") || horizonValue == QStringLiteral("24h")) || !load.isDouble() || load.toDouble() < 0 || load.toDouble() > 1 || !available.isDouble() || available.toDouble() != available.toInteger() || available.toInteger() < 0 || !item.value(QStringLiteral("peakLevel")).toString().contains(QRegularExpression(QStringLiteral("^(LOW|MEDIUM|HIGH)$"))) || item.value(QStringLiteral("modelName")).toString().trimmed().isEmpty()) { if (error) *error = QStringLiteral("invalid prediction record"); return false; }
    }
    return true;
}
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
ResponseMessage PredictionHandler::recommendation(const RequestMessage &r, const SessionContext &)
{
    if (!limitOk(r.payload.value(QStringLiteral("limit"))) || !horizonOk(horizon(r.payload))) return invalid(r, QStringLiteral("invalid prediction filter"));
    auto result = m_service->recommendation(horizon(r.payload), limit(r.payload));
    return result.ok ? ResponseMessage::success(r.requestId, {{QStringLiteral("predictions"), result.value}}) : ResponseMessage::error(r.requestId, result.code, result.message);
}
ResponseMessage PredictionHandler::warning(const RequestMessage &r, const SessionContext &)
{
    if (!limitOk(r.payload.value(QStringLiteral("limit"))) || !horizonOk(horizon(r.payload))) return invalid(r, QStringLiteral("invalid prediction filter"));
    auto result = m_service->warning(horizon(r.payload), limit(r.payload));
    return result.ok ? ResponseMessage::success(r.requestId, {{QStringLiteral("predictions"), result.value}}) : ResponseMessage::error(r.requestId, result.code, result.message);
}

ResponseMessage PredictionHandler::importBatch(const RequestMessage &r, const SessionContext &)
{
    const QJsonObject document = r.payload.value(QStringLiteral("document")).toObject();
    QString validationError;
    if (!importDocumentOk(document, &validationError)) return invalid(r, validationError);
    auto result = m_service->importBatch(document);
    return result.ok ? ResponseMessage::success(r.requestId, result.value)
                     : ResponseMessage::error(r.requestId, result.code, result.message);
}
