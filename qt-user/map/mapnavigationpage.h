/*
 * 功能：提供可嵌入用户端主窗口的地图导航页。
 * 边界：页面只展示服务端确认的路线 URL；不保存地图 Key，不请求 SQLite 或直接调用地图服务。
 */
#pragma once

#include <QUrl>
#include <QWidget>
#include <QPointF>
#include <QVector>

class QButtonGroup;
class QLabel;
class QPushButton;
class QWebEngineView;

struct MapRoute
{
    QString originName;
    double originLongitude = 0.0;
    double originLatitude = 0.0;
    QString destinationName;
    QString destinationAddress;
    double destinationLongitude = 0.0;
    double destinationLatitude = 0.0;
};

// 仅用于页面展示；路线由服务端解压第三方折线后再发送给客户端。
struct MapRoutePlanPreview
{
    int distanceMeters = 0;
    double durationMinutes = 0.0;
    QVector<QPointF> polyline; // x=longitude, y=latitude
};

class MapNavigationPage final : public QWidget
{
    Q_OBJECT

public:
    enum class TravelMode {
        Driving,
        Walking
    };
    Q_ENUM(TravelMode)

    explicit MapNavigationPage(QWidget *parent = nullptr);

    // 由首页或站点详情页在页面跳转前提供起终点；此操作不会发起外部网络请求。
    bool setRoute(const MapRoute &route);
    // 由后续地图适配层提供已经验证的腾讯路线 URL；页面只负责加载和展示。
    void setNavigationUrl(const QUrl &url);
    void setRoutePlan(const MapRoutePlanPreview &plan);
    void setLoadError(const QString &message);
    MapRoute route() const;
    TravelMode travelMode() const;

signals:
    // 主窗口可据此回到 U02 或 U03；页面不自行决定返回到哪里。
    void backRequested();
    // 没有可用路线或加载失败时，由上层重新请求服务端地图适配层。
    void retryRequested(const MapRoute &route, MapNavigationPage::TravelMode mode);
    void travelModeChanged(MapNavigationPage::TravelMode mode);

private slots:
    void selectDriving();
    void selectWalking();
    void retry();
    void handleLoadFinished(bool ok);

private:
    bool hasValidCoordinates(const MapRoute &route) const;
    void showPlaceholder(const QString &message);
    void updateRouteSummary();
    void setTravelMode(TravelMode mode);

    MapRoute m_route;
    TravelMode m_travelMode = TravelMode::Driving;
    QWebEngineView *m_mapView = nullptr;
    QLabel *m_routeSummary = nullptr;
    QLabel *m_statusLabel = nullptr;
    QPushButton *m_drivingButton = nullptr;
    QPushButton *m_walkingButton = nullptr;
    QPushButton *m_retryButton = nullptr;
    bool m_loadingNavigation = false;
};
