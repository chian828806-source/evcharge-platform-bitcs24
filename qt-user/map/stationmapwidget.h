/*
 * 功能：首页站点地图。仅绘制服务端返回的当前位置和站点坐标，不直接访问 SQLite 或 WebService。
 * 配置：腾讯 JavaScript API GL Key 只从 TENCENT_MAP_JS_KEY 运行环境读取。
 */
#pragma once

#include <QJsonArray>
#include <QUrl>
#include <QWidget>

class QLabel;
class QWebEngineView;

class StationMapWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit StationMapWidget(QWidget *parent = nullptr);

    void setOrigin(const QString &name, double longitude, double latitude);
    void setStations(const QJsonArray &stations);
    void setSelectedStation(int stationId);

signals:
    // 地图脚本用 URL fragment 回传 stationId，避免用户端额外持有地图 WebService Key。
    void stationSelected(int stationId);

private slots:
    void handleUrlChanged(const QUrl &url);

private:
    static bool validCoordinate(double longitude, double latitude);
    QString interactiveMapHtml() const;
    void reloadMap();

    QWebEngineView *m_mapView = nullptr;
    QLabel *m_statusLabel = nullptr;
    QJsonArray m_stations;
    QString m_originName = QStringLiteral("模拟当前位置");
    double m_originLongitude = 121.538;
    double m_originLatitude = 38.889;
    int m_selectedStationId = -1;
};
