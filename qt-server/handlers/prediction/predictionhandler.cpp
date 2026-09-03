#include "predictionhandler.h"
#include "services/prediction/predictionservice.h"
#include "shared/protocol/errorcodes.h"
#include <QDateTime>
#include <QRegularExpression>
#include <cmath>
#include <limits>

namespace {
ResponseMessage invalid(const RequestMessage &r, const QString &m) { return ResponseMessage::error(r.requestId, ErrorCodes::InvalidSocketMessage, m); }
bool horizonOk(const QString &h) { return h.isEmpty() || h == QStringLiteral("1h") || h == QStringLiteral("6h") || h == QStringLiteral("24h"); }
bool positiveInteger(const QJsonValue &value, qint64 *result = nullptr)
{
    if (!value.isDouble()) return false;
    const double number = value.toDouble();
    if (!std::isfinite(number) || number <= 0 || number != std::floor(number)
        || number > static_cast<double>(std::numeric_limits<qint64>::max())) return false;
    if (result) *result = static_cast<qint64>(number);
    return true;
}
bool nonNegativeInteger(const QJsonValue &value)
{
    if (!value.isDouble()) return false;
    const double number = value.toDouble();
    return std::isfinite(number) && number >= 0 && number == std::floor(number)
        && number <= static_cast<double>(std::numeric_limits<int>::max());
}
bool isoTime(const QJsonValue &value, bool requireTimezone)
{
    if (!value.isString()) return false;
    const QString text = value.toString();
    if (text.isEmpty() || (requireTimezone && !QRegularExpression(
            QStringLiteral("(Z|[+-]\\d{2}:\\d{2})$")).match(text).hasMatch())) return false;
    return QDateTime::fromString(text, Qt::ISODate).isValid();
}
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
        const double predictedLoad = load.toDouble();
        const auto validMetric = [](const QJsonValue &metric) {
            return metric.isUndefined() || (metric.isDouble()
                && std::isfinite(metric.toDouble()) && metric.toDouble() >= 0);
        };
        if (!positiveInteger(station)
            || !isoTime(item.value(QStringLiteral("predictionTime")), false)
            || !isoTime(item.value(QStringLiteral("generatedAt")), true)
            || !(horizonValue == QStringLiteral("1h") || horizonValue == QStringLiteral("6h") || horizonValue == QStringLiteral("24h"))
            || !load.isDouble() || !std::isfinite(predictedLoad) || predictedLoad < 0 || predictedLoad > 1
            || !nonNegativeInteger(available)
            || !item.value(QStringLiteral("peakLevel")).toString().contains(QRegularExpression(QStringLiteral("^(LOW|MEDIUM|HIGH)$")))
            || item.value(QStringLiteral("modelName")).toString().trimmed().isEmpty()
            || !validMetric(item.value(QStringLiteral("mae")))
            || !validMetric(item.value(QStringLiteral("rmse")))) {
            if (error) *error = QStringLiteral("invalid prediction record");
            return false;
        }
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
    const qint64 stationId = station.isUndefined()
        ? 0 : static_cast<qint64>(station.toDouble());
    auto result = m_service->list(stationId, horizon(r.payload), limit(r.payload));
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
