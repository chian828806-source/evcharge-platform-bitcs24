/*
 * 功能：实现地图导航页的展示状态、路线模式切换和失败重试入口。
 */
#include "mapnavigationpage.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStringList>
#include <QVBoxLayout>
#include <QWebEngineView>

#include <cmath>

namespace {

QString escapeHtml(const QString &text)
{
    return text.toHtmlEscaped();
}

QString modeName(MapNavigationPage::TravelMode mode)
{
    return mode == MapNavigationPage::TravelMode::Driving
        ? QStringLiteral("驾车") : QStringLiteral("步行");
}

}

MapNavigationPage::MapNavigationPage(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 16, 20, 16);
    layout->setSpacing(12);

    auto *title = new QLabel(QStringLiteral("地图导航"), this);
    title->setObjectName(QStringLiteral("mapPageTitle"));
    layout->addWidget(title);

    m_routeSummary = new QLabel(this);
    m_routeSummary->setWordWrap(true);
    layout->addWidget(m_routeSummary);

    auto *toolbar = new QHBoxLayout();
    m_drivingButton = new QPushButton(QStringLiteral("驾车"), this);
    m_walkingButton = new QPushButton(QStringLiteral("步行"), this);
    auto *backButton = new QPushButton(QStringLiteral("返回"), this);
    toolbar->addWidget(m_drivingButton);
    toolbar->addWidget(m_walkingButton);
    toolbar->addStretch();
    toolbar->addWidget(backButton);
    layout->addLayout(toolbar);

    m_mapView = new QWebEngineView(this);
    m_mapView->setMinimumHeight(360);
    layout->addWidget(m_mapView, 1);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    m_retryButton = new QPushButton(QStringLiteral("重试加载"), this);
    auto *statusLayout = new QHBoxLayout();
    statusLayout->addWidget(m_statusLabel, 1);
    statusLayout->addWidget(m_retryButton);
    layout->addLayout(statusLayout);

    connect(m_drivingButton, &QPushButton::clicked,
            this, &MapNavigationPage::selectDriving);
    connect(m_walkingButton, &QPushButton::clicked,
            this, &MapNavigationPage::selectWalking);
    connect(backButton, &QPushButton::clicked,
            this, &MapNavigationPage::backRequested);
    connect(m_retryButton, &QPushButton::clicked,
            this, &MapNavigationPage::retry);
    connect(m_mapView, &QWebEngineView::loadFinished,
            this, &MapNavigationPage::handleLoadFinished);

    setTravelMode(TravelMode::Driving);
    showPlaceholder(QStringLiteral("请从首页或充电站详情页选择导航目标。"));
}

bool MapNavigationPage::setRoute(const MapRoute &route)
{
    if (!hasValidCoordinates(route)) {
        setLoadError(QStringLiteral("起点或终点坐标无效，无法规划路线。"));
        return false;
    }
    m_route = route;
    updateRouteSummary();
    showPlaceholder(QStringLiteral("路线信息已准备好，正在等待地图服务返回导航页面。"));
    return true;
}

void MapNavigationPage::setNavigationUrl(const QUrl &url)
{
    if (!url.isValid() || url.scheme() != QStringLiteral("https")) {
        setLoadError(QStringLiteral("地图服务返回的导航地址无效。"));
        return;
    }
    m_statusLabel->setText(QStringLiteral("正在加载%1路线…").arg(modeName(m_travelMode)));
    m_retryButton->setVisible(false);
    m_loadingNavigation = true;
    m_mapView->load(url);
}

void MapNavigationPage::setRoutePlan(const MapRoutePlanPreview &plan)
{
    if (plan.distanceMeters < 0 || plan.durationMinutes < 0.0 || plan.polyline.size() < 2) {
        setLoadError(QStringLiteral("地图服务返回的路线数据不完整。"));
        return;
    }

    double minLongitude = plan.polyline.first().x();
    double maxLongitude = minLongitude;
    double minLatitude = plan.polyline.first().y();
    double maxLatitude = minLatitude;
    for (const QPointF &point : plan.polyline) {
        minLongitude = qMin(minLongitude, point.x());
        maxLongitude = qMax(maxLongitude, point.x());
        minLatitude = qMin(minLatitude, point.y());
        maxLatitude = qMax(maxLatitude, point.y());
    }
    const double longitudeRange = qMax(maxLongitude - minLongitude, 0.000001);
    const double latitudeRange = qMax(maxLatitude - minLatitude, 0.000001);
    QStringList svgPoints;
    for (const QPointF &point : plan.polyline) {
        const double x = 48.0 + (point.x() - minLongitude) / longitudeRange * 704.0;
        const double y = 372.0 - (point.y() - minLatitude) / latitudeRange * 324.0;
        svgPoints.append(QStringLiteral("%1,%2").arg(x, 0, 'f', 1).arg(y, 0, 'f', 1));
    }
    const QString firstPoint = svgPoints.first();
    const QString lastPoint = svgPoints.last();
    const QString mode = modeName(m_travelMode);
    const QString html = QStringLiteral(
        "<html><body style='margin:0;padding:18px;background:#f7fafc;"
        "font-family:Microsoft YaHei,sans-serif;color:#102a43;'>"
        "<h2 style='margin:0 0 8px;'>腾讯路线规划结果</h2>"
        "<p style='margin:0 0 14px;color:#486581;'>%1 · 约 %2 公里 · 约 %3 分钟</p>"
        "<svg viewBox='0 0 800 420' style='width:100%;height:auto;background:#e7f2ff;"
        "border-radius:14px;'>"
        "<path d='M 40 42 H 760 M 40 126 H 760 M 40 210 H 760 M 40 294 H 760 M 40 378 H 760'"
        " stroke='#c9dff4' stroke-width='2'/>"
        "<polyline points='%4' fill='none' stroke='#1677ff' stroke-width='8'"
        " stroke-linecap='round' stroke-linejoin='round'/>"
        "<circle cx='%5' cy='%6' r='12' fill='#23a55a' stroke='white' stroke-width='5'/>"
        "<circle cx='%7' cy='%8' r='12' fill='#e5484d' stroke='white' stroke-width='5'/>"
        "</svg><p style='color:#627d98;'>路线由服务端调用腾讯地图规划；此页面只展示路线，"
        "不会保存 WebService Key。</p></body></html>")
        .arg(mode)
        .arg(plan.distanceMeters / 1000.0, 0, 'f', 1)
        .arg(plan.durationMinutes, 0, 'f', 0)
        .arg(svgPoints.join(QLatin1Char(' ')))
        .arg(firstPoint.section(QLatin1Char(','), 0, 0))
        .arg(firstPoint.section(QLatin1Char(','), 1, 1))
        .arg(lastPoint.section(QLatin1Char(','), 0, 0))
        .arg(lastPoint.section(QLatin1Char(','), 1, 1));

    m_loadingNavigation = false;
    m_mapView->setHtml(html);
    m_statusLabel->setText(QStringLiteral("已获取%1路线：%2 米，约 %3 分钟。")
        .arg(mode).arg(plan.distanceMeters).arg(plan.durationMinutes, 0, 'f', 0));
    m_retryButton->setVisible(false);
}

void MapNavigationPage::setLoadError(const QString &message)
{
    showPlaceholder(message.isEmpty() ? QStringLiteral("地图加载失败，请重试。") : message);
}

MapRoute MapNavigationPage::route() const
{
    return m_route;
}

MapNavigationPage::TravelMode MapNavigationPage::travelMode() const
{
    return m_travelMode;
}

void MapNavigationPage::selectDriving()
{
    setTravelMode(TravelMode::Driving);
    retry();
}

void MapNavigationPage::selectWalking()
{
    setTravelMode(TravelMode::Walking);
    retry();
}

void MapNavigationPage::retry()
{
    if (!hasValidCoordinates(m_route)) {
        setLoadError(QStringLiteral("没有可重试的路线坐标。"));
        return;
    }
    emit retryRequested(m_route, m_travelMode);
}

void MapNavigationPage::handleLoadFinished(bool ok)
{
    // setHtml() 的占位页也会发出 loadFinished，不能把它误认成真实路线加载成功。
    if (!m_loadingNavigation) {
        return;
    }
    m_loadingNavigation = false;
    if (ok) {
        m_statusLabel->setText(QStringLiteral("已加载%1路线。").arg(modeName(m_travelMode)));
        m_retryButton->setVisible(false);
        return;
    }
    setLoadError(QStringLiteral("腾讯地图页面加载失败，请检查网络后重试。"));
}

bool MapNavigationPage::hasValidCoordinates(const MapRoute &route) const
{
    const auto valid = [](double longitude, double latitude) {
        return std::isfinite(longitude) && std::isfinite(latitude)
            && longitude >= -180.0 && longitude <= 180.0
            && latitude >= -90.0 && latitude <= 90.0;
    };
    return valid(route.originLongitude, route.originLatitude)
        && valid(route.destinationLongitude, route.destinationLatitude);
}

void MapNavigationPage::showPlaceholder(const QString &message)
{
    m_loadingNavigation = false;
    const QString routeText = m_route.destinationName.isEmpty()
        ? QStringLiteral("尚未选择目的地")
        : QStringLiteral("目的地：%1").arg(escapeHtml(m_route.destinationName));
    m_mapView->setHtml(QStringLiteral(
        "<html><body style='font-family:Microsoft YaHei,sans-serif;padding:24px;'>"
        "<h2>路线导航</h2><p>%1</p><p>%2</p>"
        "<p style='color:#666'>地图路线将由服务端地图适配层提供，页面不会保存地图 Key。</p>"
        "</body></html>").arg(routeText, escapeHtml(message)));
    m_statusLabel->setText(message);
    m_retryButton->setVisible(hasValidCoordinates(m_route));
}

void MapNavigationPage::updateRouteSummary()
{
    m_routeSummary->setText(QStringLiteral("起点：%1（%2, %3）\n终点：%4%5（%6, %7）")
        .arg(m_route.originName.isEmpty() ? QStringLiteral("当前位置") : m_route.originName)
        .arg(m_route.originLongitude, 0, 'f', 6)
        .arg(m_route.originLatitude, 0, 'f', 6)
        .arg(m_route.destinationName)
        .arg(m_route.destinationAddress.isEmpty()
            ? QString() : QStringLiteral("，%1").arg(m_route.destinationAddress))
        .arg(m_route.destinationLongitude, 0, 'f', 6)
        .arg(m_route.destinationLatitude, 0, 'f', 6));
}

void MapNavigationPage::setTravelMode(TravelMode mode)
{
    if (m_travelMode == mode && m_drivingButton->isEnabled() == (mode != TravelMode::Driving)) {
        return;
    }
    m_travelMode = mode;
    m_drivingButton->setEnabled(mode != TravelMode::Driving);
    m_walkingButton->setEnabled(mode != TravelMode::Walking);
    emit travelModeChanged(mode);
}
