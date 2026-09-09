/*
 * 功能：实现地图导航页的展示状态、路线模式切换和失败重试入口。
 */
#include "mapnavigationpage.h"

#include <QHBoxLayout>
#include <QFrame>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QStringList>
#include <QUrl>
#include <QVBoxLayout>
#include <QWebEngineView>
#include <QWebEngineSettings>

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
    // 出行方式用可选中按钮：选中态高亮（:checked），未选中态普通描边。
    m_drivingButton = new QPushButton(QStringLiteral("驾车"), this);
    m_walkingButton = new QPushButton(QStringLiteral("步行"), this);
    m_drivingButton->setObjectName(QStringLiteral("modeToggle"));
    m_walkingButton->setObjectName(QStringLiteral("modeToggle"));
    m_drivingButton->setCheckable(true);
    m_walkingButton->setCheckable(true);
    auto *backButton = new QPushButton(QStringLiteral("返回"), this);
    toolbar->addWidget(m_drivingButton);
    toolbar->addWidget(m_walkingButton);
    toolbar->addStretch();
    toolbar->addWidget(backButton);
    layout->addLayout(toolbar);

    auto *metricsCard = new QFrame(this);
    metricsCard->setObjectName(QStringLiteral("routeMetricsCard"));
    auto *metricsLayout = new QVBoxLayout(metricsCard);
    metricsLayout->setContentsMargins(14, 10, 14, 10);
    auto *metricsTitle = new QLabel(QStringLiteral("路线信息"), metricsCard);
    metricsTitle->setObjectName(QStringLiteral("routeMetricsTitle"));
    m_routeMetrics = new QLabel(metricsCard);
    m_routeMetrics->setObjectName(QStringLiteral("routeMetricsValue"));
    m_routeMetrics->setWordWrap(true);
    metricsLayout->addWidget(metricsTitle);
    metricsLayout->addWidget(m_routeMetrics);
    layout->addWidget(metricsCard);

    m_mapView = new QWebEngineView(this);
    // 页面内容由本程序生成，但腾讯 JS GL SDK 需要从 HTTPS 加载。
    m_mapView->settings()->setAttribute(
        QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);
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
    showRouteMetricsHint(QStringLiteral("选择%1路线后，将显示距离和预计时间。")
        .arg(modeName(m_travelMode)));
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
    updateRouteMetrics(plan);

    if (qEnvironmentVariable("TENCENT_MAP_JS_KEY").trimmed().isEmpty()) {
        setLoadError(QStringLiteral(
            "未配置交互地图 Key。请在 qt-user 的运行环境中设置 TENCENT_MAP_JS_KEY。"));
        return;
    }

    m_loadingNavigation = true;
    // 用固定的本地开发来源，便于 JavaScript API GL Key 配置 localhost 域名白名单。
    m_mapView->setHtml(interactiveMapHtml(plan), QUrl(QStringLiteral("https://localhost/")));
    m_statusLabel->setText(QStringLiteral("正在加载可交互的腾讯%1地图：%2 米，约 %3 分钟。")
        .arg(modeName(m_travelMode)).arg(plan.distanceMeters).arg(plan.durationMinutes, 0, 'f', 0));
    m_retryButton->setVisible(false);
}

QString MapNavigationPage::interactiveMapHtml(const MapRoutePlanPreview &plan) const
{
    QJsonArray points;
    for (const QPointF &point : plan.polyline) {
        points.append(QJsonObject{
            {QStringLiteral("longitude"), point.x()},
            {QStringLiteral("latitude"), point.y()}
        });
    }
    const QString routeJson = QString::fromUtf8(
        QJsonDocument(points).toJson(QJsonDocument::Compact));
    const QString encodedKey = QString::fromLatin1(
        QUrl::toPercentEncoding(qEnvironmentVariable("TENCENT_MAP_JS_KEY").trimmed()));
    const QString mode = modeName(m_travelMode).toHtmlEscaped();

    return QStringLiteral(R"HTML(
<!doctype html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<style>
html,body,#container{margin:0;width:100%;height:100%;overflow:hidden;font-family:"Microsoft YaHei",sans-serif}
#hint{position:fixed;z-index:10;left:12px;top:12px;padding:7px 10px;border-radius:8px;
background:rgba(255,255,255,.94);box-shadow:0 1px 6px rgba(0,0,0,.2);font-size:12px;color:#1d4e45}
#error{display:none;position:fixed;z-index:20;inset:0;padding:24px;background:#fff7f7;color:#a61b29;font-size:14px}
</style></head><body>
<div id="container"></div><div id="hint">腾讯地图 · %1路线，可拖动、滚轮缩放</div><div id="error"></div>
<script>
const route = %2;
function showError(message) {
  const error = document.getElementById('error'); error.textContent = message; error.style.display = 'block';
}
function initMap() {
  if (typeof TMap === 'undefined') { showError('腾讯地图脚本未加载，请检查 TENCENT_MAP_JS_KEY、域名白名单和网络。'); return; }
  if (!Array.isArray(route) || route.length < 2) { showError('路线数据不完整。'); return; }
  const toLatLng = p => new TMap.LatLng(p.latitude, p.longitude);
  const map = new TMap.Map(document.getElementById('container'), {
    center: toLatLng(route[0]), zoom: 13, draggable: true, scrollable: true
  });
  const points = route.map(toLatLng);
  const bounds = new TMap.LatLngBounds(); points.forEach(point => bounds.extend(point));
  try { map.fitBounds(bounds, {padding: 52}); } catch (ignore) { }
  new TMap.MultiPolyline({
    map: map,
    styles: { route: new TMap.PolylineStyle({color:'#1677ff',width:7,borderWidth:2,borderColor:'#ffffff',lineCap:'round'}) },
    geometries: [{id:'planned-route',styleId:'route',paths:points}]
  });

  // 起、终点使用不同颜色和文字；路线箭头由相邻坐标计算，无需服务端额外字段。
  const svgDataUri = svg => 'data:image/svg+xml;charset=UTF-8,' + encodeURIComponent(svg);
  const pinIcon = (color, text) => svgDataUri(
    '<svg xmlns="http://www.w3.org/2000/svg" width="42" height="52" viewBox="0 0 42 52">'
    + '<path d="M21 1C10 1 2 9 2 20c0 14 19 30 19 30s19-16 19-30C40 9 32 1 21 1z" fill="' + color + '" stroke="#fff" stroke-width="3"/>'
    + '<circle cx="21" cy="20" r="12" fill="#fff"/>'
    + '<text x="21" y="25" text-anchor="middle" font-family="Microsoft YaHei,sans-serif" font-size="15" font-weight="700" fill="' + color + '">' + text + '</text></svg>');
  const arrowGlyphs = ['↑','↗','→','↘','↓','↙','←','↖'];
  const arrowIcon = glyph => svgDataUri(
    '<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24">'
    + '<circle cx="12" cy="12" r="10" fill="#1677ff" stroke="#fff" stroke-width="2"/>'
    + '<text x="12" y="17" text-anchor="middle" font-family="sans-serif" font-size="16" font-weight="700" fill="#fff">' + glyph + '</text></svg>');
  const distanceMeters = (a, b) => {
    const rad = Math.PI / 180;
    const dLat = (b.latitude - a.latitude) * rad;
    const dLon = (b.longitude - a.longitude) * rad;
    const h = Math.sin(dLat / 2) ** 2
      + Math.cos(a.latitude * rad) * Math.cos(b.latitude * rad) * Math.sin(dLon / 2) ** 2;
    return 6371000 * 2 * Math.atan2(Math.sqrt(h), Math.sqrt(1 - h));
  };
  const directionGlyph = (a, b) => {
    const latitude = ((a.latitude + b.latitude) / 2) * Math.PI / 180;
    const east = (b.longitude - a.longitude) * Math.cos(latitude);
    const north = b.latitude - a.latitude;
    const index = (Math.round(Math.atan2(east, north) / (Math.PI / 4)) + 8) % 8;
    return arrowGlyphs[index];
  };
  const markerStyles = {
    start: new TMap.MarkerStyle({width:42,height:52,anchor:{x:21,y:50},src:pinIcon('#1677ff','起')}),
    end: new TMap.MarkerStyle({width:42,height:52,anchor:{x:21,y:50},src:pinIcon('#16a36a','终')})
  };
  arrowGlyphs.forEach(glyph => {
    markerStyles['arrow-' + glyph.charCodeAt(0)] = new TMap.MarkerStyle({
      width:24, height:24, anchor:{x:12,y:12}, src:arrowIcon(glyph)
    });
  });
  const arrowGeometries = [];
  let totalDistance = 0;
  for (let index = 1; index < route.length; index += 1) totalDistance += distanceMeters(route[index - 1], route[index]);
  // 目标显示约 5–7 个箭头；短路线至少保留一个，长路线最多八个，避免遮住道路。
  const arrowSpacing = Math.max(260, totalDistance / 6);
  let travelled = 0;
  for (let index = 1; index < route.length && arrowGeometries.length < 8; index += 1) {
    travelled += distanceMeters(route[index - 1], route[index]);
    if (travelled >= arrowSpacing) {
      const glyph = directionGlyph(route[index - 1], route[index]);
      arrowGeometries.push({id:'route-arrow-' + index,
        styleId:'arrow-' + glyph.charCodeAt(0), position:toLatLng(route[index])});
      travelled = 0;
    }
  }
  new TMap.MultiMarker({
    map: map,
    styles: markerStyles,
    geometries: [
      {id:'route-start',styleId:'start',position:points[0],properties:{title:'起点'}},
      {id:'route-end',styleId:'end',position:points[points.length-1],properties:{title:'终点'}},
      ...arrowGeometries
    ]
  });
}
const script = document.createElement('script');
script.charset = 'utf-8'; script.src = 'https://map.qq.com/api/gljs?v=1.exp&key=%3';
script.onload = initMap; script.onerror = () => showError('腾讯地图脚本加载失败，请检查网络和 JavaScript API GL Key。');
document.head.appendChild(script);
</script></body></html>)HTML")
        .arg(mode, routeJson, encodedKey);
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
    // 已处于驾车模式时忽略重复点击，避免无意义的重新请求。
    if (m_travelMode == TravelMode::Driving) {
        m_drivingButton->setChecked(true);
        return;
    }
    setTravelMode(TravelMode::Driving);
    retry();
}

void MapNavigationPage::selectWalking()
{
    if (m_travelMode == TravelMode::Walking) {
        m_walkingButton->setChecked(true);
        return;
    }
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

void MapNavigationPage::showRouteMetricsHint(const QString &message)
{
    if (m_routeMetrics) {
        m_routeMetrics->setText(message);
    }
}

void MapNavigationPage::updateRouteMetrics(const MapRoutePlanPreview &plan)
{
    const QString distance = plan.distanceMeters >= 1000
        ? QStringLiteral("%1 公里").arg(plan.distanceMeters / 1000.0, 0, 'f', 1)
        : QStringLiteral("%1 米").arg(plan.distanceMeters);
    const int roundedMinutes = qRound(plan.durationMinutes);
    const QString duration = roundedMinutes >= 60
        ? QStringLiteral("%1 小时 %2 分钟").arg(roundedMinutes / 60).arg(roundedMinutes % 60)
        : QStringLiteral("%1 分钟").arg(roundedMinutes);
    showRouteMetricsHint(QStringLiteral("%1 · %2 · 预计 %3")
        .arg(modeName(m_travelMode), distance, duration));
}

void MapNavigationPage::setTravelMode(TravelMode mode)
{
    if (m_travelMode == mode && m_drivingButton->isChecked() == (mode == TravelMode::Driving)) {
        return;
    }
    m_travelMode = mode;
    // 选中项保持可点击并显示高亮，不再用禁用（灰色）表达当前模式。
    m_drivingButton->setChecked(mode == TravelMode::Driving);
    m_walkingButton->setChecked(mode == TravelMode::Walking);
    if (hasValidCoordinates(m_route)) {
        showRouteMetricsHint(QStringLiteral("正在重新规划%1路线…").arg(modeName(mode)));
    } else {
        showRouteMetricsHint(QStringLiteral("选择路线后，将显示%1的距离和预计时间。")
            .arg(modeName(mode)));
    }
    emit travelModeChanged(mode);
}
