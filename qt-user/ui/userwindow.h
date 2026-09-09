#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QHash>
#include <QMainWindow>

class QLabel;
class QLineEdit;
class MapNavigationPage;
class QPushButton;
class QStackedWidget;
class QVBoxLayout;
class QTimer;
class SocketClient;
struct MapRoute;

class UserWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit UserWindow(QWidget *parent = nullptr);

private:
    enum Page {
        Login,
        Home,
        StationDetail,
        Charging,
        Profile,
        Favorites,
        Navigation
    };

    enum class SessionMode {
        None,
        Real
    };

    SocketClient *m_socketClient = nullptr;
    QStackedWidget *m_pages = nullptr;
    QLabel *m_connectionLabel = nullptr;
    QLabel *m_orderStatusLabel = nullptr;
    QLabel *m_orderHintLabel = nullptr;
    QLabel *m_balanceLabel = nullptr;
    QLabel *m_nicknameLabel = nullptr;
    MapNavigationPage *m_mapNavigationPage = nullptr;
    QLabel *m_profilePhoneLabel = nullptr;
    QLabel *m_orderSummaryLabel = nullptr;
    QLabel *m_chargeStatisticsLabel = nullptr;
    QLabel *m_stationDetailTitle = nullptr;
    QLabel *m_stationDetailSummary = nullptr;
    QPushButton *m_stationFavoriteButton = nullptr;
    QLabel *m_avatarLabel = nullptr;
    QLineEdit *m_phoneEdit = nullptr;
    QPushButton *m_startButton = nullptr;
    QPushButton *m_cancelButton = nullptr;
    QPushButton *m_stopButton = nullptr;
    QPushButton *m_settleButton = nullptr;
    QString m_sessionId;
    SessionMode m_sessionMode = SessionMode::None;
    QString m_loginRequestId;
    QString m_routePlanRequestId;
    QString m_avatarRequestId;
    QString m_avatarRequestPath;
    QString m_avatarPath;
    QHash<QString, QString> m_requestTypes;
    QString m_orderStatus = QStringLiteral("CREATED");
    QJsonObject m_activeOrder;
    QJsonObject m_selectedStation;
    QJsonArray m_nearbyStations;
    QJsonArray m_recommendedStations;
    QJsonArray m_favoriteStations;
    QVBoxLayout *m_stationListLayout = nullptr;
    QVBoxLayout *m_pileListLayout = nullptr;
    QVBoxLayout *m_orderListLayout = nullptr;
    QVBoxLayout *m_favoriteListLayout = nullptr;
    QTimer *m_orderPollTimer = nullptr;
    int m_balanceFenInFen = 0;
    double m_originLongitude = 121.538;
    double m_originLatitude = 38.889;
    QString m_originName = QStringLiteral("默认位置 · 甘井子区");
    QString m_locationDistrict = QStringLiteral("甘井子区");
    Page m_navigationSource = Home;
    Page m_stationDetailSource = Home;

    QWidget *buildLoginPage();
    QWidget *buildHomePage();
    QWidget *buildStationDetailPage();
    QWidget *buildChargingPage();
    QWidget *buildProfilePage();
    QWidget *buildFavoritesPage();
    QWidget *buildNavigationPage();
    QWidget *buildPageHeader(const QString &eyebrow, const QString &title,
                             const QString &subtitle = {});
    QWidget *buildBottomNavigation(Page activePage);
    QWidget *buildStationCard(const QJsonObject &station);
    QWidget *buildPileCard(const QJsonObject &pile);
    QWidget *buildOrderCard(const QString &station, const QString &description,
                            const QString &amount, const QString &status);

    void showPage(Page page);
    void openNavigation(const QJsonObject &station, Page source);
    void requestRoutePlan(const MapRoute &route, bool driving);
    void attemptLogin();
    void setConnected(bool connected);
    void setOrderStatus(const QString &status);
    void showNotice(const QString &message, bool error = false);
    void showRechargeDialog();
    void showRenameDialog();
    void uploadAvatar();
    QString sendRequest(const QString &type, const QJsonObject &payload = {});
    void requestInitialData();
    void requestFavoriteList();
    void toggleFavorite(int stationId);
    void requestActiveOrder();
    void applyUser(const QJsonObject &user);
    void requestAvatar(const QString &avatarPath);
    void resetAvatar();
    void applyOrder(const QJsonObject &order);
    void renderStations(const QJsonArray &stations);
    void renderStationDetail(const QJsonObject &station, const QJsonArray &piles);
    void renderOrders(const QJsonArray &orders);
    void renderFavorites(const QJsonArray &stations);
    void applyFavoriteState(int stationId, bool isFavorite);
    void clearLayout(QVBoxLayout *layout);
    void handleResponse(const QJsonObject &response);
};
