/*
 * 功能：实现腾讯地图 WebService 地理编码调用与超时、响应校验。
 */
#include "mapadapter.h"

#include "shared/protocol/errorcodes.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QCryptographicHash>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSharedPointer>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QStringList>

#include <cmath>
#include <algorithm>

namespace {

bool validCoordinate(double longitude, double latitude)
{
    return std::isfinite(longitude) && std::isfinite(latitude)
        && longitude >= -180.0 && longitude <= 180.0
        && latitude >= -90.0 && latitude <= 90.0;
}

QString coordinateText(double longitude, double latitude)
{
    // 腾讯路线接口要求纬度在前、经度在后。
    return QStringLiteral("%1,%2")
        .arg(latitude, 0, 'f', 6)
        .arg(longitude, 0, 'f', 6);
}

}

MapAdapter::MapAdapter(const QString &apiKey, const QString &signingSecret)
    : m_apiKey(apiKey.trimmed()), m_signingSecret(signingSecret.trimmed())
{
}

bool MapAdapter::isConfigured() const
{
    return !m_apiKey.isEmpty();
}

void MapAdapter::appendSignature(QUrl &url, QUrlQuery *query) const
{
    if (m_signingSecret.isEmpty() || !query) {
        return;
    }

    // 腾讯 WebService 签名：path + '?' + 按参数名排序的 query + SK，再作 MD5。
    // sig 本身不参与签名；签名完成后才加入最终请求参数。
    QList<QPair<QString, QString>> items = query->queryItems(QUrl::FullyDecoded);
    std::sort(items.begin(), items.end(), [](const auto &left, const auto &right) {
        return left.first < right.first;
    });
    QStringList pieces;
    for (const auto &item : items) {
        pieces.append(item.first + QLatin1Char('=') + item.second);
    }
    const QByteArray source = (url.path() + QLatin1Char('?')
        + pieces.join(QLatin1Char('&')) + m_signingSecret).toUtf8();
    query->addQueryItem(QStringLiteral("sig"), QString::fromLatin1(
        QCryptographicHash::hash(source, QCryptographicHash::Md5).toHex()));
}

void MapAdapter::geocode(const QString &district, const QString &address,
                         Callback callback)
{
    if (!isConfigured()) {
        callback(ServiceResult<MapGeocodeResult>::failure(
            ErrorCodes::InternalError,
            QStringLiteral("map service is not configured")));
        return;
    }

    QUrl url(QStringLiteral("https://apis.map.qq.com/ws/geocoder/v1/"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("address"), address.trimmed());
    if (!district.trimmed().isEmpty()) {
        query.addQueryItem(QStringLiteral("region"), district.trimmed());
    }
    query.addQueryItem(QStringLiteral("key"), m_apiKey);
    appendSignature(url, &query);
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("EVCharge-Qt-Server/1.0"));
    auto *reply = m_network.get(request);
    auto *timeout = new QTimer(reply);
    timeout->setSingleShot(true);
    const auto finished = QSharedPointer<bool>::create(false);
    const auto complete = [reply, timeout, finished, callback]
        (ServiceResult<MapGeocodeResult> result) {
        if (*finished) {
            return;
        }
        *finished = true;
        timeout->stop();
        callback(std::move(result));
        reply->deleteLater();
    };

    QObject::connect(timeout, &QTimer::timeout, reply, [reply, complete]() {
        reply->abort();
        complete(ServiceResult<MapGeocodeResult>::failure(
            ErrorCodes::InternalError, QStringLiteral("map request timed out")));
    });
    QObject::connect(reply, &QNetworkReply::finished, reply, [reply, complete]() {
        if (reply->error() != QNetworkReply::NoError) {
            complete(ServiceResult<MapGeocodeResult>::failure(
                ErrorCodes::InternalError, QStringLiteral("map request failed")));
            return;
        }
        const QJsonDocument document = QJsonDocument::fromJson(reply->readAll());
        const QJsonObject root = document.object();
        if (!document.isObject() || root.value(QStringLiteral("status")).toInt(-1) != 0) {
            complete(ServiceResult<MapGeocodeResult>::failure(
                ErrorCodes::InternalError, QStringLiteral("map service rejected the address")));
            return;
        }
        const QJsonObject result = root.value(QStringLiteral("result")).toObject();
        const QJsonObject location = result.value(QStringLiteral("location")).toObject();
        if (!location.value(QStringLiteral("lng")).isDouble()
            || !location.value(QStringLiteral("lat")).isDouble()) {
            complete(ServiceResult<MapGeocodeResult>::failure(
                ErrorCodes::InternalError, QStringLiteral("map service returned invalid coordinates")));
            return;
        }
        MapGeocodeResult geocode;
        geocode.formattedAddress = result.value(QStringLiteral("title")).toString();
        geocode.longitude = location.value(QStringLiteral("lng")).toDouble();
        geocode.latitude = location.value(QStringLiteral("lat")).toDouble();
        complete(ServiceResult<MapGeocodeResult>::success(std::move(geocode)));
    });
    timeout->start(5000);
}

void MapAdapter::planRoute(double originLongitude, double originLatitude,
                           double destinationLongitude, double destinationLatitude,
                           const QString &mode, RoutePlanCallback callback)
{
    const QString normalizedMode = mode.trimmed().toUpper();
    if (!validCoordinate(originLongitude, originLatitude)
        || !validCoordinate(destinationLongitude, destinationLatitude)
        || (normalizedMode != QStringLiteral("DRIVING")
            && normalizedMode != QStringLiteral("WALKING"))) {
        callback(ServiceResult<MapRoutePlanResult>::failure(
            ErrorCodes::InvalidSocketMessage, QStringLiteral("invalid route request")));
        return;
    }
    if (!isConfigured()) {
        callback(ServiceResult<MapRoutePlanResult>::failure(
            ErrorCodes::InternalError, QStringLiteral("map service is not configured")));
        return;
    }

    const QString endpoint = normalizedMode == QStringLiteral("DRIVING")
        ? QStringLiteral("https://apis.map.qq.com/ws/direction/v1/driving/")
        : QStringLiteral("https://apis.map.qq.com/ws/direction/v1/walking/");
    QUrl url(endpoint);
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("from"), coordinateText(originLongitude, originLatitude));
    query.addQueryItem(QStringLiteral("to"), coordinateText(destinationLongitude, destinationLatitude));
    query.addQueryItem(QStringLiteral("key"), m_apiKey);
    appendSignature(url, &query);
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("EVCharge-Qt-Server/1.0"));
    auto *reply = m_network.get(request);
    auto *timeout = new QTimer(reply);
    timeout->setSingleShot(true);
    const auto finished = QSharedPointer<bool>::create(false);
    const auto complete = [reply, timeout, finished, callback]
        (ServiceResult<MapRoutePlanResult> result) {
        if (*finished) {
            return;
        }
        *finished = true;
        timeout->stop();
        callback(std::move(result));
        reply->deleteLater();
    };

    QObject::connect(timeout, &QTimer::timeout, reply, [reply, complete]() {
        reply->abort();
        complete(ServiceResult<MapRoutePlanResult>::failure(
            ErrorCodes::InternalError, QStringLiteral("route request timed out")));
    });
    QObject::connect(reply, &QNetworkReply::finished, reply,
        [reply, complete, normalizedMode]() {
            if (reply->error() != QNetworkReply::NoError) {
                complete(ServiceResult<MapRoutePlanResult>::failure(
                    ErrorCodes::InternalError, QStringLiteral("route request failed")));
                return;
            }
            const QJsonDocument document = QJsonDocument::fromJson(reply->readAll());
            const QJsonObject root = document.object();
            const QJsonArray routes = root.value(QStringLiteral("result"))
                .toObject().value(QStringLiteral("routes")).toArray();
            if (!document.isObject() || root.value(QStringLiteral("status")).toInt(-1) != 0
                || routes.isEmpty()) {
                complete(ServiceResult<MapRoutePlanResult>::failure(
                    ErrorCodes::InternalError, QStringLiteral("map service could not plan a route")));
                return;
            }
            const QJsonObject firstRoute = routes.first().toObject();
            const QJsonArray encodedPolyline = firstRoute.value(QStringLiteral("polyline")).toArray();
            if (!firstRoute.value(QStringLiteral("distance")).isDouble()
                || !firstRoute.value(QStringLiteral("duration")).isDouble()
                || encodedPolyline.size() < 2) {
                complete(ServiceResult<MapRoutePlanResult>::failure(
                    ErrorCodes::InternalError, QStringLiteral("map service returned invalid route data")));
                return;
            }

            // 腾讯路线从第3个数开始使用相邻坐标差值（单位 1e-6）压缩。
            double latitude = encodedPolyline.at(0).toDouble();
            double longitude = encodedPolyline.at(1).toDouble();
            if (!validCoordinate(longitude, latitude)) {
                complete(ServiceResult<MapRoutePlanResult>::failure(
                    ErrorCodes::InternalError, QStringLiteral("map service returned invalid route coordinates")));
                return;
            }
            MapRoutePlanResult plan;
            plan.mode = normalizedMode;
            plan.distanceMeters = qRound(firstRoute.value(QStringLiteral("distance")).toDouble());
            plan.durationMinutes = firstRoute.value(QStringLiteral("duration")).toDouble();
            plan.polyline.append({longitude, latitude});
            for (int index = 2; index < encodedPolyline.size(); ++index) {
                if (!encodedPolyline.at(index).isDouble()) {
                    complete(ServiceResult<MapRoutePlanResult>::failure(
                        ErrorCodes::InternalError, QStringLiteral("map service returned malformed route points")));
                    return;
                }
                const double delta = encodedPolyline.at(index).toDouble() / 1000000.0;
                if (index % 2 == 0) {
                    latitude += delta;
                } else {
                    longitude += delta;
                }
                if (!validCoordinate(longitude, latitude) || plan.polyline.size() >= 4096) {
                    complete(ServiceResult<MapRoutePlanResult>::failure(
                        ErrorCodes::InternalError, QStringLiteral("map service returned too many or invalid route points")));
                    return;
                }
                plan.polyline.append({longitude, latitude});
            }
            complete(ServiceResult<MapRoutePlanResult>::success(std::move(plan)));
        });
    timeout->start(8000);
}
