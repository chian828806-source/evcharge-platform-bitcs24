#include "userwindow.h"

#include "network/socketclient.h"
#include "shared/protocol/messagetypes.h"

#include <QColor>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QList>
#include <QJsonArray>
#include <QLineEdit>
#include <QMessageBox>
#include <QPair>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QScrollArea>
#include <QStackedWidget>
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
    m_connectionLabel = makeLabel(QStringLiteral("● 演示模式 · 后端未连接"), "connection");
    connectionLayout->addWidget(m_connectionLabel);
    connectionLayout->addStretch();
    auto *connectButton = makeButton(QStringLiteral("连接本机服务"), "link");
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
            m_socketClient->disconnectFromServer();
        } else {
            m_connectionLabel->setText(QStringLiteral("● 正在连接 127.0.0.1:18080…"));
            m_socketClient->connectToServer(QStringLiteral("127.0.0.1"), 18080);
        }
    });
    connect(m_socketClient, &SocketClient::connected, this,
            [this]() { setConnected(true); });
    connect(m_socketClient, &SocketClient::disconnected, this,
            [this]() { setConnected(false); });
    connect(m_socketClient, &SocketClient::socketError, this,
            [this](const QString &message) {
                setConnected(false);
                showNotice(QStringLiteral("连接失败：%1；仍可预览演示界面").arg(message), true);
            });
    connect(m_socketClient, &SocketClient::requestTimedOut, this,
            [this](const QString &requestId, const QString &) {
                m_requestTypes.remove(requestId);
                if (requestId == m_loginRequestId) {
                    m_loginRequestId.clear();
                }
                showNotice(QStringLiteral("请求超时，请检查服务后重试"), true);
            });
    connect(m_socketClient, &SocketClient::requestFailed, this,
            [this](const QString &requestId, const QString &, const QString &message) {
                if (requestId == m_loginRequestId) {
                    m_loginRequestId.clear();
                }
                m_requestTypes.remove(requestId);
                showNotice(QStringLiteral("请求失败：%1").arg(message), true);
            });
    connect(m_socketClient, &SocketClient::responseReceived,
            this, &UserWindow::handleResponse);

    m_orderPollTimer = new QTimer(this);
    m_orderPollTimer->setInterval(1000);
    connect(m_orderPollTimer, &QTimer::timeout, this, &UserWindow::requestActiveOrder);
}

QWidget *UserWindow::buildPageHeader(const QString &eyebrow,
                                     const QString &title,
                                     const QString &subtitle)
{
    auto *header = new QWidget;
    auto *layout = new QVBoxLayout(header);
    layout->setContentsMargins(4, 20, 4, 12);
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
        QStringLiteral("未连接后端时，输入合法手机号即可使用 Mock 数据预览 UI。"), "hint"));
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
                                      QStringLiteral("下午好，用户8000 · 当前位置：甘井子区")));

    auto *locationCard = makeCard();
    auto *locationLayout = new QVBoxLayout(locationCard);
    locationLayout->setContentsMargins(18, 18, 18, 18);
    locationLayout->setSpacing(10);
    locationLayout->addWidget(makeLabel(QStringLiteral("模拟当前位置"), "sectionTitle"));
    auto *locationRow = new QHBoxLayout;
    auto *districtBox = new QComboBox;
    districtBox->addItems({QStringLiteral("甘井子区"), QStringLiteral("高新园区"),
                           QStringLiteral("沙河口区"), QStringLiteral("中山区")});
    auto *addressEdit = new QLineEdit(QStringLiteral("软件园路"));
    addressEdit->setPlaceholderText(QStringLiteral("输入街道或地标"));
    auto *locateButton = makeButton(QStringLiteral("定位"), "secondary");
    locationRow->addWidget(districtBox);
    locationRow->addWidget(addressEdit, 1);
    locationRow->addWidget(locateButton);
    locationLayout->addLayout(locationRow);
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
    connect(locateButton, &QPushButton::clicked, this, [this, districtBox]() {
        if (!m_socketClient->isConnected() || m_sessionId.isEmpty()) {
            showNotice(QStringLiteral("地址解析接口尚未实现；演示坐标无需联网"), true);
            return;
        }
        const QJsonObject payload{{QStringLiteral("longitude"), 121.538},
                                  {QStringLiteral("latitude"), 38.889},
                                  {QStringLiteral("district"), districtBox->currentText()},
                                  {QStringLiteral("limit"), 20}};
        sendRequest(MessageTypes::StationListNearby, payload);
        sendRequest(MessageTypes::PredictionRecommendation,
                    QJsonObject{{QStringLiteral("longitude"), 121.538},
                                {QStringLiteral("latitude"), 38.889},
                                {QStringLiteral("limit"), 5},
                                {QStringLiteral("horizon"), QStringLiteral("1h")}});
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
    auto *navigationButton = makeButton(QStringLiteral("路线导航"), "ghost");
    actions->addWidget(detailButton, 1);
    actions->addWidget(navigationButton, 1);
    layout->addLayout(actions);
    connect(detailButton, &QPushButton::clicked, this, [this, station, stationId]() {
        m_selectedStation = station;
        showPage(StationDetail);
        if (m_sessionId.startsWith(QStringLiteral("DEMO-"))) {
            const QJsonArray piles{
                QJsonObject{{"pileId", 1}, {"pileNo", "P01"}, {"type", "FAST"}, {"powerKw", 60.0}, {"status", "AVAILABLE"}},
                QJsonObject{{"pileId", 2}, {"pileNo", "P02"}, {"type", "FAST"}, {"powerKw", 60.0}, {"status", "CHARGING"}},
                QJsonObject{{"pileId", 3}, {"pileNo", "P03"}, {"type", "SLOW"}, {"powerKw", 7.0}, {"status", "AVAILABLE"}}
            };
            renderStationDetail(station, piles);
        } else {
            sendRequest(MessageTypes::StationDetailGet,
                        QJsonObject{{QStringLiteral("stationId"), stationId}});
        }
    });
    connect(navigationButton, &QPushButton::clicked, this, [this, name]() {
        m_navigationDestination->setText(name);
        showPage(Navigation);
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
    auto *detailHeader = buildPageHeader(QStringLiteral("STATION DETAIL"),
                                         QStringLiteral("充电站详情"));
    layout->addWidget(detailHeader);
    m_stationDetailTitle = makeLabel(QStringLiteral("请选择站点"), "cardTitle");
    layout->addWidget(m_stationDetailTitle);

    auto *summaryCard = makeCard();
    auto *summaryLayout = new QHBoxLayout(summaryCard);
    summaryLayout->setContentsMargins(18, 16, 18, 16);
    m_stationDetailSummary = makeLabel(QStringLiteral("等待服务端数据"), "metricLarge");
    summaryLayout->addWidget(m_stationDetailSummary);
    summaryLayout->addStretch();
    auto *navigationButton = makeButton(QStringLiteral("导航"), "secondary");
    summaryLayout->addWidget(navigationButton);
    layout->addWidget(summaryCard);
    layout->addWidget(makeLabel(QStringLiteral("选择充电桩"), "sectionTitle"));
    m_pileListLayout = new QVBoxLayout;
    m_pileListLayout->setSpacing(12);
    layout->addLayout(m_pileListLayout);
    layout->addStretch();
    pageLayout->addWidget(makeScrollArea(content), 1);
    pageLayout->addWidget(buildBottomNavigation(Home));
    connect(navigationButton, &QPushButton::clicked, this, [this]() {
        m_navigationDestination->setText(m_selectedStation.value(QStringLiteral("name")).toString());
        showPage(Navigation);
    });
    return page;
}

QWidget *UserWindow::buildPileCard(const QJsonObject &pile)
{
    const QString number = pile.value(QStringLiteral("pileNo")).toString();
    const QString type = pile.value(QStringLiteral("type")).toString();
    const QString status = pile.value(QStringLiteral("status")).toString();
    const int pileId = pile.value(QStringLiteral("pileId")).toInt();
    auto *pileCard = makeCard();
    auto *layout = new QHBoxLayout(pileCard);
    layout->setContentsMargins(17, 15, 17, 15);
    auto *information = new QVBoxLayout;
    information->addWidget(makeLabel(number + QStringLiteral(" · ") + type, "cardTitle"));
    information->addWidget(makeLabel(QStringLiteral("%1 kW · %2")
        .arg(pile.value(QStringLiteral("powerKw")).toDouble(), 0, 'f', 1).arg(status), "caption"));
    layout->addLayout(information);
    layout->addStretch();
    const bool available = status == QStringLiteral("AVAILABLE");
    auto *selectButton = makeButton(available ? QStringLiteral("选择") : QStringLiteral("不可用"),
                                    available ? "primary" : "disabled");
    selectButton->setEnabled(available);
    layout->addWidget(selectButton);
    connect(selectButton, &QPushButton::clicked, this, [this, number, pileId]() {
        const auto choice = QMessageBox::question(
            this, QStringLiteral("创建预约"),
            QStringLiteral("确认选择充电桩 %1？服务端将创建待开始订单。").arg(number));
        if (choice == QMessageBox::Yes) {
            showPage(Charging);
            if (isDemoMode()) {
                applyOrder(QJsonObject{{"orderId", 1001}, {"orderNo", "DEMO-ORDER-001"},
                                       {"stationName", m_selectedStation.value("name")},
                                       {"pileNo", number}, {"status", "CREATED"},
                                       {"chargeSeconds", 0}, {"energyKwh", 0.0}, {"amountFen", 0}});
            } else {
                sendRequest(MessageTypes::OrderCreate,
                            QJsonObject{{QStringLiteral("pileId"), pileId}});
            }
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
    auto *orderLayout = new QVBoxLayout(orderCard);
    orderLayout->setContentsMargins(20, 19, 20, 19);
    auto *orderTop = new QHBoxLayout;
    orderTop->addWidget(makeLabel(QStringLiteral("软件园智慧充电站"), "cardTitle"));
    orderTop->addStretch();
    m_orderStatusLabel = makeLabel(QStringLiteral("待开始"), "badgeWarn");
    orderTop->addWidget(m_orderStatusLabel);
    orderLayout->addLayout(orderTop);
    m_orderSummaryLabel = makeLabel(QStringLiteral("暂无活动订单"), "caption");
    orderLayout->addWidget(m_orderSummaryLabel);
    layout->addWidget(orderCard);

    auto *progressCard = makeCard();
    auto *progressLayout = new QVBoxLayout(progressCard);
    progressLayout->setContentsMargins(20, 20, 20, 20);
    progressLayout->setSpacing(15);
    progressLayout->addWidget(makeLabel(QStringLiteral("本次充电"), "sectionTitle"));
    m_chargeStatisticsLabel = makeLabel(QStringLiteral("0 秒　0.00 kWh　¥0.00"), "metricLarge");
    progressLayout->addWidget(m_chargeStatisticsLabel);
    auto *progress = new QProgressBar;
    progress->setRange(0, 100);
    progress->setValue(38);
    progress->setTextVisible(false);
    progressLayout->addWidget(progress);
    m_orderHintLabel = makeLabel(QString(), "hint");
    progressLayout->addWidget(m_orderHintLabel);
    layout->addWidget(progressCard);

    auto *actions = new QHBoxLayout;
    m_startButton = makeButton(QStringLiteral("开始充电"));
    m_cancelButton = makeButton(QStringLiteral("取消预约"), "dangerGhost");
    m_stopButton = makeButton(QStringLiteral("停止充电"), "danger");
    m_settleButton = makeButton(QStringLiteral("确认结算"));
    actions->addWidget(m_startButton);
    actions->addWidget(m_cancelButton);
    actions->addWidget(m_stopButton);
    actions->addWidget(m_settleButton);
    layout->addLayout(actions);
    layout->addStretch();
    pageLayout->addWidget(makeScrollArea(content), 1);
    pageLayout->addWidget(buildBottomNavigation(Charging));

    connect(m_startButton, &QPushButton::clicked, this, [this]() {
        if (isDemoMode()) setOrderStatus(QStringLiteral("CHARGING"));
        else sendRequest(MessageTypes::OrderStart,
                         QJsonObject{{QStringLiteral("orderId"), m_activeOrder.value(QStringLiteral("orderId")).toInt()}});
    });
    connect(m_cancelButton, &QPushButton::clicked, this, [this]() {
        if (QMessageBox::question(this, QStringLiteral("取消预约"),
                                  QStringLiteral("确认取消尚未开始的订单？")) == QMessageBox::Yes) {
            if (isDemoMode()) setOrderStatus(QStringLiteral("CANCELLED"));
            else sendRequest(MessageTypes::OrderCancel,
                             QJsonObject{{QStringLiteral("orderId"), m_activeOrder.value(QStringLiteral("orderId")).toInt()}});
        }
    });
    connect(m_stopButton, &QPushButton::clicked, this, [this]() {
        if (QMessageBox::question(this, QStringLiteral("停止充电"),
                                  QStringLiteral("确认停止充电并生成待结算金额？")) == QMessageBox::Yes) {
            if (isDemoMode()) setOrderStatus(QStringLiteral("PENDING_PAYMENT"));
            else sendRequest(MessageTypes::OrderStop,
                             QJsonObject{{QStringLiteral("orderId"), m_activeOrder.value(QStringLiteral("orderId")).toInt()}});
        }
    });
    connect(m_settleButton, &QPushButton::clicked, this, [this]() {
        if (isDemoMode()) {
            setOrderStatus(QStringLiteral("COMPLETED"));
            showNotice(QStringLiteral("Mock 结算完成"));
        } else sendRequest(MessageTypes::OrderSettle,
                           QJsonObject{{QStringLiteral("orderId"), m_activeOrder.value(QStringLiteral("orderId")).toInt()}});
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
    layout->addWidget(buildPageHeader(QStringLiteral("MY EVCHARGE"),
                                      QStringLiteral("我的"),
                                      QStringLiteral("账户、钱包与充电记录")));

    auto *profileCard = makeCard();
    auto *profileLayout = new QHBoxLayout(profileCard);
    profileLayout->setContentsMargins(20, 20, 20, 20);
    m_avatarLabel = makeLabel(QStringLiteral("U"), "avatar");
    m_avatarLabel->setAlignment(Qt::AlignCenter);
    m_avatarLabel->setFixedSize(58, 58);
    profileLayout->addWidget(m_avatarLabel);
    auto *identity = new QVBoxLayout;
    m_nicknameLabel = makeLabel(QStringLiteral("用户8000"), "cardTitle");
    identity->addWidget(m_nicknameLabel);
    m_profilePhoneLabel = makeLabel(QStringLiteral("尚未登录"), "caption");
    identity->addWidget(m_profilePhoneLabel);
    profileLayout->addLayout(identity, 1);
    auto *avatarButton = makeButton(QStringLiteral("头像"), "ghost");
    auto *renameButton = makeButton(QStringLiteral("昵称"), "ghost");
    profileLayout->addWidget(avatarButton);
    profileLayout->addWidget(renameButton);
    layout->addWidget(profileCard);

    auto *walletCard = makeCard();
    auto *walletLayout = new QHBoxLayout(walletCard);
    walletLayout->setContentsMargins(20, 19, 20, 19);
    auto *walletInformation = new QVBoxLayout;
    walletInformation->addWidget(makeLabel(QStringLiteral("钱包余额"), "caption"));
    m_balanceLabel = makeLabel(displayMoney(m_balanceFenInFen), "walletAmount");
    walletInformation->addWidget(m_balanceLabel);
    walletLayout->addLayout(walletInformation);
    walletLayout->addStretch();
    auto *rechargeButton = makeButton(QStringLiteral("充值"));
    walletLayout->addWidget(rechargeButton);
    layout->addWidget(walletCard);

    layout->addWidget(makeLabel(QStringLiteral("最近订单"), "sectionTitle"));
    m_orderListLayout = new QVBoxLayout;
    m_orderListLayout->setSpacing(12);
    layout->addLayout(m_orderListLayout);
    auto *logoutButton = makeButton(QStringLiteral("退出登录"), "dangerGhost");
    layout->addWidget(logoutButton);
    layout->addStretch();
    pageLayout->addWidget(makeScrollArea(content), 1);
    pageLayout->addWidget(buildBottomNavigation(Profile));

    connect(renameButton, &QPushButton::clicked, this, &UserWindow::showRenameDialog);
    connect(rechargeButton, &QPushButton::clicked, this, &UserWindow::showRechargeDialog);
    connect(avatarButton, &QPushButton::clicked, this, &UserWindow::uploadAvatar);
    connect(logoutButton, &QPushButton::clicked, this, [this]() {
        m_sessionId.clear();
        showPage(Login);
    });
    return page;
}

QWidget *UserWindow::buildOrderCard(const QString &station, const QString &description,
                                    const QString &amount, const QString &status)
{
    auto *orderCard = makeCard();
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
    auto *statusLabel = makeLabel(status, status == QStringLiteral("待结算")
                                              ? "badgeWarn" : "badgeNeutral");
    statusLabel->setAlignment(Qt::AlignCenter);
    summary->addWidget(statusLabel);
    layout->addLayout(summary);
    return orderCard;
}

QWidget *UserWindow::buildNavigationPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(20, 18, 20, 24);
    layout->setSpacing(14);
    auto *backButton = makeButton(QStringLiteral("← 返回"), "ghost");
    backButton->setMaximumWidth(90);
    layout->addWidget(backButton);
    layout->addWidget(makeLabel(QStringLiteral("路线导航"), "pageTitle"));

    auto *routeCard = makeCard();
    auto *routeLayout = new QVBoxLayout(routeCard);
    routeLayout->setContentsMargins(20, 18, 20, 18);
    routeLayout->addWidget(makeLabel(QStringLiteral("起点 · 软件园路"), "caption"));
    m_navigationDestination = makeLabel(QStringLiteral("软件园智慧充电站"), "cardTitle");
    routeLayout->addWidget(m_navigationDestination);
    auto *modeRow = new QHBoxLayout;
    auto *driveButton = makeButton(QStringLiteral("驾车 · 8 分钟"));
    auto *walkButton = makeButton(QStringLiteral("步行 · 22 分钟"), "secondary");
    modeRow->addWidget(driveButton);
    modeRow->addWidget(walkButton);
    routeLayout->addLayout(modeRow);
    layout->addWidget(routeCard);

    auto *mapPlaceholder = new QFrame;
    mapPlaceholder->setObjectName(QStringLiteral("mapPlaceholder"));
    auto *mapLayout = new QVBoxLayout(mapPlaceholder);
    mapLayout->setAlignment(Qt::AlignCenter);
    auto *pin = makeLabel(QStringLiteral("⌖"), "mapPin");
    pin->setAlignment(Qt::AlignCenter);
    mapLayout->addWidget(pin, 0, Qt::AlignCenter);
    auto *mapTitle = makeLabel(QStringLiteral("地图容器占位"), "sectionTitle");
    mapTitle->setAlignment(Qt::AlignCenter);
    mapLayout->addWidget(mapTitle);
    auto *mapHint = makeLabel(
        QStringLiteral("导航页面 UI 已完成；QWebEngineView + 腾讯地图 Web API\n待后续联调实现。"), "subtitle");
    mapHint->setAlignment(Qt::AlignCenter);
    mapLayout->addWidget(mapHint);
    layout->addWidget(mapPlaceholder, 1);

    connect(backButton, &QPushButton::clicked, this,
            [this]() { showPage(Home); });
    connect(driveButton, &QPushButton::clicked, this,
            [this]() { showNotice(QStringLiteral("已选择驾车路线")); });
    connect(walkButton, &QPushButton::clicked, this,
            [this]() { showNotice(QStringLiteral("已选择步行路线")); });
    return page;
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
        auto *navigationButton = makeButton(
            item.first, item.second == activePage ? "navActive" : "nav");
        navigationButton->setMinimumHeight(52);
        layout->addWidget(navigationButton, 1);
        connect(navigationButton, &QPushButton::clicked, this,
                [this, page = item.second]() { showPage(page); });
    }
    return navigation;
}

void UserWindow::showPage(Page page)
{
    m_pages->setCurrentIndex(static_cast<int>(page));
    if (!m_sessionId.isEmpty() && m_socketClient->isConnected()) {
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
    m_sessionId = QStringLiteral("DEMO-SESSION");
    loadDemoData();
    showPage(Home);
    showNotice(QStringLiteral("已进入演示模式；接入后端后使用真实数据"));
}

bool UserWindow::isDemoMode() const
{
    return !m_socketClient->isConnected();
}

void UserWindow::setConnected(bool connected)
{
    m_connectionLabel->setText(connected
        ? QStringLiteral("● 服务已连接 · 127.0.0.1:18080")
        : QStringLiteral("● 演示模式 · 后端未连接"));
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
    const bool canWrite = (m_socketClient->isConnected() && !m_sessionId.isEmpty())
        || m_sessionId.startsWith(QStringLiteral("DEMO-"));
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
    if (!isDemoMode() && !m_sessionId.isEmpty()) {
        sendRequest(MessageTypes::UserRecharge,
                    QJsonObject{{QStringLiteral("amountFen"), amountFen}});
    } else {
        m_balanceFenInFen += amountFen;
        m_balanceLabel->setText(displayMoney(m_balanceFenInFen));
        showNotice(QStringLiteral("Mock 充值成功"));
    }
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
    if (!isDemoMode() && !m_sessionId.isEmpty()) {
        sendRequest(MessageTypes::UserProfileUpdate,
                    QJsonObject{{QStringLiteral("nickname"), nickname}});
    } else {
        m_nicknameLabel->setText(nickname);
        showNotice(QStringLiteral("昵称已更新（Mock 数据）"));
    }
}

void UserWindow::uploadAvatar()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择头像"), {}, QStringLiteral("图片 (*.png *.jpg *.jpeg)"));
    if (path.isEmpty()) {
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 512 * 1024) {
        showNotice(QStringLiteral("头像无法读取或超过 512 KiB"), true);
        return;
    }
    const QString suffix = QFileInfo(path).suffix().toLower();
    const QString mimeType = suffix == QStringLiteral("png")
        ? QStringLiteral("image/png") : QStringLiteral("image/jpeg");
    sendRequest(MessageTypes::UserAvatarUpload,
                QJsonObject{{QStringLiteral("fileName"), QFileInfo(path).fileName()},
                            {QStringLiteral("mimeType"), mimeType},
                            {QStringLiteral("contentBase64"),
                             QString::fromLatin1(file.readAll().toBase64())}});
}

QString UserWindow::sendRequest(const QString &type, const QJsonObject &payload)
{
    if (!m_socketClient->isConnected()) {
        showNotice(QStringLiteral("服务未连接，当前操作不可提交"), true);
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
    sendRequest(MessageTypes::StationListNearby,
                QJsonObject{{QStringLiteral("longitude"), 121.538},
                            {QStringLiteral("latitude"), 38.889},
                            {QStringLiteral("limit"), 20}});
    sendRequest(MessageTypes::PredictionRecommendation,
                QJsonObject{{QStringLiteral("longitude"), 121.538},
                            {QStringLiteral("latitude"), 38.889},
                            {QStringLiteral("limit"), 5},
                            {QStringLiteral("horizon"), QStringLiteral("1h")}});
    requestActiveOrder();
}

void UserWindow::loadDemoData()
{
    const QJsonArray stations{
        QJsonObject{{"stationId", 1}, {"name", "软件园智慧充电站"}, {"address", "软件园路 8 号"},
                    {"totalPriceFenPerKwh", 110}, {"availablePileCount", 2}, {"pileCount", 4},
                    {"distanceKm", 1.25}, {"recommended", true}},
        QJsonObject{{"stationId", 2}, {"name", "万达广场充电中心"}, {"address", "虹韵路 6 号 B2 层"},
                    {"totalPriceFenPerKwh", 118}, {"availablePileCount", 5}, {"pileCount", 12},
                    {"distanceKm", 2.80}},
        QJsonObject{{"stationId", 3}, {"name", "星海绿色能源站"}, {"address", "中山路 608 号"},
                    {"totalPriceFenPerKwh", 98}, {"availablePileCount", 1}, {"pileCount", 8},
                    {"distanceKm", 4.36}}
    };
    renderStations(stations);
    renderOrders(QJsonArray{
        QJsonObject{{"stationName", "软件园智慧充电站"}, {"createdAt", "今天 16:00"},
                    {"pileNo", "P01"}, {"energyKwh", 5.0}, {"amountFen", 550},
                    {"status", "PENDING_PAYMENT"}},
        QJsonObject{{"stationName", "万达广场充电中心"}, {"createdAt", "08-30 12:24"},
                    {"pileNo", "A07"}, {"energyKwh", 18.6}, {"amountFen", 2195},
                    {"status", "COMPLETED"}}
    });
}

void UserWindow::requestActiveOrder()
{
    if (!m_sessionId.isEmpty() && m_socketClient->isConnected()) {
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
        m_profilePhoneLabel->setText(masked + QStringLiteral(" · ")
            + user.value(QStringLiteral("status")).toString());
    }
    m_balanceFenInFen = user.value(QStringLiteral("balanceFen")).toInt();
    if (m_balanceLabel) m_balanceLabel->setText(displayMoney(m_balanceFenInFen));
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
        showNotice(response.value(QStringLiteral("message")).toString(), true);
        return;
    }
    const QJsonObject data = response.value(QStringLiteral("data")).toObject();
    if (type == MessageTypes::UserLogin
        && data.value(QStringLiteral("sessionId")).isString()) {
        m_sessionId = data.value(QStringLiteral("sessionId")).toString();
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
    } else if (type == MessageTypes::UserProfileGet
               || type == MessageTypes::UserProfileUpdate) {
        applyUser(data.value(QStringLiteral("user")).toObject());
        if (type == MessageTypes::UserProfileUpdate) showNotice(QStringLiteral("昵称已更新"));
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
    }
}
