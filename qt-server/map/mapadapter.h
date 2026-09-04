/*
 * 功能：异步调用腾讯地图地理编码和路线规划服务。
 * 边界：地图 WebService Key/SK 仅存在服务端进程配置中；本类不处理 Socket、会话或数据库。
 */
#pragma once

#include "common/serviceresult.h"

#include <QNetworkAccessManager>
#include <QVector>

#include <functional>

class QUrl;
class QUrlQuery;

struct MapGeocodeResult
{
    QString formattedAddress;
    double longitude = 0.0;
    double latitude = 0.0;
};

struct MapRoutePoint
{
    double longitude = 0.0;
    double latitude = 0.0;
};

// 服务端已解压腾讯路线折线，客户端不必理解第三方的压缩格式。
struct MapRoutePlanResult
{
    QString mode;
    int distanceMeters = 0;
    double durationMinutes = 0.0;
    QVector<MapRoutePoint> polyline;
};

class MapAdapter
{
public:
    using Callback = std::function<void(ServiceResult<MapGeocodeResult>)>;
    using RoutePlanCallback = std::function<void(ServiceResult<MapRoutePlanResult>)>;

    explicit MapAdapter(const QString &apiKey = {}, const QString &signingSecret = {});

    bool isConfigured() const;
    // 立即返回；完成结果经 callback 回到 Qt 事件循环，不阻塞 TCP 读取线程。
    void geocode(const QString &district, const QString &address, Callback callback);
    void planRoute(double originLongitude, double originLatitude,
                   double destinationLongitude, double destinationLatitude,
                   const QString &mode, RoutePlanCallback callback);

private:
    void appendSignature(QUrl &url, QUrlQuery *query) const;

    QString m_apiKey;
    QString m_signingSecret;
    QNetworkAccessManager m_network;
};
