/*
 * 功能：将当前位置和附近站点绘制到腾讯 JavaScript API GL 地图。
 */
#include "stationmapwidget.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QUrl>
#include <QVBoxLayout>
#include <QWebEngineSettings>
#include <QWebEngineView>

#include <cmath>

namespace {

QString mapErrorHtml(const QString &message)
{
    return QStringLiteral(
        "<html><body style='margin:0;padding:24px;font-family:Microsoft YaHei,sans-serif;"
        "background:#f4f8f7;color:#30534a'><h3>附近充电站地图</h3><p>%1</p>"
        "<p style='color:#6b7d78'>站点列表仍可正常浏览。</p></body></html>")
        .arg(message.toHtmlEscaped());
}

QString svgDataUri(const QString &svg)
{
    return QStringLiteral("data:image/svg+xml;charset=UTF-8,")
        + QString::fromLatin1(QUrl::toPercentEncoding(svg));
}

QString pinSvg(const QString &color, const QString &text, bool round = false)
{
    if (round) {
        return QStringLiteral(
            "<svg xmlns='http://www.w3.org/2000/svg' width='44' height='44' viewBox='0 0 44 44'>"
            "<circle cx='22' cy='22' r='18' fill='%1' fill-opacity='.22'/><circle cx='22' cy='22' r='12' "
            "fill='%1' stroke='#fff' stroke-width='4'/><circle cx='22' cy='22' r='3' fill='#fff'/></svg>")
            .arg(color);
    }
    return QStringLiteral(
        "<svg xmlns='http://www.w3.org/2000/svg' width='42' height='52' viewBox='0 0 42 52'>"
        "<path d='M21 1C10 1 2 9 2 20c0 14 19 30 19 30s19-16 19-30C40 9 32 1 21 1z' "
        "fill='%1' stroke='#fff' stroke-width='3'/><circle cx='21' cy='20' r='12' fill='#fff'/>"
        "<text x='21' y='25' text-anchor='middle' font-family='Microsoft YaHei,sans-serif' font-size='13' "
        "font-weight='700' fill='%1'>%2</text></svg>")
        .arg(color, text.toHtmlEscaped());
}

}

StationMapWidget::StationMapWidget(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("stationMapWidget"));
    setMinimumHeight(0);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_mapView = new QWebEngineView(this);
    m_mapView->setMinimumHeight(0);
    m_mapView->settings()->setAttribute(
        QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);
    layout->addWidget(m_mapView, 1);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName(QStringLiteral("stationMapStatus"));
    m_statusLabel->setWordWrap(true);
    layout->addWidget(m_statusLabel);

    connect(m_mapView, &QWebEngineView::urlChanged,
            this, &StationMapWidget::handleUrlChanged);
    reloadMap();
}

void StationMapWidget::setOrigin(const QString &name, double longitude, double latitude)
{
    if (!validCoordinate(longitude, latitude)) {
        return;
    }
    m_originName = name.trimmed().isEmpty() ? QStringLiteral("模拟当前位置") : name.trimmed();
    m_originLongitude = longitude;
    m_originLatitude = latitude;
    reloadMap();
}

void StationMapWidget::setStations(const QJsonArray &stations)
{
    m_stations = stations;
    reloadMap();
}

void StationMapWidget::setSelectedStation(int stationId)
{
    if (m_selectedStationId == stationId) {
        return;
    }
    m_selectedStationId = stationId;
    reloadMap();
}

void StationMapWidget::handleUrlChanged(const QUrl &url)
{
    const QString fragment = url.fragment(QUrl::FullyDecoded);
    if (!fragment.startsWith(QStringLiteral("station="))) {
        return;
    }
    bool ok = false;
    const int stationId = fragment.mid(QStringLiteral("station=").size()).toInt(&ok);
    if (ok && stationId > 0) {
        emit stationSelected(stationId);
    }
}

bool StationMapWidget::validCoordinate(double longitude, double latitude)
{
    return std::isfinite(longitude) && std::isfinite(latitude)
        && longitude >= -180.0 && longitude <= 180.0
        && latitude >= -90.0 && latitude <= 90.0;
}

void StationMapWidget::reloadMap()
{
    if (!m_mapView) {
        return;
    }
    if (!validCoordinate(m_originLongitude, m_originLatitude)) {
        m_mapView->setHtml(mapErrorHtml(QStringLiteral("当前搜索位置坐标无效。")));
        m_statusLabel->setText(QStringLiteral("当前位置不可用"));
        return;
    }
    if (qEnvironmentVariable("TENCENT_MAP_JS_KEY").trimmed().isEmpty()) {
        m_mapView->setHtml(mapErrorHtml(
            QStringLiteral("未配置交互地图 Key，请设置 TENCENT_MAP_JS_KEY。")));
        m_statusLabel->setText(QStringLiteral("地图未配置，仍可浏览站点列表"));
        return;
    }
    m_mapView->setHtml(interactiveMapHtml(), QUrl(QStringLiteral("https://localhost/")));
    m_statusLabel->setText(QStringLiteral("当前位置：%1 · %2 个站点")
        .arg(m_originName).arg(m_stations.size()));
}

QString StationMapWidget::interactiveMapHtml() const
{
    QJsonArray visibleStations;
    for (const QJsonValue &value : m_stations) {
        const QJsonObject station = value.toObject();
        const QJsonValue id = station.value(QStringLiteral("stationId"));
        const QJsonValue longitude = station.value(QStringLiteral("longitude"));
        const QJsonValue latitude = station.value(QStringLiteral("latitude"));
        if (!id.isDouble() || !longitude.isDouble() || !latitude.isDouble()
            || !validCoordinate(longitude.toDouble(), latitude.toDouble())) {
            continue;
        }
        visibleStations.append(QJsonObject{
            {QStringLiteral("stationId"), id.toInt()},
            {QStringLiteral("name"), station.value(QStringLiteral("name")).toString()},
            {QStringLiteral("longitude"), longitude.toDouble()},
            {QStringLiteral("latitude"), latitude.toDouble()},
            {QStringLiteral("availablePileCount"),
             station.value(QStringLiteral("availablePileCount")).toInt()},
            {QStringLiteral("pileCount"), station.value(QStringLiteral("pileCount")).toInt()},
            {QStringLiteral("totalPriceFenPerKwh"),
             station.value(QStringLiteral("totalPriceFenPerKwh")).toInt()}
        });
    }
    const QJsonObject model{
        {QStringLiteral("origin"), QJsonObject{
            {QStringLiteral("name"), m_originName},
            {QStringLiteral("longitude"), m_originLongitude},
            {QStringLiteral("latitude"), m_originLatitude}
        }},
        {QStringLiteral("stations"), visibleStations},
        {QStringLiteral("selectedStationId"), m_selectedStationId}
    };
    const QString dataJson = QString::fromUtf8(
        QJsonDocument(model).toJson(QJsonDocument::Compact));
    const QString encodedKey = QString::fromLatin1(
        QUrl::toPercentEncoding(qEnvironmentVariable("TENCENT_MAP_JS_KEY").trimmed()));
    const QString currentIcon = svgDataUri(pinSvg(QStringLiteral("#1677ff"), {}, true));
    const QString availableIcon = svgDataUri(pinSvg(QStringLiteral("#18a67e"), QStringLiteral("桩")));
    const QString busyIcon = svgDataUri(pinSvg(QStringLiteral("#7b8b87"), QStringLiteral("满")));
    const QString selectedIcon = svgDataUri(pinSvg(QStringLiteral("#ed8a27"), QStringLiteral("选")));

    return QStringLiteral(R"HTML(
<!doctype html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<style>
html,body,#container{margin:0;width:100%;height:100%;overflow:hidden;font-family:"Microsoft YaHei",sans-serif}
#hint{position:fixed;z-index:10;left:12px;top:12px;max-width:calc(100% - 24px);padding:7px 10px;border-radius:9px;
background:rgba(255,255,255,.94);box-shadow:0 1px 6px rgba(0,0,0,.22);font-size:12px;color:#24554a}
#error{display:none;position:fixed;z-index:20;inset:0;padding:24px;background:#fff7f7;color:#a61b29;font-size:14px}
</style></head><body><div id="container"></div><div id="hint">模拟当前位置 · 点击站点图钉查看详情</div><div id="error"></div>
<script>
const model=%1;
const icon={current:'%2',available:'%3',busy:'%4',selected:'%5'};
function error(message){const box=document.getElementById('error');box.textContent=message;box.style.display='block';}
function escapeHtml(text){return String(text).replace(/[&<>'"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;',"'":'&#39;','"':'&quot;'}[c]));}
function initMap(){
  if(typeof TMap==='undefined'){error('腾讯地图脚本未加载，请检查 TENCENT_MAP_JS_KEY、域名白名单和网络。');return;}
  const point=p=>new TMap.LatLng(p.latitude,p.longitude);
  const origin=point(model.origin);
  const selectedStation=model.stations.find(station=>station.stationId===model.selectedStationId);
  const map=new TMap.Map(document.getElementById('container'),{center:selectedStation?point(selectedStation):origin,zoom:selectedStation?16:14,draggable:true,scrollable:true});
  const bounds=new TMap.LatLngBounds(); bounds.extend(origin);
  const styles={
    current:new TMap.MarkerStyle({width:44,height:44,anchor:{x:22,y:22},src:icon.current}),
    available:new TMap.MarkerStyle({width:42,height:52,anchor:{x:21,y:50},src:icon.available}),
    busy:new TMap.MarkerStyle({width:42,height:52,anchor:{x:21,y:50},src:icon.busy}),
    selected:new TMap.MarkerStyle({width:42,height:52,anchor:{x:21,y:50},src:icon.selected})
  };
  const geometries=[{id:'current-location',styleId:'current',position:origin,properties:{title:model.origin.name}}];
  const byId={};
  model.stations.forEach(station=>{
    const position=point(station); bounds.extend(position); byId['station-'+station.stationId]=station;
    const styleId=station.stationId===model.selectedStationId?'selected':(station.availablePileCount>0?'available':'busy');
    geometries.push({id:'station-'+station.stationId,styleId:styleId,position:position,properties:{title:station.name}});
  });
  try{if(!selectedStation&&model.stations.length>0)map.fitBounds(bounds,{padding:46});}catch(ignore){}
  const markers=new TMap.MultiMarker({map:map,styles:styles,geometries:geometries});
  const info=new TMap.InfoWindow({map:map,position:origin,offset:{x:0,y:-42}});info.close();
  const showInfo=station=>{
    info.setPosition(point(station));
    info.setContent('<div style="padding:5px 3px;line-height:1.7"><b>'+escapeHtml(station.name)+'</b><br>综合价 ¥'+(station.totalPriceFenPerKwh/100).toFixed(2)+'/度 · '+station.availablePileCount+'/'+station.pileCount+' 空闲</div>');
    info.open();
  };
  if(selectedStation)showInfo(selectedStation);
  markers.on('click',event=>{
    const id=event.geometry&&event.geometry.id;
    if(!id||id==='current-location')return;
    const station=byId[id];if(!station)return;
    showInfo(station);window.location.hash='station='+station.stationId;
  });
}
const script=document.createElement('script');script.charset='utf-8';script.src='https://map.qq.com/api/gljs?v=1.exp&key=%6';
script.onload=initMap;script.onerror=()=>error('腾讯地图脚本加载失败，请检查网络和 JavaScript API GL Key。');document.head.appendChild(script);
</script></body></html>)HTML")
        .arg(dataJson, currentIcon, availableIcon, busyIcon, selectedIcon, encodedKey);
}
