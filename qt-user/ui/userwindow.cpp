#include "userwindow.h"

#include "map/mapnavigationpage.h"
#include "network/socketclient.h"
#include "shared/protocol/messagetypes.h"

#include <QColor>
#include <QByteArray>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QList>
#include <QJsonArray>
#include <QLineEdit>
#include <QMessageBox>
#include <QMetaObject>
#include <QPair>
#include <QPixmap>
#include <QProgressBar>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QScrollArea>
#include <QStackedWidget>
#include <QStringList>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>
#include <QtMath>

namespace {

QLabel *makeLabel(const QString &text, const char *role = nullptr)
{
    auto *result = new QLabel(text);
    result->setWordWrap(true);
    if (role) {
        result->setProperty("role", role);
    }
    return result;
}

QPushButton *makeButton(const QString &text, const char *kind = "primary")
{
    auto *result = new QPushButton(text);
    result->setCursor(Qt::PointingHandCursor);
    result->setProperty("kind", kind);
    return result;
}

QFrame *makeCard()
{
    auto *result = new QFrame;
    result->setObjectName(QStringLiteral("card"));
    auto *shadow = new QGraphicsDropShadowEffect(result);
    shadow->setBlurRadius(20);
    shadow->setOffset(0, 6);
    shadow->setColor(QColor(15, 23, 42, 25));
    result->setGraphicsEffect(shadow);
    return result;
}

QScrollArea *makeScrollArea(QWidget *content)
{
    auto *scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setWidget(content);
    return scrollArea;
}

QString displayMoney(int amountFen)
{
    return QStringLiteral("¥%1").arg(amountFen / 100.0, 0, 'f', 2);
}

QPixmap circularAvatar(const QPixmap &source, const QSize &size)
{
    QPixmap result(size);
    result.fill(Qt::transparent);

    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing, true);
    QPainterPath clip;
    clip.addEllipse(result.rect());
    painter.setClipPath(clip);
    const QPixmap scaled = source.scaled(size, Qt::KeepAspectRatioByExpanding,
                                         Qt::SmoothTransformation);
    painter.drawPixmap((size.width() - scaled.width()) / 2,
                       (size.height() - scaled.height()) / 2, scaled);
    return result;
}

QString pileTypeText(const QString &type)
{
    if (type == QStringLiteral("FAST")) return QStringLiteral("快充");
    if (type == QStringLiteral("SLOW")) return QStringLiteral("慢充");
    return type.isEmpty() ? QStringLiteral("未知类型") : type;
}

QString pileStatusText(const QString &status)
{
    static const QHash<QString, QString> labels{
        {QStringLiteral("AVAILABLE"), QStringLiteral("空闲")},
        {QStringLiteral("RESERVED"), QStringLiteral("已预约")},
        {QStringLiteral("CHARGING"), QStringLiteral("充电中")},
        {QStringLiteral("FAULT"), QStringLiteral("故障")},
        {QStringLiteral("OFFLINE"), QStringLiteral("离线")},
        {QStringLiteral("RESTARTING"), QStringLiteral("重启中")}
    };
    return labels.value(status, status.isEmpty() ? QStringLiteral("未知状态") : status);
}

QString orderStatusText(const QString &status)
{
    static const QHash<QString, QString> labels{
        {QStringLiteral("CREATED"), QStringLiteral("待开始")},
        {QStringLiteral("CHARGING"), QStringLiteral("充电中")},
        {QStringLiteral("PENDING_PAYMENT"), QStringLiteral("待结算")},
        {QStringLiteral("COMPLETED"), QStringLiteral("已完成")},
        {QStringLiteral("CANCELLED"), QStringLiteral("已取消")}
    };
    return labels.value(status, status.isEmpty() ? QStringLiteral("未知状态") : status);
}

QString userStatusText(const QString &status)
{
    if (status == QStringLiteral("NORMAL")) return QStringLiteral("正常");
    if (status == QStringLiteral("FROZEN")) return QStringLiteral("已冻结");
    return status.isEmpty() ? QStringLiteral("未知") : status;
}

bool confirmUserAction(QWidget *parent, const QString &title, const QString &message,
                       const QString &detail, const QString &confirmText,
                       bool dangerous = false)
{
    QDialog dialog(parent, Qt::Dialog | Qt::FramelessWindowHint);
    dialog.setObjectName(QStringLiteral("userConfirmDialog"));
    dialog.setModal(true);
    dialog.setAttribute(Qt::WA_TranslucentBackground);
    dialog.setMinimumWidth(390);

    auto *outerLayout = new QVBoxLayout(&dialog);
    outerLayout->setContentsMargins(18, 18, 18, 18);
    auto *card = new QFrame(&dialog);
    card->setObjectName(QStringLiteral("confirmCard"));
    card->setProperty("dangerous", dangerous);
    auto *shadow = new QGraphicsDropShadowEffect(card);
    shadow->setBlurRadius(36);
    shadow->setOffset(0, 12);
    shadow->setColor(QColor(15, 44, 37, 70));
    card->setGraphicsEffect(shadow);
    outerLayout->addWidget(card);

    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(28, 20, 28, 26);
    layout->setSpacing(12);
    auto *topRow = new QHBoxLayout;
    topRow->addStretch();
    auto *closeButton = makeButton(QStringLiteral("×"), "dialogClose");
    closeButton->setFixedSize(36, 36);
    closeButton->setAccessibleName(QStringLiteral("关闭弹窗"));
    topRow->addWidget(closeButton);
    layout->addLayout(topRow);

    auto *icon = makeLabel(dangerous ? QStringLiteral("!") : QStringLiteral("✓"),
                           dangerous ? "dialogDangerIcon" : "dialogIcon");
    icon->setAlignment(Qt::AlignCenter);
    icon->setFixedSize(58, 58);
    layout->addWidget(icon, 0, Qt::AlignHCenter);

    auto *titleLabel = makeLabel(title, "dialogTitle");
    titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(titleLabel);
    auto *messageLabel = makeLabel(message, "dialogMessage");
    messageLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(messageLabel);
    auto *detailLabel = makeLabel(detail, "dialogDetail");
    detailLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(detailLabel);
    layout->addSpacing(8);

    auto *confirmButton = makeButton(confirmText,
        dangerous ? "dialogDanger" : "dialogPrimary");
    auto *cancelButton = makeButton(QStringLiteral("暂不操作"), "dialogSecondary");
    confirmButton->setMinimumHeight(50);
    cancelButton->setMinimumHeight(46);
    layout->addWidget(confirmButton);
    layout->addWidget(cancelButton);

    QObject::connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::reject);
    QObject::connect(cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);
    QObject::connect(confirmButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    cancelButton->setProperty("kind", "dialogSecondary");
    cancelButton->setDefault(true);
    return dialog.exec() == QDialog::Accepted;
}

} // namespace

UserWindow::UserWindow(QWidget *parent)
    : QMainWindow(parent),
      m_socketClient(new SocketClient(this))
{
    setWindowTitle(QStringLiteral("EVCharge · 充电用户端"));
    setMinimumSize(430, 620);
    resize(460, 720);

    auto *root = new QWidget;
    root->setObjectName(QStringLiteral("appRoot"));
    auto *rootLayout = new QVBoxLayout(root);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    auto *connectionBar = new QFrame;
    connectionBar->setObjectName(QStringLiteral("connectionBar"));
    auto *connectionLayout = new QHBoxLayout(connectionBar);
    connectionLayout->setContentsMargins(20, 7, 20, 7);
    m_connectionLabel = makeLabel(QStringLiteral("● 正在连接服务…"), "connection");
    connectionLayout->addWidget(m_connectionLabel);
    connectionLayout->addStretch();
    auto *connectButton = makeButton(QStringLiteral("重新连接"), "link");
    connectionLayout->addWidget(connectButton);
    rootLayout->addWidget(connectionBar);

    m_pages = new QStackedWidget;
    m_pages->addWidget(buildLoginPage());
    m_pages->addWidget(buildHomePage());
    m_pages->addWidget(buildStationDetailPage());
    m_pages->addWidget(buildChargingPage());
    m_pages->addWidget(buildProfilePage());
    m_pages->addWidget(buildNavigationPage());
    rootLayout->addWidget(m_pages, 1);
    setCentralWidget(root);

    connect(connectButton, &QPushButton::clicked, this, [this]() {
        if (m_socketClient->isConnected()) {
            showNotice(QStringLiteral("服务连接正常"));
            return;
        }
        m_connectionLabel->setText(QStringLiteral("● 正在连接 127.0.0.1:18080…"));
        m_socketClient->connectToServer(QStringLiteral("127.0.0.1"), 18080);
    });
    connect(m_socketClient, &SocketClient::connected, this,
            [this]() { setConnected(true); });
    connect(m_socketClient, &SocketClient::disconnected, this,
            [this]() { setConnected(false); });
    connect(m_socketClient, &SocketClient::socketError, this,
            [this](const QString &message) {
                setConnected(false);
                showNotice(QStringLiteral("连接失败：%1").arg(message), true);
            });
    connect(m_socketClient, &SocketClient::requestTimedOut, this,
            [this](const QString &requestId, const QString &type) {
                m_requestTypes.remove(requestId);
                if (requestId == m_loginRequestId) {
                    m_loginRequestId.clear();
                }
                if (requestId == m_routePlanRequestId) {
                    m_routePlanRequestId.clear();
                    if (m_mapNavigationPage) {
                        m_mapNavigationPage->setLoadError(
                            QStringLiteral("路线规划超时，请检查网络后重试。"));
                    }
                }
                if (requestId == m_avatarRequestId) {
                    m_avatarRequestId.clear();
                    m_avatarRequestPath.clear();
                }
                if (type == MessageTypes::StationDetailGet && m_pileListLayout) {
                    clearLayout(m_pileListLayout);
                    m_stationDetailSummary->setText(QStringLiteral("站点详情加载超时"));
                    m_pileListLayout->addWidget(makeLabel(
                        QStringLiteral("暂时无法获取电桩信息，请返回首页后重试。"), "hint"));
                }
                showNotice(QStringLiteral("请求超时，请检查服务后重试"), true);
            });
    connect(m_socketClient, &SocketClient::requestFailed, this,
            [this](const QString &requestId, const QString &type, const QString &message) {
                if (requestId == m_loginRequestId) {
                    m_loginRequestId.clear();
                }
                m_requestTypes.remove(requestId);
                if (requestId == m_routePlanRequestId) {
                    m_routePlanRequestId.clear();
                    if (m_mapNavigationPage) {
                        m_mapNavigationPage->setLoadError(message);
                    }
                }
                if (requestId == m_avatarRequestId) {
                    m_avatarRequestId.clear();
                    m_avatarRequestPath.clear();
                }
                if (type == MessageTypes::StationDetailGet && m_pileListLayout) {
                    clearLayout(m_pileListLayout);
                    m_stationDetailSummary->setText(QStringLiteral("站点详情加载失败"));
                    m_pileListLayout->addWidget(makeLabel(
                        QStringLiteral("暂时无法获取电桩信息，请返回首页后重试。"), "hint"));
                }
                showNotice(QStringLiteral("请求失败：%1").arg(message), true);
            });
    connect(m_socketClient, &SocketClient::responseReceived,
            this, &UserWindow::handleResponse);

    m_orderPollTimer = new QTimer(this);
    m_orderPollTimer->setInterval(1000);
    connect(m_orderPollTimer, &QTimer::timeout, this, &UserWindow::requestActiveOrder);
    m_socketClient->connectToServer(QStringLiteral("127.0.0.1"), 18080);
}

QWidget *UserWindow::buildPageHeader(const QString &eyebrow,
                                     const QString &title,
                                     const QString &subtitle)
{
    auto *header = new QWidget;
    auto *layout = new QVBoxLayout(header);
    layout->setContentsMargins(4, 22, 4, 14);
    layout->setSpacing(5);
    layout->addWidget(makeLabel(eyebrow.toUpper(), "eyebrow"));
    layout->addWidget(makeLabel(title, "pageTitle"));
    if (!subtitle.isEmpty()) {
        layout->addWidget(makeLabel(subtitle, "subtitle"));
    }
    return header;
}

QWidget *UserWindow::buildLoginPage()
{
    auto *page = new QWidget;
    page->setObjectName(QStringLiteral("loginPage"));
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(34, 48, 34, 42);
    layout->setSpacing(17);
    layout->addStretch();

    auto *mark = makeLabel(QStringLiteral("⚡"), "brandMark");
    mark->setAlignment(Qt::AlignCenter);
    layout->addWidget(mark, 0, Qt::AlignCenter);
    auto *title = makeLabel(QStringLiteral("一路满电，随时出发"), "heroTitle");
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);
    auto *subtitle = makeLabel(QStringLiteral("EVCharge 智慧充电用户端"), "subtitle");
    subtitle->setAlignment(Qt::AlignCenter);
    layout->addWidget(subtitle);
    layout->addSpacing(15);

    auto *loginCard = makeCard();
    auto *form = new QVBoxLayout(loginCard);
    form->setContentsMargins(24, 24, 24, 24);
    form->setSpacing(12);
    form->addWidget(makeLabel(QStringLiteral("手机号登录"), "sectionTitle"));
    form->addWidget(makeLabel(QStringLiteral("新手机号将自动完成注册，无需密码"), "caption"));
    m_phoneEdit = new QLineEdit;
    m_phoneEdit->setPlaceholderText(QStringLiteral("请输入 11 位手机号"));
    m_phoneEdit->setMaxLength(11);
    m_phoneEdit->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("[0-9]{0,11}")), m_phoneEdit));
    form->addWidget(m_phoneEdit);
    auto *loginButton = makeButton(QStringLiteral("登录 / 自动注册"));
    loginButton->setMinimumHeight(48);
    form->addWidget(loginButton);
    form->addWidget(makeLabel(
        QStringLiteral("请保持服务连接；新手机号首次登录会自动注册。"), "hint"));
    layout->addWidget(loginCard);
    layout->addStretch(2);

    connect(loginButton, &QPushButton::clicked, this, &UserWindow::attemptLogin);
    connect(m_phoneEdit, &QLineEdit::returnPressed, this, &UserWindow::attemptLogin);
    return page;
}

QWidget *UserWindow::buildHomePage()
{
    auto *page = new QWidget;
    auto *pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(0);

    auto *content = new QWidget;
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(20, 0, 20, 24);
    layout->setSpacing(14);
    layout->addWidget(buildPageHeader(QStringLiteral("EVCharge"),
                                      QStringLiteral("附近充电站"),
                                      QStringLiteral("查询附近站点，查看实时电桩状态")));

    auto *locationCard = makeCard();
    locationCard->setProperty("variant", "search");
    auto *locationLayout = new QVBoxLayout(locationCard);
    locationLayout->setContentsMargins(18, 18, 18, 18);
    locationLayout->setSpacing(10);
    locationLayout->addWidget(makeLabel(QStringLiteral("搜索位置"), "sectionTitle"));
    auto *locationRow = new QHBoxLayout;
    auto *districtBox = new QComboBox;
    districtBox->addItem(QStringLiteral("甘井子区 · 东软软件园 A 区"),
                         QStringLiteral("甘井子区|黄浦路901号东软软件园A区"));
    districtBox->addItem(QStringLiteral("高新园区 · 河口湾产业园"),
                         QStringLiteral("高新园区|河口湾产业园区地下停车场"));
    districtBox->addItem(QStringLiteral("沙河口区 · 星海广场"),
                         QStringLiteral("沙河口区|星海广场"));
    districtBox->addItem(QStringLiteral("中山区 · 人民路"),
                         QStringLiteral("中山区|人民路"));
    auto *presetButton = makeButton(QStringLiteral("预设定位"), "primary");
    presetButton->setMinimumWidth(86);
    locationRow->addWidget(districtBox, 1);
    locationRow->addWidget(presetButton);
    locationLayout->addLayout(locationRow);

    auto *manualRow = new QHBoxLayout;
    auto *addressEdit = new QLineEdit;
    addressEdit->setPlaceholderText(QStringLiteral("输入完整街道、门牌号或地标"));
    auto *locateButton = makeButton(QStringLiteral("地址搜索"), "secondary");
    locateButton->setMinimumWidth(86);
    manualRow->addWidget(addressEdit, 1);
    manualRow->addWidget(locateButton);
    locationLayout->addLayout(manualRow);
    layout->addWidget(locationCard);

    auto *headingRow = new QHBoxLayout;
    headingRow->addWidget(makeLabel(QStringLiteral("为你推荐"), "sectionTitle"));
    headingRow->addStretch();
    headingRow->addWidget(makeLabel(QStringLiteral("按距离从近到远"), "caption"));
    layout->addLayout(headingRow);
    m_stationListLayout = new QVBoxLayout;
    m_stationListLayout->setSpacing(14);
    layout->addLayout(m_stationListLayout);
    m_stationListLayout->addWidget(makeLabel(QStringLiteral("登录后加载服务端站点数据"), "hint"));
    layout->addStretch();

    pageLayout->addWidget(makeScrollArea(content), 1);
    pageLayout->addWidget(buildBottomNavigation(Home));
    const auto sendLocation = [this](const QString &district, const QString &address) {
        if (!m_socketClient->isConnected() || m_sessionId.isEmpty()) {
            showNotice(QStringLiteral("请连接服务并登录后再定位"), true);
            return;
        }
        if (address.isEmpty()) {
            showNotice(QStringLiteral("请输入街道或地标"), true);
            return;
        }
        m_locationDistrict = district;
        m_originName = m_locationDistrict + QStringLiteral(" · ") + address;
        sendRequest(MessageTypes::MapGeocode,
                    QJsonObject{{QStringLiteral("district"), m_locationDistrict},
                                {QStringLiteral("address"), address}});
    };
    connect(presetButton, &QPushButton::clicked, this, [districtBox, sendLocation]() {
        const QStringList preset = districtBox->currentData().toString().split(QLatin1Char('|'));
        if (preset.size() == 2) sendLocation(preset.at(0), preset.at(1));
    });
    connect(locateButton, &QPushButton::clicked, this, [districtBox, addressEdit, sendLocation]() {
        const QString district = districtBox->currentData().toString().section(QLatin1Char('|'), 0, 0);
        sendLocation(district, addressEdit->text().trimmed());
    });
    return page;
}

QWidget *UserWindow::buildStationCard(const QJsonObject &station)
{
    const QString name = station.value(QStringLiteral("name")).toString();
    const QString address = station.value(QStringLiteral("address")).toString();
    const bool recommended = station.value(QStringLiteral("recommended")).toBool();
    const int stationId = station.value(QStringLiteral("stationId")).toInt();
    auto *stationCard = makeCard();
    stationCard->setProperty("variant", "station");
    auto *layout = new QVBoxLayout(stationCard);
    layout->setContentsMargins(18, 17, 18, 17);
    layout->setSpacing(10);
    auto *titleRow = new QHBoxLayout;
    titleRow->addWidget(makeLabel(name, "cardTitle"));
    titleRow->addStretch();
    if (recommended) {
        titleRow->addWidget(makeLabel(QStringLiteral("低拥堵推荐"), "badgeGood"));
    }
    layout->addLayout(titleRow);
    layout->addWidget(makeLabel(QStringLiteral("⌖ %1").arg(address), "caption"));
    auto *metrics = new QHBoxLayout;
    metrics->addWidget(makeLabel(QStringLiteral("综合价 %1 / 度").arg(
        displayMoney(station.value(QStringLiteral("totalPriceFenPerKwh")).toInt())), "metric"));
    metrics->addWidget(makeLabel(QStringLiteral("%1 / %2 空闲")
        .arg(station.value(QStringLiteral("availablePileCount")).toInt())
        .arg(station.value(QStringLiteral("pileCount")).toInt()), "metric"));
    metrics->addStretch();
    metrics->addWidget(makeLabel(QStringLiteral("%1 km").arg(
        station.value(QStringLiteral("distanceKm")).toDouble(), 0, 'f', 2), "distance"));
    layout->addLayout(metrics);
    auto *actions = new QHBoxLayout;
    auto *detailButton = makeButton(QStringLiteral("查看详情"), "secondary");
    auto *navigationButton = makeButton(QStringLiteral("导航到这里"), "primary");
    actions->addWidget(detailButton, 1);
    actions->addWidget(navigationButton, 1);
    layout->addLayout(actions);
    connect(detailButton, &QPushButton::clicked, this, [this, station, stationId]() {
        m_selectedStation = station;
        m_stationDetailTitle->setText(station.value(QStringLiteral("name")).toString());
        m_stationDetailSummary->setText(QStringLiteral("正在获取站点详情…"));
        clearLayout(m_pileListLayout);
        m_pileListLayout->addWidget(makeLabel(QStringLiteral("正在加载站内电桩…"), "hint"));
        showPage(StationDetail);
        sendRequest(MessageTypes::StationDetailGet,
                    QJsonObject{{QStringLiteral("stationId"), stationId}});
    });
    connect(navigationButton, &QPushButton::clicked, this, [this, station]() {
        openNavigation(station, Home);
    });
    return stationCard;
}

QWidget *UserWindow::buildStationDetailPage()
{
    auto *page = new QWidget;
    auto *pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(0);
    auto *content = new QWidget;
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(20, 0, 20, 24);
    layout->setSpacing(13);
    auto *detailTop = new QHBoxLayout;
    detailTop->setContentsMargins(0, 16, 0, 0);
    detailTop->setSpacing(12);
    auto *backButton = makeButton(QStringLiteral("←"), "icon");
    backButton->setFixedSize(42, 42);
    backButton->setToolTip(QStringLiteral("返回附近充电站列表"));
    backButton->setAccessibleName(QStringLiteral("返回首页"));
    auto *detailHeader = buildPageHeader(QStringLiteral("STATION DETAIL"),
                                         QStringLiteral("充电站详情"));
    detailTop->addWidget(backButton, 0, Qt::AlignTop);
    detailTop->addWidget(detailHeader, 1);
    layout->addLayout(detailTop);
    m_stationDetailTitle = makeLabel(QStringLiteral("请选择站点"), "cardTitle");
    layout->addWidget(m_stationDetailTitle);

    auto *summaryCard = makeCard();
    summaryCard->setProperty("variant", "stationHero");
    auto *summaryLayout = new QHBoxLayout(summaryCard);
    summaryLayout->setContentsMargins(18, 16, 18, 16);
    m_stationDetailSummary = makeLabel(QStringLiteral("等待服务端数据"), "metricLarge");
    summaryLayout->addWidget(m_stationDetailSummary);
    summaryLayout->addStretch();
    auto *navigationButton = makeButton(QStringLiteral("导航到这里"), "primary");
    summaryLayout->addWidget(navigationButton);
    layout->addWidget(summaryCard);
    layout->addWidget(makeLabel(QStringLiteral("选择充电桩"), "sectionTitle"));
    m_pileListLayout = new QVBoxLayout;
    m_pileListLayout->setSpacing(12);
    layout->addLayout(m_pileListLayout);
    layout->addStretch();
    pageLayout->addWidget(makeScrollArea(content), 1);
    pageLayout->addWidget(buildBottomNavigation(Home));
    connect(backButton, &QPushButton::clicked, this,
            [this]() { showPage(Home); });
    connect(navigationButton, &QPushButton::clicked, this, [this]() {
        openNavigation(m_selectedStation, StationDetail);
    });
    return page;
}

QWidget *UserWindow::buildPileCard(const QJsonObject &pile)
{
    const QString number = pile.value(QStringLiteral("pileNo")).toString();
    const QString type = pile.value(QStringLiteral("type")).toString();
    const QString status = pile.value(QStringLiteral("status")).toString();
    const int pileId = pile.value(QStringLiteral("pileId")).toInt();
    const bool available = status == QStringLiteral("AVAILABLE");
    auto *pileCard = makeCard();
    pileCard->setProperty("variant", "pile");
    auto *layout = new QHBoxLayout(pileCard);
    layout->setContentsMargins(17, 15, 17, 15);
    auto *information = new QVBoxLayout;
    information->addWidget(makeLabel(number + QStringLiteral(" · ") + pileTypeText(type), "cardTitle"));
    information->addWidget(makeLabel(QStringLiteral("%1 kW · %2")
        .arg(pile.value(QStringLiteral("powerKw")).toDouble(), 0, 'f', 1)
        .arg(pileStatusText(status)), available ? "badgeGood" : "badgeNeutral"));
    layout->addLayout(information);
    layout->addStretch();
    auto *selectButton = makeButton(available ? QStringLiteral("选择") : QStringLiteral("不可用"),
                                    available ? "primary" : "disabled");
    selectButton->setEnabled(available);
    layout->addWidget(selectButton);
    connect(selectButton, &QPushButton::clicked, this, [this, number, pileId]() {
        if (confirmUserAction(this, QStringLiteral("确认预约充电桩"),
                              QStringLiteral("选择 %1 开始本次充电？").arg(number),
                              QStringLiteral("预约成功后将创建待开始订单，你可以在充电页开始或取消。"),
                              QStringLiteral("确认预约"))) {
            showPage(Charging);
            sendRequest(MessageTypes::OrderCreate,
                        QJsonObject{{QStringLiteral("pileId"), pileId}});
        }
    });
    return pileCard;
}

QWidget *UserWindow::buildChargingPage()
{
    auto *page = new QWidget;
    auto *pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(0);
    auto *content = new QWidget;
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(20, 0, 20, 24);
    layout->setSpacing(14);
    layout->addWidget(buildPageHeader(QStringLiteral("ACTIVE ORDER"),
                                      QStringLiteral("当前充电"),
                                      QStringLiteral("时长、电量和金额均以服务端数据为准")));

    auto *orderCard = makeCard();
    orderCard->setProperty("variant", "orderHero");
    auto *orderLayout = new QVBoxLayout(orderCard);
    orderLayout->setContentsMargins(20, 19, 20, 19);
    auto *orderTop = new QHBoxLayout;
    orderTop->addWidget(makeLabel(QStringLiteral("活动订单"), "cardTitle"));
    orderTop->addStretch();
    m_orderStatusLabel = makeLabel(QStringLiteral("待开始"), "badgeWarn");
    orderTop->addWidget(m_orderStatusLabel);
    orderLayout->addLayout(orderTop);
    m_orderSummaryLabel = makeLabel(QStringLiteral("暂无活动订单"), "caption");
    orderLayout->addWidget(m_orderSummaryLabel);
    layout->addWidget(orderCard);

    auto *progressCard = makeCard();
    progressCard->setProperty("variant", "chargeProgress");
    auto *progressLayout = new QVBoxLayout(progressCard);
    progressLayout->setContentsMargins(20, 20, 20, 20);
    progressLayout->setSpacing(15);
    progressLayout->addWidget(makeLabel(QStringLiteral("本次充电"), "sectionTitle"));
    m_chargeStatisticsLabel = makeLabel(QStringLiteral("0 秒　0.00 kWh　¥0.00"), "metricLarge");
    progressLayout->addWidget(m_chargeStatisticsLabel);
    auto *progress = new QProgressBar;
    progress->setRange(0, 100);
    progress->setValue(0);
    progress->setTextVisible(false);
    progressLayout->addWidget(progress);
    m_orderHintLabel = makeLabel(QString(), "hint");
    progressLayout->addWidget(m_orderHintLabel);
    layout->addWidget(progressCard);

    auto *actions = new QVBoxLayout;
    actions->setSpacing(10);
    m_startButton = makeButton(QStringLiteral("开始充电"));
    m_cancelButton = makeButton(QStringLiteral("取消预约"), "dangerGhost");
    m_stopButton = makeButton(QStringLiteral("停止充电"), "danger");
    m_settleButton = makeButton(QStringLiteral("确认结算"));
    for (QPushButton *action : {m_startButton, m_cancelButton, m_stopButton, m_settleButton}) {
        action->setMinimumHeight(50);
        actions->addWidget(action);
    }
    layout->addLayout(actions);
    layout->addStretch();
    pageLayout->addWidget(makeScrollArea(content), 1);
    pageLayout->addWidget(buildBottomNavigation(Charging));

    connect(m_startButton, &QPushButton::clicked, this, [this]() {
        if (confirmUserAction(this, QStringLiteral("开始充电"),
                              QStringLiteral("确认启动当前充电桩？"),
                              QStringLiteral("启动后系统将开始记录充电时长、电量和费用。"),
                              QStringLiteral("开始充电"))) {
            sendRequest(MessageTypes::OrderStart,
                        QJsonObject{{QStringLiteral("orderId"), m_activeOrder.value(QStringLiteral("orderId")).toInt()}});
        }
    });
    connect(m_cancelButton, &QPushButton::clicked, this, [this]() {
        if (confirmUserAction(this, QStringLiteral("取消预约"),
                              QStringLiteral("确认取消当前预约？"),
                              QStringLiteral("订单尚未开始，取消后该充电桩将重新释放。"),
                              QStringLiteral("取消预约"), true)) {
            sendRequest(MessageTypes::OrderCancel,
                        QJsonObject{{QStringLiteral("orderId"), m_activeOrder.value(QStringLiteral("orderId")).toInt()}});
        }
    });
    connect(m_stopButton, &QPushButton::clicked, this, [this]() {
        if (confirmUserAction(this, QStringLiteral("停止充电"),
                              QStringLiteral("确认结束本次充电？"),
                              QStringLiteral("停止后将按实际充电量生成待结算金额，此操作无法撤销。"),
                              QStringLiteral("停止并结算"), true)) {
            sendRequest(MessageTypes::OrderStop,
                        QJsonObject{{QStringLiteral("orderId"), m_activeOrder.value(QStringLiteral("orderId")).toInt()}});
        }
    });
    connect(m_settleButton, &QPushButton::clicked, this, [this]() {
        if (confirmUserAction(this, QStringLiteral("确认结算"),
                              QStringLiteral("使用钱包余额结算当前订单？"),
                              QStringLiteral("结算成功后订单将完成，并更新钱包余额与充电记录。"),
                              QStringLiteral("确认支付"))) {
            sendRequest(MessageTypes::OrderSettle,
                        QJsonObject{{QStringLiteral("orderId"), m_activeOrder.value(QStringLiteral("orderId")).toInt()}});
        }
    });
    setOrderStatus(m_orderStatus);
    return page;
}

QWidget *UserWindow::buildProfilePage()
{
    auto *page = new QWidget;
    auto *pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(0);
    auto *content = new QWidget;
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(20, 0, 20, 24);
    layout->setSpacing(14);
    auto *profileCard = makeCard();
    profileCard->setProperty("variant", "profile");
    auto *profileLayout = new QHBoxLayout(profileCard);
    profileLayout->setContentsMargins(20, 20, 20, 20);
    m_avatarLabel = makeLabel(QStringLiteral("U"), "avatar");
    m_avatarLabel->setAlignment(Qt::AlignCenter);
    m_avatarLabel->setFixedSize(66, 66);
    m_avatarLabel->setProperty("hasAvatar", false);
    profileLayout->addWidget(m_avatarLabel);
    auto *identity = new QVBoxLayout;
    m_nicknameLabel = makeLabel(QStringLiteral("用户信息加载中"), "cardTitle");
    identity->addWidget(m_nicknameLabel);
    m_profileIdLabel = makeLabel(QStringLiteral("ID：--"), "caption");
    identity->addWidget(m_profileIdLabel);
    m_profilePhoneLabel = makeLabel(QStringLiteral("尚未登录"), "caption");
    identity->addWidget(m_profilePhoneLabel);
    m_profileStatusLabel = makeLabel(QStringLiteral("账户状态：--"), "badgeGood");
    m_profileStatusLabel->setMaximumWidth(120);
    identity->addWidget(m_profileStatusLabel, 0, Qt::AlignLeft);
    profileLayout->addLayout(identity, 1);
    auto *profileActions = new QVBoxLayout;
    auto *avatarButton = makeButton(QStringLiteral("更换头像"), "ghostCompact");
    auto *removeAvatarButton = makeButton(QStringLiteral("移除头像"), "ghostCompact");
    auto *renameButton = makeButton(QStringLiteral("修改昵称"), "ghostCompact");
    profileActions->addWidget(avatarButton);
    profileActions->addWidget(removeAvatarButton);
    profileActions->addWidget(renameButton);
    profileLayout->addLayout(profileActions);
    layout->addWidget(profileCard);

    auto *walletCard = makeCard();
    walletCard->setProperty("variant", "wallet");
    auto *walletLayout = new QVBoxLayout(walletCard);
    walletLayout->setContentsMargins(20, 19, 20, 19);
    walletLayout->setSpacing(14);
    auto *walletTop = new QHBoxLayout;
    auto *walletInformation = new QVBoxLayout;
    walletInformation->addWidget(makeLabel(QStringLiteral("我的钱包"), "cardTitle"));
    walletInformation->addWidget(makeLabel(QStringLiteral("账户余额"), "caption"));
    m_balanceLabel = makeLabel(displayMoney(m_balanceFenInFen), "walletAmount");
    walletInformation->addWidget(m_balanceLabel);
    walletTop->addLayout(walletInformation);
    walletTop->addStretch();
    walletLayout->addLayout(walletTop);
    auto *rechargeButton = makeButton(QStringLiteral("立即充值"));
    walletTop->addWidget(rechargeButton, 0, Qt::AlignVCenter);
    layout->addWidget(walletCard);

    layout->addWidget(makeLabel(QStringLiteral("常用功能"), "sectionTitle"));
    auto *commonCard = makeCard();
    commonCard->setProperty("variant", "common");
    auto *commonLayout = new QVBoxLayout(commonCard);
    commonLayout->setContentsMargins(18, 16, 18, 18);
    auto *featureGrid = new QGridLayout;
    featureGrid->setHorizontalSpacing(10);
    featureGrid->setVerticalSpacing(8);
    auto *ordersButton = makeButton(QStringLiteral("▤\n我的订单"), "profileFeature");
    ordersButton->setToolTip(QStringLiteral("查看最近的充电与结算记录"));
    ordersButton->setMinimumSize(92, 100);
    featureGrid->addWidget(ordersButton, 0, 0);
    featureGrid->setColumnStretch(0, 0);
    featureGrid->setColumnStretch(1, 1);
    featureGrid->setColumnStretch(2, 1);
    featureGrid->setColumnStretch(3, 1);
    commonLayout->addLayout(featureGrid);
    layout->addWidget(commonCard);
    auto *logoutButton = makeButton(QStringLiteral("退出登录"), "dangerGhost");
    layout->addWidget(logoutButton);
    layout->addStretch();
    pageLayout->addWidget(makeScrollArea(content), 1);
    pageLayout->addWidget(buildBottomNavigation(Profile));

    connect(renameButton, &QPushButton::clicked, this, &UserWindow::showRenameDialog);
    connect(rechargeButton, &QPushButton::clicked, this, &UserWindow::showRechargeDialog);
    connect(ordersButton, &QPushButton::clicked, this, [this]() {
        if (m_ordersDialog) {
            m_ordersDialog->raise();
            m_ordersDialog->activateWindow();
            return;
        }
        auto *dialog = new QDialog(this);
        m_ordersDialog = dialog;
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setWindowTitle(QStringLiteral("我的订单"));
        dialog->setModal(true);
        dialog->resize(430, 560);
        auto *dialogLayout = new QVBoxLayout(dialog);
        dialogLayout->setContentsMargins(18, 18, 18, 18);
        auto *title = makeLabel(QStringLiteral("最近订单"), "sectionTitle");
        dialogLayout->addWidget(title);
        auto *hint = makeLabel(QStringLiteral("查看最近的充电与结算记录"), "caption");
        dialogLayout->addWidget(hint);
        auto *ordersScroll = new QScrollArea(dialog);
        ordersScroll->setWidgetResizable(true);
        auto *ordersContent = new QWidget(ordersScroll);
        m_orderListLayout = new QVBoxLayout(ordersContent);
        m_orderListLayout->setSpacing(12);
        ordersScroll->setWidget(ordersContent);
        dialogLayout->addWidget(ordersScroll, 1);
        auto *closeButton = makeButton(QStringLiteral("关闭"), "secondary");
        dialogLayout->addWidget(closeButton);
        connect(closeButton, &QPushButton::clicked, dialog, &QDialog::close);
        connect(dialog, &QObject::destroyed, this, [this]() {
            m_ordersDialog = nullptr;
            m_orderListLayout = nullptr;
        });
        dialog->open();
        sendRequest(MessageTypes::UserOrderList,
                    QJsonObject{{QStringLiteral("page"), 1},
                                {QStringLiteral("pageSize"), 20}});
    });
    connect(avatarButton, &QPushButton::clicked, this, &UserWindow::uploadAvatar);
    connect(removeAvatarButton, &QPushButton::clicked, this, [this]() {
        if (confirmUserAction(this, QStringLiteral("移除头像"),
                              QStringLiteral("确认移除当前头像？"),
                              QStringLiteral("移除后将恢复为默认头像，但可随时重新上传。"),
                              QStringLiteral("确认移除"), true)) {
            sendRequest(MessageTypes::UserAvatarRemove);
        }
    });
    connect(logoutButton, &QPushButton::clicked, this, [this]() {
        if (confirmUserAction(this, QStringLiteral("退出登录"),
                              QStringLiteral("确认退出当前账户？"),
                              QStringLiteral("退出后需要重新输入手机号登录，当前服务连接不会断开。"),
                              QStringLiteral("退出登录"), true)) {
            m_sessionId.clear();
            showPage(Login);
        }
    });
    return page;
}

QWidget *UserWindow::buildOrderCard(const QString &station, const QString &description,
                                    const QString &amount, const QString &status)
{
    auto *orderCard = makeCard();
    orderCard->setProperty("variant", "order");
    auto *layout = new QHBoxLayout(orderCard);
    layout->setContentsMargins(17, 15, 17, 15);
    auto *information = new QVBoxLayout;
    information->addWidget(makeLabel(station, "cardTitle"));
    information->addWidget(makeLabel(description, "caption"));
    layout->addLayout(information, 1);
    auto *summary = new QVBoxLayout;
    auto *amountLabel = makeLabel(amount, "metric");
    amountLabel->setAlignment(Qt::AlignRight);
    summary->addWidget(amountLabel);
    const QString displayStatus = orderStatusText(status);
    auto *statusLabel = makeLabel(displayStatus, displayStatus == QStringLiteral("待结算")
                                              ? "badgeWarn" : "badgeNeutral");
    statusLabel->setAlignment(Qt::AlignCenter);
    summary->addWidget(statusLabel);
    layout->addLayout(summary);
    return orderCard;
}

QWidget *UserWindow::buildNavigationPage()
{
    m_mapNavigationPage = new MapNavigationPage;
    connect(m_mapNavigationPage, &MapNavigationPage::backRequested, this,
            [this]() { showPage(m_navigationSource); });
    connect(m_mapNavigationPage, &MapNavigationPage::retryRequested, this,
            [this](const MapRoute &route, MapNavigationPage::TravelMode mode) {
                requestRoutePlan(route, mode == MapNavigationPage::TravelMode::Driving);
            });
    return m_mapNavigationPage;
}

QWidget *UserWindow::buildBottomNavigation(Page activePage)
{
    auto *navigation = new QFrame;
    navigation->setObjectName(QStringLiteral("bottomNavigation"));
    auto *layout = new QHBoxLayout(navigation);
    layout->setContentsMargins(12, 8, 12, 10);
    layout->setSpacing(8);
    const QList<QPair<QString, Page>> items{
        {QStringLiteral("⌂\n首页"), Home},
        {QStringLiteral("ϟ\n充电"), Charging},
        {QStringLiteral("●\n我的"), Profile}
    };
    for (const auto &item : items) {
        const bool active = item.second == activePage;
        auto *navigationButton = makeButton(item.first, active ? "navActive" : "nav");
        navigationButton->setObjectName(QStringLiteral("bottomNavButton"));
        navigationButton->setMinimumHeight(58);
        layout->addWidget(navigationButton, 1);
        connect(navigationButton, &QPushButton::clicked, this,
                [this, page = item.second]() { showPage(page); });
    }
    return navigation;
}

void UserWindow::openNavigation(const QJsonObject &station, Page source)
{
    if (!m_mapNavigationPage) {
        showNotice(QStringLiteral("地图页面尚未初始化"), true);
        return;
    }
    const QString name = station.value(QStringLiteral("name")).toString();
    const double longitude = station.value(QStringLiteral("longitude")).toDouble();
    const double latitude = station.value(QStringLiteral("latitude")).toDouble();
    const MapRoute route{
        m_originName, m_originLongitude, m_originLatitude,
        name, station.value(QStringLiteral("address")).toString(), longitude, latitude
    };
    if (!m_mapNavigationPage->setRoute(route)) {
        return;
    }
    m_navigationSource = source;
    showPage(Navigation);
    // 页面不保存地图 Key；由服务端异步返回路线数据后再渲染。
    QMetaObject::invokeMethod(m_mapNavigationPage, "retry", Qt::QueuedConnection);
}

void UserWindow::requestRoutePlan(const MapRoute &route, bool driving)
{
    if (!m_mapNavigationPage) {
        return;
    }
    if (!m_socketClient->isConnected() || m_sessionMode != SessionMode::Real
        || m_sessionId.isEmpty()) {
        m_mapNavigationPage->setLoadError(
            QStringLiteral("请连接服务端并完成真实登录，再获取路线。"));
        return;
    }
    const QJsonObject payload{
        {QStringLiteral("originLongitude"), route.originLongitude},
        {QStringLiteral("originLatitude"), route.originLatitude},
        {QStringLiteral("destinationLongitude"), route.destinationLongitude},
        {QStringLiteral("destinationLatitude"), route.destinationLatitude},
        {QStringLiteral("mode"), driving ? QStringLiteral("DRIVING") : QStringLiteral("WALKING")}
    };
    m_routePlanRequestId = m_socketClient->sendRequest(
        MessageTypes::MapRoutePlan, m_sessionId, payload, {}, 10000);
    if (m_routePlanRequestId.isEmpty()) {
        m_mapNavigationPage->setLoadError(QStringLiteral("路线请求发送失败，请检查服务连接。"));
        return;
    }
    m_requestTypes.insert(m_routePlanRequestId, MessageTypes::MapRoutePlan);
    m_mapNavigationPage->setLoadError(QStringLiteral("正在由服务端规划路线…"));
}

void UserWindow::showPage(Page page)
{
    m_pages->setCurrentIndex(static_cast<int>(page));
    if (m_sessionMode == SessionMode::Real && !m_sessionId.isEmpty()
        && m_socketClient->isConnected()) {
        if (page == Charging) {
            requestActiveOrder();
        } else if (page == Profile) {
            sendRequest(MessageTypes::UserProfileGet);
            sendRequest(MessageTypes::UserOrderList,
                        QJsonObject{{QStringLiteral("page"), 1},
                                    {QStringLiteral("pageSize"), 20}});
        }
    }
    if (page != Charging && m_orderPollTimer) {
        m_orderPollTimer->stop();
    }
}

void UserWindow::attemptLogin()
{
    const QString phone = m_phoneEdit->text().trimmed();
    if (phone.size() != 11 || !phone.startsWith(QLatin1Char('1'))) {
        showNotice(QStringLiteral("请输入正确的 11 位手机号"), true);
        return;
    }
    if (m_socketClient->isConnected()) {
        m_loginRequestId = sendRequest(
            MessageTypes::UserLogin, QJsonObject{{QStringLiteral("phone"), phone}});
        showNotice(m_loginRequestId.isEmpty() ? QStringLiteral("登录请求发送失败")
                                              : QStringLiteral("正在登录…"),
                   m_loginRequestId.isEmpty());
        return;
    }
    showNotice(QStringLiteral("服务尚未连接，请稍后重试"), true);
}

void UserWindow::setConnected(bool connected)
{
    m_connectionLabel->setText(connected
        ? QStringLiteral("● 服务已连接 · 127.0.0.1:18080")
        : m_sessionMode == SessionMode::Real
            ? QStringLiteral("● 服务连接已断开 · 请重新连接")
            : QStringLiteral("● 服务未连接 · 点击右侧重试"));
    m_connectionLabel->setProperty("online", connected);
    m_connectionLabel->style()->unpolish(m_connectionLabel);
    m_connectionLabel->style()->polish(m_connectionLabel);
    if (!connected && m_orderPollTimer) {
        m_orderPollTimer->stop();
    }
}

void UserWindow::setOrderStatus(const QString &status)
{
    m_orderStatus = status;
    if (!m_orderStatusLabel) {
        return;
    }
    const bool created = status == QStringLiteral("CREATED");
    const bool charging = status == QStringLiteral("CHARGING");
    const bool pendingPayment = status == QStringLiteral("PENDING_PAYMENT");
    const bool completed = status == QStringLiteral("COMPLETED");
    m_startButton->setVisible(created);
    m_cancelButton->setVisible(created);
    m_stopButton->setVisible(charging);
    m_settleButton->setVisible(pendingPayment);
    const bool canWrite = m_sessionMode == SessionMode::Real
        && m_socketClient->isConnected() && !m_sessionId.isEmpty();
    m_startButton->setEnabled(created && canWrite);
    m_cancelButton->setEnabled(created && canWrite);
    m_stopButton->setEnabled(charging && canWrite);
    m_settleButton->setEnabled(pendingPayment && canWrite);
    m_orderStatusLabel->setText(created ? QStringLiteral("待开始")
        : charging ? QStringLiteral("充电中")
        : pendingPayment ? QStringLiteral("待结算")
        : completed ? QStringLiteral("已完成") : QStringLiteral("已取消"));
    m_orderHintLabel->setText(created
        ? QStringLiteral("订单已创建，请确认车辆连接后开始充电。")
        : charging ? QStringLiteral("页面将定时获取服务端返回的权威充电进度。")
        : pendingPayment ? QStringLiteral("充电已停止，请核对应付金额并完成钱包结算。")
        : completed ? QStringLiteral("订单已完成，电桩已经释放。")
                    : QStringLiteral("预约已取消，电桩已经释放。"));
    if (charging && m_pages && m_pages->currentIndex() == Charging) {
        m_orderPollTimer->start();
    } else if (m_orderPollTimer) {
        m_orderPollTimer->stop();
    }
}

void UserWindow::showRechargeDialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("钱包充值"));
    auto *layout = new QFormLayout(&dialog);
    auto *amountEdit = new QLineEdit;
    amountEdit->setPlaceholderText(QStringLiteral("例如：50.00"));
    layout->addRow(QStringLiteral("充值金额（元）"), amountEdit);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    bool valid = false;
    const double amountYuan = amountEdit->text().toDouble(&valid);
    if (!valid || amountYuan <= 0.0) {
        showNotice(QStringLiteral("请输入有效的充值金额"), true);
        return;
    }
    const int amountFen = qRound(amountYuan * 100.0);
    sendRequest(MessageTypes::UserRecharge,
                QJsonObject{{QStringLiteral("amountFen"), amountFen}});
}

void UserWindow::showRenameDialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("修改昵称"));
    auto *layout = new QFormLayout(&dialog);
    auto *nicknameEdit = new QLineEdit(m_nicknameLabel->text());
    nicknameEdit->setMaxLength(20);
    layout->addRow(QStringLiteral("昵称"), nicknameEdit);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    layout->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const QString nickname = nicknameEdit->text().trimmed();
    if (nickname.size() < 2) {
        showNotice(QStringLiteral("昵称长度应为 2–20 个字符"), true);
        return;
    }
    sendRequest(MessageTypes::UserProfileUpdate,
                QJsonObject{{QStringLiteral("nickname"), nickname}});
}

void UserWindow::uploadAvatar()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择头像"), {}, QStringLiteral("图片 (*.png *.jpg *.jpeg)"));
    if (path.isEmpty()) {
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 1000 * 1024) {
        showNotice(QStringLiteral("头像无法读取或超过 1000 KiB"), true);
        return;
    }
    const QString suffix = QFileInfo(path).suffix().toLower();
    const QString mimeType = suffix == QStringLiteral("png")
        ? QStringLiteral("image/png") : QStringLiteral("image/jpeg");
    const QByteArray content = file.readAll();
    if (m_sessionMode != SessionMode::Real || m_sessionId.isEmpty()) {
        showNotice(QStringLiteral("请先登录后再上传头像"), true);
        return;
    }
    sendRequest(MessageTypes::UserAvatarUpload,
                QJsonObject{{QStringLiteral("fileName"), QFileInfo(path).fileName()},
                            {QStringLiteral("mimeType"), mimeType},
                            {QStringLiteral("contentBase64"),
                             QString::fromLatin1(content.toBase64())}});
}

QString UserWindow::sendRequest(const QString &type, const QJsonObject &payload)
{
    if (!m_socketClient->isConnected()) {
        showNotice(QStringLiteral("服务未连接，当前操作不可提交"), true);
        return {};
    }
    if (type != MessageTypes::UserLogin
        && (m_sessionMode != SessionMode::Real || m_sessionId.isEmpty())) {
        showNotice(QStringLiteral("请先完成真实登录"), true);
        return {};
    }
    const QString session = type == MessageTypes::UserLogin ? QString() : m_sessionId;
    const QString requestId = m_socketClient->sendRequest(type, session, payload);
    if (!requestId.isEmpty()) {
        m_requestTypes.insert(requestId, type);
    }
    return requestId;
}

void UserWindow::requestInitialData()
{
    sendRequest(MessageTypes::UserProfileGet);
    // 站点列表来自数据库，不应被第三方地理编码服务的可用性阻断。
    // 先按默认位置加载真实站点；地理编码成功后会使用新坐标再次刷新。
    sendRequest(MessageTypes::StationListNearby,
                QJsonObject{{QStringLiteral("longitude"), m_originLongitude},
                            {QStringLiteral("latitude"), m_originLatitude},
                            {QStringLiteral("limit"), 20}});
    sendRequest(MessageTypes::PredictionRecommendation,
                QJsonObject{{QStringLiteral("longitude"), m_originLongitude},
                            {QStringLiteral("latitude"), m_originLatitude},
                            {QStringLiteral("limit"), 5},
                            {QStringLiteral("horizon"), QStringLiteral("1h")}});
    sendRequest(MessageTypes::MapGeocode,
                QJsonObject{{QStringLiteral("district"), QStringLiteral("大连市")},
                            {QStringLiteral("address"),
                             QStringLiteral("大连市甘井子区黄浦路901号东软软件园A区")}});
    requestActiveOrder();
}

void UserWindow::requestActiveOrder()
{
    if (m_sessionMode == SessionMode::Real && !m_sessionId.isEmpty()
        && m_socketClient->isConnected()) {
        sendRequest(MessageTypes::OrderActiveCheck);
    }
}

void UserWindow::showNotice(const QString &message, bool error)
{
    auto *notice = new QLabel(message, centralWidget());
    notice->setObjectName(error ? QStringLiteral("errorNotice")
                                : QStringLiteral("successNotice"));
    notice->setWordWrap(true);
    notice->setAlignment(Qt::AlignCenter);
    notice->setGeometry(28, centralWidget()->height() - 120,
                        centralWidget()->width() - 56, 54);
    notice->raise();
    notice->show();
    QTimer::singleShot(2800, notice, &QLabel::deleteLater);
}

void UserWindow::clearLayout(QVBoxLayout *layout)
{
    if (!layout) return;
    while (QLayoutItem *item = layout->takeAt(0)) {
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }
}

void UserWindow::applyUser(const QJsonObject &user)
{
    const QString nickname = user.value(QStringLiteral("nickname")).toString();
    const QString phone = user.value(QStringLiteral("phone")).toString();
    if (!nickname.isEmpty()) m_nicknameLabel->setText(nickname);
    if (m_profilePhoneLabel && !phone.isEmpty()) {
        const QString masked = phone.size() == 11
            ? phone.left(3) + QStringLiteral("****") + phone.right(4) : phone;
        m_profilePhoneLabel->setText(QStringLiteral("手机号：") + masked);
    }
    if (m_profileIdLabel) m_profileIdLabel->setText(QStringLiteral("ID：%1").arg(user.value(QStringLiteral("userId")).toInteger()));
    if (m_profileStatusLabel) m_profileStatusLabel->setText(QStringLiteral("账户状态：%1").arg(userStatusText(user.value(QStringLiteral("status")).toString())));
    m_balanceFenInFen = user.value(QStringLiteral("balanceFen")).toInt();
    if (m_balanceLabel) m_balanceLabel->setText(displayMoney(m_balanceFenInFen));

    const QString avatarPath = user.value(QStringLiteral("avatarPath")).toString();
    if (avatarPath != m_avatarPath) {
        m_avatarPath = avatarPath;
        requestAvatar(avatarPath);
    }
}

void UserWindow::resetAvatar()
{
    if (!m_avatarLabel) {
        return;
    }
    m_avatarLabel->setPixmap(QPixmap());
    m_avatarLabel->setText(QStringLiteral("U"));
    m_avatarLabel->setProperty("hasAvatar", false);
    m_avatarLabel->style()->unpolish(m_avatarLabel);
    m_avatarLabel->style()->polish(m_avatarLabel);
}

void UserWindow::requestAvatar(const QString &avatarPath)
{
    if (avatarPath.isEmpty()) {
        resetAvatar();
        return;
    }
    if (m_avatarRequestPath == avatarPath && !m_avatarRequestId.isEmpty()) {
        return;
    }
    m_avatarRequestPath = avatarPath;
    m_avatarRequestId = sendRequest(MessageTypes::UserAvatarGet);
    if (m_avatarRequestId.isEmpty()) {
        m_avatarRequestPath.clear();
    }
}

void UserWindow::applyOrder(const QJsonObject &order)
{
    m_activeOrder = order;
    if (order.isEmpty()) {
        m_orderSummaryLabel->setText(QStringLiteral("暂无活动订单"));
        setOrderStatus(QStringLiteral("COMPLETED"));
        return;
    }
    const QString status = order.value(QStringLiteral("status")).toString();
    m_orderSummaryLabel->setText(QStringLiteral("%1 · %2 · %3")
        .arg(order.value(QStringLiteral("stationName")).toString(),
             order.value(QStringLiteral("pileNo")).toString(),
             order.value(QStringLiteral("orderNo")).toString()));
    m_chargeStatisticsLabel->setText(QStringLiteral("%1 秒　%2 kWh　%3")
        .arg(order.value(QStringLiteral("chargeSeconds")).toInt())
        .arg(order.value(QStringLiteral("energyKwh")).toDouble(), 0, 'f', 2)
        .arg(displayMoney(order.value(QStringLiteral("amountFen")).toInt())));
    setOrderStatus(status);
}

void UserWindow::renderStations(const QJsonArray &stations)
{
    clearLayout(m_stationListLayout);
    if (stations.isEmpty()) {
        m_stationListLayout->addWidget(makeLabel(QStringLiteral("当前没有可展示的充电站"), "hint"));
        return;
    }
    for (const QJsonValue &value : stations) {
        m_stationListLayout->addWidget(buildStationCard(value.toObject()));
    }
}

void UserWindow::renderStationDetail(const QJsonObject &station, const QJsonArray &piles)
{
    m_selectedStation = station;
    m_stationDetailTitle->setText(station.value(QStringLiteral("name")).toString()
        + QStringLiteral("\n") + station.value(QStringLiteral("address")).toString());
    m_stationDetailSummary->setText(QStringLiteral("综合价 %1 / 度　%2 / %3 空闲")
        .arg(displayMoney(station.value(QStringLiteral("totalPriceFenPerKwh")).toInt()))
        .arg(station.value(QStringLiteral("availablePileCount")).toInt())
        .arg(station.value(QStringLiteral("pileCount")).toInt()));
    clearLayout(m_pileListLayout);
    if (piles.isEmpty()) {
        m_pileListLayout->addWidget(makeLabel(QStringLiteral("站内暂无电桩"), "hint"));
    } else {
        for (const QJsonValue &value : piles) m_pileListLayout->addWidget(buildPileCard(value.toObject()));
    }
}

void UserWindow::renderOrders(const QJsonArray &orders)
{
    if (!m_orderListLayout) return;
    clearLayout(m_orderListLayout);
    if (orders.isEmpty()) {
        m_orderListLayout->addWidget(makeLabel(QStringLiteral("暂无订单记录"), "hint"));
        return;
    }
    for (const QJsonValue &value : orders) {
        const QJsonObject order = value.toObject();
        m_orderListLayout->addWidget(buildOrderCard(
            order.value(QStringLiteral("stationName")).toString(),
            QStringLiteral("%1 · %2 · %3 kWh").arg(
                order.value(QStringLiteral("createdAt")).toString(),
                order.value(QStringLiteral("pileNo")).toString())
                .arg(order.value(QStringLiteral("energyKwh")).toDouble(), 0, 'f', 2),
            displayMoney(order.value(QStringLiteral("amountFen")).toInt()),
            order.value(QStringLiteral("status")).toString()));
    }
}

void UserWindow::handleResponse(const QJsonObject &response)
{
    const QString requestId = response.value(QStringLiteral("requestId")).toString();
    const QString type = m_requestTypes.take(requestId);
    const int code = response.value(QStringLiteral("code")).toInt();
    if (code == 4003) {
        m_sessionId.clear();
        showPage(Login);
        showNotice(QStringLiteral("会话已失效，请重新登录"), true);
        return;
    }
    if (code != 200) {
        if (requestId == m_loginRequestId) {
            m_loginRequestId.clear();
        }
        if (requestId == m_routePlanRequestId) {
            m_routePlanRequestId.clear();
            if (m_mapNavigationPage) {
                m_mapNavigationPage->setLoadError(
                    response.value(QStringLiteral("message")).toString());
            }
        }
        if (requestId == m_avatarRequestId) {
            m_avatarRequestId.clear();
            m_avatarRequestPath.clear();
        }
        showNotice(response.value(QStringLiteral("message")).toString(), true);
        return;
    }
    const QJsonObject data = response.value(QStringLiteral("data")).toObject();
    if (type == MessageTypes::UserLogin
        && data.value(QStringLiteral("sessionId")).isString()) {
        m_sessionId = data.value(QStringLiteral("sessionId")).toString();
        m_sessionMode = SessionMode::Real;
        const QJsonObject user = data.value(QStringLiteral("user")).toObject();
        if (user.value(QStringLiteral("nickname")).isString() && m_nicknameLabel) {
            m_nicknameLabel->setText(user.value(QStringLiteral("nickname")).toString());
        }
        if (user.value(QStringLiteral("balanceFen")).isDouble() && m_balanceLabel) {
            m_balanceFenInFen = user.value(QStringLiteral("balanceFen")).toInt();
            m_balanceLabel->setText(displayMoney(m_balanceFenInFen));
        }
        m_loginRequestId.clear();
        applyUser(data.value(QStringLiteral("user")).toObject());
        showPage(Home);
        showNotice(QStringLiteral("登录成功"));
        requestInitialData();
    } else if (type == MessageTypes::MapGeocode) {
        m_originLongitude = data.value(QStringLiteral("longitude")).toDouble();
        m_originLatitude = data.value(QStringLiteral("latitude")).toDouble();
        const QString formatted = data.value(QStringLiteral("formattedAddress")).toString();
        if (!formatted.isEmpty()) m_originName = formatted;
        sendRequest(MessageTypes::StationListNearby,
                    QJsonObject{{QStringLiteral("longitude"), m_originLongitude},
                                {QStringLiteral("latitude"), m_originLatitude},
                                {QStringLiteral("limit"), 20}});
        sendRequest(MessageTypes::PredictionRecommendation,
                    QJsonObject{{QStringLiteral("longitude"), m_originLongitude},
                                {QStringLiteral("latitude"), m_originLatitude},
                                {QStringLiteral("limit"), 5},
                                {QStringLiteral("horizon"), QStringLiteral("1h")}});
        showNotice(QStringLiteral("位置已更新，正在加载附近站点"));
    } else if (type == MessageTypes::MapRoutePlan && m_mapNavigationPage) {
        m_routePlanRequestId.clear();
        const QJsonArray points = data.value(QStringLiteral("polyline")).toArray();
        MapRoutePlanPreview plan;
        plan.distanceMeters = data.value(QStringLiteral("distanceMeters")).toInt();
        plan.durationMinutes = data.value(QStringLiteral("durationMinutes")).toDouble();
        for (const QJsonValue &value : points) {
            const QJsonObject point = value.toObject();
            const QJsonValue longitude = point.value(QStringLiteral("longitude"));
            const QJsonValue latitude = point.value(QStringLiteral("latitude"));
            if (!longitude.isDouble() || !latitude.isDouble()) {
                m_mapNavigationPage->setLoadError(
                    QStringLiteral("服务端返回的路线坐标格式错误。"));
                return;
            }
            plan.polyline.append(QPointF(longitude.toDouble(), latitude.toDouble()));
        }
        m_mapNavigationPage->setRoutePlan(plan);
    } else if (type == MessageTypes::UserProfileGet
               || type == MessageTypes::UserProfileUpdate) {
        applyUser(data.value(QStringLiteral("user")).   toObject());
        if (type == MessageTypes::UserProfileUpdate) showNotice(QStringLiteral("昵称已更新"));
    } else if (type == MessageTypes::UserAvatarGet) {
        // 只接受当前头像路径对应的响应，避免旧请求覆盖刚上传的新头像。
        if (requestId != m_avatarRequestId) {
            return;
        }
        m_avatarRequestId.clear();
        m_avatarRequestPath.clear();
        const QString avatarPath = data.value(QStringLiteral("avatarPath")).toString();
        if (!data.value(QStringLiteral("hasAvatar")).toBool()
            || avatarPath.isEmpty() || avatarPath != m_avatarPath) {
            if (m_avatarPath.isEmpty()) resetAvatar();
            return;
        }
        const QByteArray content = QByteArray::fromBase64(
            data.value(QStringLiteral("contentBase64")).toString().toLatin1(),
            QByteArray::AbortOnBase64DecodingErrors);
        QPixmap avatar;
        if (content.isEmpty() || !avatar.loadFromData(content)) {
            showNotice(QStringLiteral("头像文件无法显示"), true);
            return;
        }
        m_avatarLabel->setText(QString());
        m_avatarLabel->setProperty("hasAvatar", true);
        m_avatarLabel->style()->unpolish(m_avatarLabel);
        m_avatarLabel->style()->polish(m_avatarLabel);
        m_avatarLabel->setPixmap(circularAvatar(avatar, QSize(56, 56)));
    } else if (type == MessageTypes::StationListNearby
               || type == MessageTypes::PredictionRecommendation) {
        if (type == MessageTypes::StationListNearby) {
            m_nearbyStations = data.value(QStringLiteral("stations")).toArray();
        } else {
            m_recommendedStations = data.value(QStringLiteral("stations")).toArray();
        }
        QSet<int> recommendedIds;
        for (const QJsonValue &value : m_recommendedStations) {
            recommendedIds.insert(value.toObject().value(QStringLiteral("stationId")).toInt());
        }
        QJsonArray combined;
        for (const QJsonValue &value : m_nearbyStations) {
            QJsonObject station = value.toObject();
            if (recommendedIds.contains(station.value(QStringLiteral("stationId")).toInt())) {
                station.insert(QStringLiteral("recommended"), true);
            }
            combined.append(station);
        }
        renderStations(combined.isEmpty() ? m_recommendedStations : combined);
    } else if (type == MessageTypes::StationDetailGet) {
        renderStationDetail(data.value(QStringLiteral("station")).toObject(),
                            data.value(QStringLiteral("piles")).toArray());
    } else if (type == MessageTypes::OrderActiveCheck) {
        m_balanceFenInFen = data.value(QStringLiteral("balanceFen")).toInt(m_balanceFenInFen);
        applyOrder(data.value(QStringLiteral("order")).toObject());
    } else if (type == MessageTypes::OrderCreate || type == MessageTypes::OrderStart
               || type == MessageTypes::OrderStop || type == MessageTypes::OrderCancel) {
        applyOrder(data.value(QStringLiteral("order")).toObject());
        showPage(Charging);
    } else if (type == MessageTypes::OrderSettle) {
        m_balanceFenInFen = data.value(QStringLiteral("balanceFen")).toInt(m_balanceFenInFen);
        if (m_balanceLabel) m_balanceLabel->setText(displayMoney(m_balanceFenInFen));
        applyOrder(data.value(QStringLiteral("order")).toObject());
        showNotice(QStringLiteral("结算完成，欢迎下次使用"));
    } else if (type == MessageTypes::UserRecharge) {
        m_balanceFenInFen = data.value(QStringLiteral("balanceFen")).toInt();
        m_balanceLabel->setText(displayMoney(m_balanceFenInFen));
        showNotice(QStringLiteral("充值成功"));
    } else if (type == MessageTypes::UserOrderList) {
        renderOrders(data.value(QStringLiteral("items")).toArray());
    } else if (type == MessageTypes::UserAvatarUpload) {
        applyUser(data.value(QStringLiteral("user")).toObject());
        showNotice(QStringLiteral("头像上传成功"));
    } else if (type == MessageTypes::UserAvatarRemove) {
        applyUser(data.value(QStringLiteral("user")).toObject());
        showNotice(QStringLiteral("头像已移除"));
    }
}
