#include "userwindow.h"

#include "map/mapnavigationpage.h"
#include "map/stationmapwidget.h"
#include "network/socketclient.h"
#include "shared/protocol/messagetypes.h"
#include "ui/stationsheet.h"

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
#include <QMouseEvent>
#include <QPair>
#include <QPixmap>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSplitter>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QSet>
#include <QStringList>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>
#include <QtMath>
#include <cmath>

class EnergyFlowWidget final : public QWidget
{
public:
    explicit EnergyFlowWidget(QWidget *parent = nullptr)
        : QWidget(parent), m_animation(new QTimer(this))
    {
        setObjectName(QStringLiteral("energyFlow"));
        setFixedHeight(56);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setAccessibleName(QStringLiteral("充能流动状态"));
        m_animation->setInterval(32);
        connect(m_animation, &QTimer::timeout, this, [this]() {
            if (m_phase < 1.0) {
                m_phase = qMin(1.0, m_phase + 0.010);
            } else if (++m_holdTicks >= 22) {
                m_phase = 0.0;
                m_holdTicks = 0;
            }
            update();
        });
        setVisible(false);
    }

    void setCharging(bool charging)
    {
        if (m_charging == charging) return;
        m_charging = charging;
        setVisible(charging);
        if (charging) {
            m_phase = 0.0;
            m_holdTicks = 0;
            m_animation->start();
        } else {
            m_animation->stop();
        }
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        const QRectF track = rect().adjusted(1, 8, -1, -8);
        const qreal radius = track.height() / 2.0;

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(226, 237, 233));
        painter.drawRoundedRect(track, radius, radius);

        const qreal progress = m_phase;
        const qreal inset = 3.0;
        const QRectF inner = track.adjusted(inset, inset, -inset, -inset);
        const qreal fillWidth = qMax(inner.height(), inner.width() * progress);
        QRectF fill(inner.left(), inner.top(), qMin(fillWidth, inner.width()), inner.height());
        QPainterPath fillClip;
        fillClip.addRoundedRect(inner, inner.height() / 2.0, inner.height() / 2.0);
        painter.save();
        painter.setClipPath(fillClip);

        QLinearGradient energy(fill.topLeft(), fill.topRight());
        energy.setColorAt(0.0, QColor(8, 132, 96));
        energy.setColorAt(0.55, QColor(24, 194, 139));
        energy.setColorAt(0.88, QColor(91, 239, 181));
        energy.setColorAt(1.0, QColor(220, 255, 238));
        painter.fillRect(fill, energy);

        const qreal headX = fill.right();
        QLinearGradient glow(headX - 28.0, 0, headX + 16.0, 0);
        glow.setColorAt(0.0, QColor(220, 255, 238, 0));
        glow.setColorAt(0.65, QColor(235, 255, 245, 190));
        glow.setColorAt(1.0, QColor(235, 255, 245, 0));
        painter.fillRect(QRectF(headX - 28.0, inner.top(), 44.0, inner.height()), glow);

        painter.setPen(QPen(QColor(231, 255, 244, 105), 1.0));
        const int segmentCount = 16;
        for (int segment = 1; segment < segmentCount; ++segment) {
            const qreal x = inner.left() + inner.width() * segment / segmentCount;
            if (x < fill.right() - 2.0)
                painter.drawLine(QPointF(x, inner.top() + 2.0), QPointF(x, inner.bottom() - 2.0));
        }
        painter.restore();

        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor(112, 181, 158, 135), 1.0));
        painter.drawRoundedRect(track, radius, radius);

        painter.setPen(Qt::NoPen);
        for (int dot = 0; dot < 7; ++dot) {
            const qreal drift = std::fmod(m_phase * (0.32 + dot * 0.025)
                                         + dot * 0.173, 1.0);
            const qreal x = track.left() + drift * track.width();
            const bool above = dot % 2 == 0;
            const qreal y = above ? track.top() - 3.5 - (dot % 3)
                                  : track.bottom() + 3.5 + (dot % 3);
            const qreal pulse = 0.55 + 0.45 * qSin(m_phase * 12.0 + dot * 1.8);
            const int alpha = static_cast<int>(70 + pulse * 95);
            const qreal dotRadius = dot % 3 == 0 ? 2.2 : 1.5;
            painter.setBrush(QColor(61, 220, 159, alpha));
            painter.drawEllipse(QPointF(x, y), dotRadius, dotRadius);
        }
    }

private:
    QTimer *m_animation = nullptr;
    qreal m_phase = 0.0;
    int m_holdTicks = 0;
    bool m_charging = false;
};

#include <functional>

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

class ClickableStationCard final : public QFrame
{
public:
    std::function<void()> activated;

protected:
    void mouseReleaseEvent(QMouseEvent *event) override
    {
        QFrame::mouseReleaseEvent(event);
        if (event->button() == Qt::LeftButton && rect().contains(event->pos())
            && activated) {
            activated();
        }
    }
};

class ScaledPixmapLabel final : public QLabel
{
public:
    explicit ScaledPixmapLabel(QWidget *parent = nullptr)
        : QLabel(parent)
    {
        setAlignment(Qt::AlignCenter);
    }

    void setSourcePixmap(const QPixmap &pixmap)
    {
        m_sourcePixmap = pixmap;
        updateScaledPixmap();
    }

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QLabel::resizeEvent(event);
        updateScaledPixmap();
    }

private:
    QPixmap m_sourcePixmap;

    void updateScaledPixmap()
    {
        if (!m_sourcePixmap.isNull() && size().isValid()) {
            setPixmap(m_sourcePixmap.scaled(size(), Qt::KeepAspectRatio,
                                            Qt::SmoothTransformation));
        }
    }
};

ClickableStationCard *makeClickableStationCard()
{
    auto *result = new ClickableStationCard;
    result->setObjectName(QStringLiteral("card"));
    result->setCursor(Qt::PointingHandCursor);
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
    m_pages->addWidget(buildPromotionPage());
    m_pages->addWidget(buildHomePage());
    m_pages->addWidget(buildStationDetailPage());
    m_pages->addWidget(buildChargingPage());
    m_pages->addWidget(buildProfilePage());
    m_pages->addWidget(buildFavoritesPage());
    m_pages->addWidget(buildOrdersPage());
    m_pages->addWidget(buildCouponsPage());
    m_pages->addWidget(buildMembershipPage());
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
                if (type == MessageTypes::UserStationFavoriteList && m_favoriteListLayout) {
                    clearLayout(m_favoriteListLayout);
                    m_favoriteListLayout->addWidget(makeLabel(
                        QStringLiteral("收藏列表加载超时，请稍后重试。"), "hint"));
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
                if (type == MessageTypes::UserStationFavoriteList && m_favoriteListLayout) {
                    clearLayout(m_favoriteListLayout);
                    m_favoriteListLayout->addWidget(makeLabel(
                        QStringLiteral("暂时无法获取收藏列表，请稍后重试。"), "hint"));
                }
                showNotice(QStringLiteral("请求失败：%1").arg(message), true);
            });
    connect(m_socketClient, &SocketClient::responseReceived,
            this, &UserWindow::handleResponse);

    m_orderPollTimer = new QTimer(this);
    m_orderPollTimer->setInterval(1000);
    connect(m_orderPollTimer, &QTimer::timeout, this, &UserWindow::requestActiveOrder);
    m_promotionTimer = new QTimer(this);
    m_promotionTimer->setInterval(1000);
    connect(m_promotionTimer, &QTimer::timeout, this, [this]() {
        --m_promotionSecondsRemaining;
        if (m_promotionSecondsRemaining <= 0) {
            finishPromotion();
            return;
        }
        updatePromotionSkipText();
    });
    m_couponPollTimer = new QTimer(this);
    m_couponPollTimer->setInterval(5000);
    connect(m_couponPollTimer, &QTimer::timeout, this, [this]() {
        if (!m_sessionId.isEmpty()) sendRequest(MessageTypes::UserCouponList);
    });
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

QWidget *UserWindow::buildPromotionPage()
{
    auto *page = new QWidget;
    page->setObjectName(QStringLiteral("promotionPage"));
    auto *layout = new QGridLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *image = new ScaledPixmapLabel(page);
    image->setObjectName(QStringLiteral("promotionImage"));
    image->setSourcePixmap(QPixmap(QStringLiteral(":/images/promotion-ad.png")));
    layout->addWidget(image, 0, 0);

    auto *skipButton = makeButton(QStringLiteral("跳过 3s"), "promotionSkip");
    skipButton->setMinimumSize(78, 34);
    layout->addWidget(skipButton, 0, 0, Qt::AlignTop | Qt::AlignRight);
    connect(skipButton, &QPushButton::clicked, this, &UserWindow::finishPromotion);
    return page;
}

QWidget *UserWindow::buildHomePage()
{
    auto *page = new QWidget;
    auto *pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(0);

    m_homeSplitter = new QSplitter(Qt::Vertical, page);
    m_homeSplitter->setObjectName(QStringLiteral("homeMapSplitter"));
    m_homeSplitter->setChildrenCollapsible(true);
    m_homeSplitter->setCollapsible(0, true);
    m_homeSplitter->setHandleWidth(0);

    m_stationMap = new StationMapWidget(m_homeSplitter);
    m_stationSheet = new StationSheet(m_homeSplitter);
    m_homeSplitter->addWidget(m_stationMap);
    m_homeSplitter->addWidget(m_stationSheet);
    m_homeSplitter->setStretchFactor(0, 1);
    m_homeSplitter->setStretchFactor(1, 1);
    pageLayout->addWidget(m_homeSplitter, 1);
    pageLayout->addWidget(buildBottomNavigation(Home));

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

    m_stationSheet->setContent(content);
    m_stationMap->setOrigin(m_originName, m_originLongitude, m_originLatitude);

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
    connect(m_stationMap, &StationMapWidget::stationSelected,
            this, &UserWindow::selectHomeStation);
    connect(m_stationSheet, &StationSheet::dragStarted, this, [this]() {
        m_homeSheetStartHeight = m_stationSheet ? m_stationSheet->height() : 0;
    });
    connect(m_stationSheet, &StationSheet::dragMoved, this, [this](int deltaY) {
        setHomeSheetHeight(m_homeSheetStartHeight - deltaY);
    });
    connect(m_stationSheet, &StationSheet::dragReleased,
            this, &UserWindow::snapHomeSheet);
    QTimer::singleShot(0, page, [this]() {
        if (m_homeSplitter) {
            setHomeSheetHeight(m_homeSplitter->height() / 2);
        }
    });
    return page;
}

QWidget *UserWindow::buildStationCard(const QJsonObject &station)
{
    const QString name = station.value(QStringLiteral("name")).toString();
    const QString address = station.value(QStringLiteral("address")).toString();
    const bool recommended = station.value(QStringLiteral("recommended")).toBool();
    const bool disabled = station.value(QStringLiteral("status")).toString()
        == QStringLiteral("DISABLED");
    const int stationId = station.value(QStringLiteral("stationId")).toInt();
    const bool isFavorite = station.value(QStringLiteral("isFavorite")).toBool();
    auto *stationCard = makeClickableStationCard();
    stationCard->setProperty("variant", "station");
    stationCard->setProperty("selected", stationId == m_selectedHomeStationId);
    stationCard->setToolTip(QStringLiteral("点击在地图中选中此站点"));
    auto *layout = new QVBoxLayout(stationCard);
    layout->setContentsMargins(18, 17, 18, 17);
    layout->setSpacing(10);
    auto *titleRow = new QHBoxLayout;
    titleRow->addWidget(makeLabel(name, "cardTitle"));
    titleRow->addStretch();
    if (recommended) {
        titleRow->addWidget(makeLabel(QStringLiteral("低拥堵推荐"), "badgeGood"));
    }
    if (disabled) {
        titleRow->addWidget(makeLabel(QStringLiteral("已停用"), "badgeNeutral"));
    }
    auto *favoriteButton = makeButton(isFavorite ? QStringLiteral("★") : QStringLiteral("☆"),
                                      isFavorite ? "favoriteIconActive" : "favoriteIcon");
    favoriteButton->setFixedSize(38, 38);
    favoriteButton->setToolTip(isFavorite ? QStringLiteral("取消收藏") : QStringLiteral("收藏站点"));
    favoriteButton->setAccessibleName(favoriteButton->toolTip());
    titleRow->addWidget(favoriteButton);
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
    stationCard->activated = [this, stationId]() { selectHomeStation(stationId); };
    m_stationCards.insert(stationId, stationCard);
    connect(detailButton, &QPushButton::clicked, this, [this, station, stationId]() {
        selectHomeStation(stationId);
        m_selectedStation = station;
        m_stationDetailSource = m_pages->currentIndex() == static_cast<int>(Favorites)
            ? Favorites : Home;
        m_stationDetailTitle->setText(station.value(QStringLiteral("name")).toString());
        m_stationDetailSummary->setText(QStringLiteral("正在获取站点详情…"));
        clearLayout(m_pileListLayout);
        m_pileListLayout->addWidget(makeLabel(QStringLiteral("正在加载站内电桩…"), "hint"));
        showPage(StationDetail);
        sendRequest(MessageTypes::StationDetailGet,
                    QJsonObject{{QStringLiteral("stationId"), stationId}});
    });
    connect(navigationButton, &QPushButton::clicked, this, [this, station, stationId]() {
        selectHomeStation(stationId);
        const Page source = m_pages->currentIndex() == static_cast<int>(Favorites)
            ? Favorites : Home;
        openNavigation(station, source);
    });
    connect(favoriteButton, &QPushButton::clicked, this, [this, stationId]() {
        toggleFavorite(stationId);
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
    m_stationFavoriteButton = makeButton(QStringLiteral("☆ 收藏"), "favorite");
    summaryLayout->addWidget(m_stationFavoriteButton);
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
            [this]() { showPage(m_stationDetailSource); });
    connect(m_stationFavoriteButton, &QPushButton::clicked, this, [this]() {
        toggleFavorite(m_selectedStation.value(QStringLiteral("stationId")).toInt());
    });
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
    const bool available = status == QStringLiteral("AVAILABLE")
        && m_selectedStation.value(QStringLiteral("status")).toString()
            != QStringLiteral("DISABLED");
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
            QJsonObject payload{{QStringLiteral("pileId"), pileId}};
            qint64 availableCouponId = 0;
            for (const QJsonValue &value : m_coupons) {
                const QJsonObject coupon = value.toObject();
                if (coupon.value(QStringLiteral("status")).toString() == QStringLiteral("AVAILABLE")) {
                    availableCouponId = static_cast<qint64>(
                        coupon.value(QStringLiteral("couponId")).toDouble());
                    break;
                }
            }
            if (availableCouponId > 0
                && QMessageBox::question(this, QStringLiteral("使用优惠券"),
                    QStringLiteral("检测到可用的八折优惠券，本次订单是否使用？"),
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes) {
                payload.insert(QStringLiteral("couponId"), availableCouponId);
            }
            showPage(Charging);
            sendRequest(MessageTypes::OrderCreate, payload);
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
    m_energyFlow = new EnergyFlowWidget(progressCard);
    progressLayout->addWidget(m_energyFlow);
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
    layout->setContentsMargins(20, 18, 20, 24);
    layout->setSpacing(14);
    auto *profileCard = makeCard();
    profileCard->setProperty("variant", "profile");
    auto *profileLayout = new QVBoxLayout(profileCard);
    profileLayout->setContentsMargins(20, 18, 20, 18);
    profileLayout->setSpacing(14);
    auto *profileTop = new QHBoxLayout;
    profileTop->setSpacing(15);
    m_avatarLabel = makeLabel(QStringLiteral("U"), "avatar");
    m_avatarLabel->setAlignment(Qt::AlignCenter);
    m_avatarLabel->setFixedSize(66, 66);
    m_avatarLabel->setProperty("hasAvatar", false);
    m_avatarLabel->setProperty("vip", false);
    profileTop->addWidget(m_avatarLabel, 0, Qt::AlignTop);
    auto *identity = new QVBoxLayout;
    identity->setSpacing(5);
    m_nicknameLabel = makeLabel(QStringLiteral("用户信息加载中"), "cardTitle");
    identity->addWidget(m_nicknameLabel);
    auto *identityMeta = new QHBoxLayout;
    identityMeta->setSpacing(14);
    m_profileIdLabel = makeLabel(QStringLiteral("ID：--"), "caption");
    identityMeta->addWidget(m_profileIdLabel);
    m_profilePhoneLabel = makeLabel(QStringLiteral("尚未登录"), "caption");
    identityMeta->addWidget(m_profilePhoneLabel);
    identityMeta->addStretch();
    identity->addLayout(identityMeta);
    m_profileStatusLabel = makeLabel(QStringLiteral("账户状态：--"), "badgeGood");
    identity->addWidget(m_profileStatusLabel, 0, Qt::AlignLeft);
    profileTop->addLayout(identity, 1);
    profileLayout->addLayout(profileTop);
    auto *profileActions = new QHBoxLayout;
    profileActions->setSpacing(8);
    auto *avatarButton = makeButton(QStringLiteral("更换头像"), "ghostCompact");
    auto *removeAvatarButton = makeButton(QStringLiteral("移除头像"), "ghostCompact");
    auto *renameButton = makeButton(QStringLiteral("修改昵称"), "ghostCompact");
    profileActions->addWidget(avatarButton, 1);
    profileActions->addWidget(removeAvatarButton, 1);
    profileActions->addWidget(renameButton, 1);
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
    commonLayout->setContentsMargins(12, 14, 12, 14);
    auto *featureRow = new QHBoxLayout;
    featureRow->setSpacing(4);
    const auto addProfileFeature = [commonCard, featureRow](const QString &iconText,
                                                             const QString &buttonText,
                                                             const QString &toolTip) {
        auto *feature = new QWidget(commonCard);
        feature->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        auto *featureLayout = new QVBoxLayout(feature);
        featureLayout->setContentsMargins(2, 2, 2, 2);
        featureLayout->setSpacing(5);
        auto *icon = makeLabel(iconText, "profileFeatureIcon");
        icon->setFixedSize(48, 48);
        icon->setAlignment(Qt::AlignCenter);
        if (iconText.size() > 1) icon->setProperty("compact", true);
        featureLayout->addWidget(icon, 0, Qt::AlignHCenter);
        auto *button = makeButton(buttonText, "profileFeature");
        button->setToolTip(toolTip);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        featureLayout->addWidget(button);
        featureRow->addWidget(feature, 1);
        return button;
    };
    auto *couponButton = addProfileFeature(QStringLiteral("券"), QStringLiteral("我的优惠券"),
                                            QStringLiteral("查看账户中的优惠券"));
    auto *membershipButton = addProfileFeature(QStringLiteral("VIP"), QStringLiteral("会员中心"),
                                                QStringLiteral("查看会员状态与会员卡"));
    auto *ordersButton = addProfileFeature(QStringLiteral("单"), QStringLiteral("我的订单"),
                                            QStringLiteral("查看最近的充电与结算记录"));
    auto *favoritesButton = addProfileFeature(QStringLiteral("★"), QStringLiteral("我的收藏"),
                                               QStringLiteral("我的收藏功能入口"));
    commonLayout->addLayout(featureRow);
    layout->addWidget(commonCard);
    auto *logoutButton = makeButton(QStringLiteral("退出登录"), "dangerGhost");
    layout->addWidget(logoutButton);
    layout->addStretch();
    pageLayout->addWidget(makeScrollArea(content), 1);
    pageLayout->addWidget(buildBottomNavigation(Profile));

    connect(renameButton, &QPushButton::clicked, this, &UserWindow::showRenameDialog);
    connect(rechargeButton, &QPushButton::clicked, this, &UserWindow::showRechargeDialog);
    connect(couponButton, &QPushButton::clicked, this, [this]() { showPage(Coupons); });
    connect(favoritesButton, &QPushButton::clicked, this, [this]() { showPage(Favorites); });
    connect(ordersButton, &QPushButton::clicked, this, [this]() { showPage(Orders); });
    connect(membershipButton, &QPushButton::clicked, this, [this]() { showPage(Membership); });
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
            if (m_couponPollTimer) m_couponPollTimer->stop();
            showPage(Login);
        }
    });
    return page;
}

QWidget *UserWindow::buildFavoritesPage()
{
    auto *page = new QWidget;
    auto *pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(0);

    auto *content = new QWidget;
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(20, 0, 20, 24);
    layout->setSpacing(14);

    auto *top = new QHBoxLayout;
    top->setContentsMargins(0, 16, 0, 0);
    top->setSpacing(12);
    auto *backButton = makeButton(QStringLiteral("←"), "icon");
    backButton->setFixedSize(42, 42);
    backButton->setToolTip(QStringLiteral("返回个人中心"));
    backButton->setAccessibleName(QStringLiteral("返回个人中心"));
    top->addWidget(backButton, 0, Qt::AlignTop);
    top->addWidget(buildPageHeader(QStringLiteral("MY FAVORITES"),
                                   QStringLiteral("我的收藏"),
                                   QStringLiteral("集中查看常用站点，快速进入详情或导航")), 1);
    layout->addLayout(top);

    auto *heading = new QHBoxLayout;
    heading->addWidget(makeLabel(QStringLiteral("收藏站点"), "sectionTitle"));
    heading->addStretch();
    auto *refreshButton = makeButton(QStringLiteral("刷新"), "ghost");
    refreshButton->setMinimumWidth(76);
    heading->addWidget(refreshButton);
    layout->addLayout(heading);

    m_favoriteListLayout = new QVBoxLayout;
    m_favoriteListLayout->setSpacing(14);
    m_favoriteListLayout->addWidget(makeLabel(QStringLiteral("进入页面后加载收藏站点"), "hint"));
    layout->addLayout(m_favoriteListLayout);
    layout->addStretch();

    pageLayout->addWidget(makeScrollArea(content), 1);
    pageLayout->addWidget(buildBottomNavigation(Profile));
    connect(backButton, &QPushButton::clicked, this, [this]() { showPage(Profile); });
    connect(refreshButton, &QPushButton::clicked, this, &UserWindow::requestFavoriteList);
    return page;
}

QWidget *UserWindow::buildOrdersPage()
{
    auto *page = new QWidget;
    auto *pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(0);

    auto *content = new QWidget;
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(20, 0, 20, 24);
    layout->setSpacing(14);

    auto *top = new QHBoxLayout;
    top->setContentsMargins(0, 16, 0, 0);
    top->setSpacing(12);
    auto *backButton = makeButton(QStringLiteral("←"), "icon");
    backButton->setFixedSize(42, 42);
    backButton->setToolTip(QStringLiteral("返回个人中心"));
    backButton->setAccessibleName(QStringLiteral("返回个人中心"));
    top->addWidget(backButton, 0, Qt::AlignTop);
    top->addWidget(buildPageHeader(QStringLiteral("MY ORDERS"),
                                   QStringLiteral("我的订单"),
                                   QStringLiteral("查看充电、结算与历史记录")), 1);
    layout->addLayout(top);

    auto *overview = makeCard();
    overview->setProperty("variant", "orderOverview");
    auto *overviewLayout = new QHBoxLayout(overview);
    overviewLayout->setContentsMargins(16, 14, 14, 14);
    overviewLayout->setSpacing(12);
    auto *icon = makeLabel(QStringLiteral("单"), "orderPageIcon");
    icon->setFixedSize(46, 46);
    icon->setAlignment(Qt::AlignCenter);
    overviewLayout->addWidget(icon);
    auto *overviewText = new QVBoxLayout;
    overviewText->setSpacing(2);
    overviewText->addWidget(makeLabel(QStringLiteral("充电订单"), "cardTitle"));
    overviewText->addWidget(makeLabel(QStringLiteral("订单状态与费用信息一目了然"), "caption"));
    overviewLayout->addLayout(overviewText, 1);
    auto *refreshButton = makeButton(QStringLiteral("刷新"), "ghost");
    refreshButton->setMinimumWidth(76);
    overviewLayout->addWidget(refreshButton);
    layout->addWidget(overview);

    auto *heading = new QHBoxLayout;
    heading->addWidget(makeLabel(QStringLiteral("全部订单"), "sectionTitle"));
    heading->addStretch();
    heading->addWidget(makeLabel(QStringLiteral("最近 20 条"), "caption"));
    layout->addLayout(heading);

    m_orderListLayout = new QVBoxLayout;
    m_orderListLayout->setSpacing(12);
    m_orderListLayout->addWidget(makeLabel(QStringLiteral("进入页面后加载订单记录"), "hint"));
    layout->addLayout(m_orderListLayout);
    layout->addStretch();

    pageLayout->addWidget(makeScrollArea(content), 1);
    pageLayout->addWidget(buildBottomNavigation(Profile));
    connect(backButton, &QPushButton::clicked, this, [this]() { showPage(Profile); });
    connect(refreshButton, &QPushButton::clicked, this, [this]() {
        sendRequest(MessageTypes::UserOrderList,
                    QJsonObject{{QStringLiteral("page"), 1},
                                {QStringLiteral("pageSize"), 20}});
    });
    return page;
}

QWidget *UserWindow::buildCouponsPage()
{
    auto *page = new QWidget;
    auto *pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(0);
    auto *content = new QWidget;
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(20, 0, 20, 24);
    layout->setSpacing(14);
    auto *top = new QHBoxLayout;
    top->setContentsMargins(0, 16, 0, 0);
    top->setSpacing(12);
    auto *backButton = makeButton(QStringLiteral("←"), "icon");
    backButton->setFixedSize(42, 42);
    backButton->setToolTip(QStringLiteral("返回个人中心"));
    top->addWidget(backButton, 0, Qt::AlignTop);
    top->addWidget(buildPageHeader(QStringLiteral("MY COUPONS"),
                                   QStringLiteral("我的优惠券"),
                                   QStringLiteral("查看可用、锁定和已使用的优惠权益")), 1);
    layout->addLayout(top);
    auto *heading = new QHBoxLayout;
    heading->addWidget(makeLabel(QStringLiteral("优惠券列表"), "sectionTitle"));
    heading->addStretch();
    auto *refreshButton = makeButton(QStringLiteral("刷新"), "ghost");
    refreshButton->setMinimumWidth(76);
    heading->addWidget(refreshButton);
    layout->addLayout(heading);
    m_couponListLayout = new QVBoxLayout;
    m_couponListLayout->setSpacing(12);
    m_couponListLayout->addWidget(makeLabel(QStringLiteral("进入页面后加载优惠券"), "hint"));
    layout->addLayout(m_couponListLayout);
    layout->addStretch();
    pageLayout->addWidget(makeScrollArea(content), 1);
    pageLayout->addWidget(buildBottomNavigation(Profile));
    connect(backButton, &QPushButton::clicked, this, [this]() { showPage(Profile); });
    connect(refreshButton, &QPushButton::clicked, this,
            [this]() { sendRequest(MessageTypes::UserCouponList); });
    return page;
}

QWidget *UserWindow::buildMembershipPage()
{
    auto *page = new QWidget;
    auto *pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(0);
    auto *content = new QWidget;
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(20, 0, 20, 24);
    layout->setSpacing(14);
    auto *top = new QHBoxLayout;
    top->setContentsMargins(0, 16, 0, 0);
    top->setSpacing(12);
    auto *backButton = makeButton(QStringLiteral("←"), "icon");
    backButton->setFixedSize(42, 42);
    backButton->setToolTip(QStringLiteral("返回个人中心"));
    top->addWidget(backButton, 0, Qt::AlignTop);
    top->addWidget(buildPageHeader(QStringLiteral("EVCHARGE VIP"),
                                   QStringLiteral("会员中心"),
                                   QStringLiteral("解锁会员权益，享受充电服务费优惠")), 1);
    layout->addLayout(top);
    auto *statusCard = makeCard();
    statusCard->setProperty("variant", "membershipHero");
    auto *statusLayout = new QVBoxLayout(statusCard);
    statusLayout->setContentsMargins(20, 18, 20, 18);
    statusLayout->setSpacing(7);
    statusLayout->addWidget(makeLabel(QStringLiteral("EVCHARGE · MEMBER"), "membershipEyebrow"));
    m_membershipStatusLabel = makeLabel(QStringLiteral("普通用户"), "membershipTitle");
    statusLayout->addWidget(m_membershipStatusLabel);
    m_membershipPeriodLabel = makeLabel(QStringLiteral("暂无有效会员权益"), "membershipText");
    statusLayout->addWidget(m_membershipPeriodLabel);
    m_membershipBalanceLabel = makeLabel(QStringLiteral("钱包余额：--"), "membershipText");
    statusLayout->addWidget(m_membershipBalanceLabel);
    layout->addWidget(statusCard);
    layout->addWidget(makeLabel(QStringLiteral("选择会员卡"), "sectionTitle"));
    m_membershipProductLayout = new QVBoxLayout;
    m_membershipProductLayout->setSpacing(12);
    m_membershipProductLayout->addWidget(makeLabel(QStringLiteral("正在加载会员卡…"), "hint"));
    layout->addLayout(m_membershipProductLayout);
    layout->addWidget(makeLabel(
        QStringLiteral("会员仅折扣服务费，不改变基础电价；订单按创建时的会员权益结算。"),
        "hint"));
    layout->addStretch();
    pageLayout->addWidget(makeScrollArea(content), 1);
    pageLayout->addWidget(buildBottomNavigation(Profile));
    connect(backButton, &QPushButton::clicked, this, [this]() { showPage(Profile); });
    return page;
}

QWidget *UserWindow::buildOrderCard(const QJsonObject &order)
{
    auto *orderCard = makeCard();
    orderCard->setProperty("variant", "order");
    auto *layout = new QVBoxLayout(orderCard);
    layout->setContentsMargins(17, 15, 17, 15);
    layout->setSpacing(9);

    auto *titleRow = new QHBoxLayout;
    titleRow->setSpacing(10);
    titleRow->addWidget(makeLabel(order.value(QStringLiteral("stationName")).toString(),
                                  "cardTitle"), 1);
    const QString status = order.value(QStringLiteral("status")).toString();
    const QString displayStatus = orderStatusText(status);
    const char *statusRole = status == QStringLiteral("CHARGING") ? "badgeGood"
                           : status == QStringLiteral("PENDING_PAYMENT") ? "badgeWarn"
                           : "badgeNeutral";
    auto *statusLabel = makeLabel(displayStatus, statusRole);
    statusLabel->setAlignment(Qt::AlignCenter);
    titleRow->addWidget(statusLabel, 0, Qt::AlignTop);
    layout->addLayout(titleRow);

    const QString orderNo = order.value(QStringLiteral("orderNo")).toString();
    layout->addWidget(makeLabel(QStringLiteral("订单号  %1").arg(
        orderNo.isEmpty() ? QStringLiteral("--") : orderNo), "orderNumber"));

    auto *details = new QHBoxLayout;
    details->setSpacing(8);
    const int seconds = order.value(QStringLiteral("chargeSeconds")).toInt();
    details->addWidget(makeLabel(QStringLiteral("电桩\n%1").arg(
        order.value(QStringLiteral("pileNo")).toString()), "orderDetail"), 1);
    details->addWidget(makeLabel(QStringLiteral("电量\n%1 kWh").arg(
        order.value(QStringLiteral("energyKwh")).toDouble(), 0, 'f', 2), "orderDetail"), 1);
    details->addWidget(makeLabel(QStringLiteral("时长\n%1 分钟").arg(seconds / 60),
                                 "orderDetail"), 1);
    layout->addLayout(details);

    auto *footer = new QHBoxLayout;
    footer->addWidget(makeLabel(order.value(QStringLiteral("createdAt")).toString(),
                                "caption"), 1);
    footer->addWidget(makeLabel(QStringLiteral("实付  %1").arg(displayMoney(
        order.value(QStringLiteral("amountFen")).toInt())), "orderAmount"));
    layout->addLayout(footer);
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
    if (page != Promotion && m_promotionTimer) {
        m_promotionTimer->stop();
    }
    m_pages->setCurrentIndex(static_cast<int>(page));
    if (page == Home && !m_homeSheetInitialized && m_homeSplitter) {
        QTimer::singleShot(0, this, [this]() {
            if (m_homeSplitter && m_homeSplitter->height() > 0) {
                setHomeSheetHeight(m_homeSplitter->height() / 2);
                m_homeSheetInitialized = true;
            }
        });
    }
    if (m_sessionMode == SessionMode::Real && !m_sessionId.isEmpty()
        && m_socketClient->isConnected()) {
        if (page == Charging) {
            requestActiveOrder();
        } else if (page == Profile) {
            sendRequest(MessageTypes::UserProfileGet);
            sendRequest(MessageTypes::MembershipProductList);
        } else if (page == Favorites) {
            requestFavoriteList();
        } else if (page == Orders) {
            sendRequest(MessageTypes::UserOrderList,
                        QJsonObject{{QStringLiteral("page"), 1},
                                    {QStringLiteral("pageSize"), 20}});
        } else if (page == Coupons) {
            sendRequest(MessageTypes::UserCouponList);
        } else if (page == Membership) {
            sendRequest(MessageTypes::UserProfileGet);
            sendRequest(MessageTypes::MembershipProductList);
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
    if (m_energyFlow) m_energyFlow->setCharging(charging);
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
    QDialog dialog(this, Qt::Dialog | Qt::FramelessWindowHint);
    dialog.setObjectName(QStringLiteral("userConfirmDialog"));
    dialog.setModal(true);
    dialog.setAttribute(Qt::WA_TranslucentBackground);
    dialog.setMinimumWidth(400);

    auto *outerLayout = new QVBoxLayout(&dialog);
    outerLayout->setContentsMargins(18, 18, 18, 18);
    auto *card = new QFrame(&dialog);
    card->setObjectName(QStringLiteral("confirmCard"));
    auto *shadow = new QGraphicsDropShadowEffect(card);
    shadow->setBlurRadius(36);
    shadow->setOffset(0, 12);
    shadow->setColor(QColor(15, 44, 37, 70));
    card->setGraphicsEffect(shadow);
    outerLayout->addWidget(card);

    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(28, 20, 28, 28);
    layout->setSpacing(12);
    auto *topRow = new QHBoxLayout;
    topRow->addStretch();
    auto *closeButton = makeButton(QStringLiteral("×"), "dialogClose");
    closeButton->setFixedSize(36, 36);
    closeButton->setAccessibleName(QStringLiteral("关闭充值弹窗"));
    topRow->addWidget(closeButton);
    layout->addLayout(topRow);

    auto *icon = makeLabel(QStringLiteral("¥"), "dialogIcon");
    icon->setAlignment(Qt::AlignCenter);
    icon->setFixedSize(58, 58);
    layout->addWidget(icon, 0, Qt::AlignHCenter);
    auto *title = makeLabel(QStringLiteral("钱包充值"), "dialogTitle");
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);
    auto *subtitle = makeLabel(QStringLiteral("选择常用金额或输入自定义金额"), "dialogDetail");
    subtitle->setAlignment(Qt::AlignCenter);
    layout->addWidget(subtitle);

    auto *amountEdit = new QLineEdit;
    amountEdit->setProperty("role", "rechargeAmount");
    amountEdit->setPlaceholderText(QStringLiteral("请输入充值金额（元）"));
    amountEdit->setAlignment(Qt::AlignCenter);
    amountEdit->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("[0-9]{0,7}(\\.[0-9]{0,2})?")), amountEdit));
    layout->addWidget(amountEdit);

    auto *quickAmounts = new QHBoxLayout;
    quickAmounts->setSpacing(9);
    for (const int amount : {20, 50, 100}) {
        auto *quickButton = makeButton(QStringLiteral("¥%1").arg(amount), "rechargeQuick");
        quickButton->setMinimumHeight(38);
        quickAmounts->addWidget(quickButton, 1);
        connect(quickButton, &QPushButton::clicked, amountEdit,
                [amountEdit, amount]() { amountEdit->setText(QString::number(amount)); });
    }
    layout->addLayout(quickAmounts);

    auto *errorLabel = makeLabel(QString(), "rechargeError");
    errorLabel->setAlignment(Qt::AlignCenter);
    errorLabel->setVisible(false);
    layout->addWidget(errorLabel);
    auto *confirmButton = makeButton(QStringLiteral("确认充值"), "dialogPrimary");
    auto *cancelButton = makeButton(QStringLiteral("暂不充值"), "dialogSecondary");
    confirmButton->setMinimumHeight(50);
    cancelButton->setMinimumHeight(46);
    layout->addWidget(confirmButton);
    layout->addWidget(cancelButton);

    int amountFen = 0;
    const auto submitRecharge = [&dialog, amountEdit, errorLabel, &amountFen]() {
        bool valid = false;
        const double amountYuan = amountEdit->text().toDouble(&valid);
        amountFen = qRound(amountYuan * 100.0);
        if (!valid || amountFen <= 0 || amountFen > 100000000) {
            errorLabel->setText(QStringLiteral("请输入 0.01 至 1,000,000.00 元之间的金额"));
            errorLabel->setVisible(true);
            return;
        }
        dialog.accept();
    };
    connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(confirmButton, &QPushButton::clicked, &dialog, submitRecharge);
    connect(amountEdit, &QLineEdit::returnPressed, &dialog, submitRecharge);
    amountEdit->setFocus();
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    sendRequest(MessageTypes::UserRecharge,
                QJsonObject{{QStringLiteral("amountFen"), amountFen}});
}

void UserWindow::showRenameDialog()
{
    QDialog dialog(this, Qt::Dialog | Qt::FramelessWindowHint);
    dialog.setObjectName(QStringLiteral("userConfirmDialog"));
    dialog.setModal(true);
    dialog.setAttribute(Qt::WA_TranslucentBackground);
    dialog.setMinimumWidth(400);
    auto *outerLayout = new QVBoxLayout(&dialog);
    outerLayout->setContentsMargins(18, 18, 18, 18);
    auto *card = new QFrame(&dialog);
    card->setObjectName(QStringLiteral("confirmCard"));
    auto *shadow = new QGraphicsDropShadowEffect(card);
    shadow->setBlurRadius(36);
    shadow->setOffset(0, 12);
    shadow->setColor(QColor(15, 44, 37, 70));
    card->setGraphicsEffect(shadow);
    outerLayout->addWidget(card);
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(28, 20, 28, 28);
    layout->setSpacing(12);
    auto *topRow = new QHBoxLayout;
    topRow->addStretch();
    auto *closeButton = makeButton(QStringLiteral("×"), "dialogClose");
    closeButton->setFixedSize(36, 36);
    topRow->addWidget(closeButton);
    layout->addLayout(topRow);
    auto *icon = makeLabel(QStringLiteral("名"), "dialogIcon");
    icon->setAlignment(Qt::AlignCenter);
    icon->setFixedSize(58, 58);
    layout->addWidget(icon, 0, Qt::AlignHCenter);
    auto *title = makeLabel(QStringLiteral("修改昵称"), "dialogTitle");
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);
    auto *subtitle = makeLabel(QStringLiteral("设置一个便于识别的新昵称（2–20 个字符）"),
                               "dialogDetail");
    subtitle->setAlignment(Qt::AlignCenter);
    layout->addWidget(subtitle);
    auto *nicknameEdit = new QLineEdit(m_nicknameLabel->text());
    nicknameEdit->setProperty("role", "renameInput");
    nicknameEdit->setPlaceholderText(QStringLiteral("请输入新昵称"));
    nicknameEdit->setAlignment(Qt::AlignCenter);
    nicknameEdit->setMaxLength(20);
    layout->addWidget(nicknameEdit);
    auto *errorLabel = makeLabel(QStringLiteral("昵称长度应为 2–20 个字符"), "rechargeError");
    errorLabel->setAlignment(Qt::AlignCenter);
    errorLabel->setVisible(false);
    layout->addWidget(errorLabel);
    auto *saveButton = makeButton(QStringLiteral("保存昵称"), "dialogPrimary");
    auto *cancelButton = makeButton(QStringLiteral("暂不修改"), "dialogSecondary");
    saveButton->setMinimumHeight(50);
    cancelButton->setMinimumHeight(46);
    layout->addWidget(saveButton);
    layout->addWidget(cancelButton);
    const auto submitRename = [&dialog, nicknameEdit, errorLabel]() {
        if (nicknameEdit->text().trimmed().size() < 2) {
            errorLabel->setVisible(true);
            return;
        }
        dialog.accept();
    };
    connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(saveButton, &QPushButton::clicked, &dialog, submitRename);
    connect(nicknameEdit, &QLineEdit::returnPressed, &dialog, submitRename);
    nicknameEdit->setFocus();
    nicknameEdit->selectAll();
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const QString nickname = nicknameEdit->text().trimmed();
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
    sendRequest(MessageTypes::UserCouponList);
    sendRequest(MessageTypes::MembershipProductList);
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

void UserWindow::requestFavoriteList()
{
    if (!m_favoriteListLayout) return;
    if (!m_socketClient->isConnected() || m_sessionId.isEmpty()) {
        clearLayout(m_favoriteListLayout);
        m_favoriteListLayout->addWidget(makeLabel(
            QStringLiteral("请连接服务并登录后查看收藏。"), "hint"));
        return;
    }
    clearLayout(m_favoriteListLayout);
    m_favoriteListLayout->addWidget(makeLabel(QStringLiteral("正在加载收藏站点…"), "hint"));
    sendRequest(MessageTypes::UserStationFavoriteList,
                QJsonObject{{QStringLiteral("longitude"), m_originLongitude},
                            {QStringLiteral("latitude"), m_originLatitude}});
}

void UserWindow::toggleFavorite(int stationId)
{
    if (stationId <= 0) {
        showNotice(QStringLiteral("站点信息不完整，暂时无法收藏"), true);
        return;
    }
    if (!m_socketClient->isConnected() || m_sessionId.isEmpty()) {
        showNotice(QStringLiteral("请连接服务并登录后再收藏"), true);
        return;
    }
    sendRequest(MessageTypes::UserStationFavoriteToggle,
                QJsonObject{{QStringLiteral("stationId"), stationId}});
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
    if (m_profileIdLabel) m_profileIdLabel->setText(QStringLiteral("ID：%1").arg(
        static_cast<qint64>(user.value(QStringLiteral("userId")).toDouble())));
    if (m_profileStatusLabel) m_profileStatusLabel->setText(QStringLiteral("账户状态：%1").arg(userStatusText(user.value(QStringLiteral("status")).toString())));
    m_balanceFenInFen = user.value(QStringLiteral("balanceFen")).toInt();
    m_isMember = user.value(QStringLiteral("isMember")).toBool();
    m_membershipRemainingDays = user.value(QStringLiteral("membershipRemainingDays")).toInt();
    m_membershipExpiresAt = user.value(QStringLiteral("membershipExpiresAt")).toString();
    m_membershipDiscountBps = user.value(QStringLiteral("membershipDiscountBps")).toInt(10000);
    if (m_balanceLabel) m_balanceLabel->setText(displayMoney(m_balanceFenInFen));
    renderMembership();
    if (m_avatarLabel) {
        m_avatarLabel->setProperty("vip", m_isMember);
        if (m_isMember) {
            auto *vipGlow = new QGraphicsDropShadowEffect(m_avatarLabel);
            vipGlow->setBlurRadius(22);
            vipGlow->setOffset(0, 0);
            vipGlow->setColor(QColor(255, 207, 82, 210));
            m_avatarLabel->setGraphicsEffect(vipGlow);
        } else {
            m_avatarLabel->setGraphicsEffect(nullptr);
        }
        m_avatarLabel->style()->unpolish(m_avatarLabel);
        m_avatarLabel->style()->polish(m_avatarLabel);
    }

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
    m_displayStations = stations;
    bool selectedStillVisible = false;
    for (const QJsonValue &value : m_displayStations) {
        if (value.toObject().value(QStringLiteral("stationId")).toInt()
            == m_selectedHomeStationId) {
            selectedStillVisible = true;
            break;
        }
    }
    if (!selectedStillVisible) {
        m_selectedHomeStationId = -1;
    }
    if (m_stationMap) {
        m_stationMap->setSelectedStation(m_selectedHomeStationId);
        m_stationMap->setStations(m_displayStations);
    }
    m_stationCards.clear();
    clearLayout(m_stationListLayout);
    if (stations.isEmpty()) {
        m_stationListLayout->addWidget(makeLabel(QStringLiteral("当前没有可展示的充电站"), "hint"));
        return;
    }
    for (const QJsonValue &value : stations) {
        m_stationListLayout->addWidget(buildStationCard(value.toObject()));
    }
}

void UserWindow::showPromotion()
{
    m_promotionSecondsRemaining = 3;
    updatePromotionSkipText();
    showPage(Promotion);
    if (m_promotionTimer) {
        m_promotionTimer->start();
    }
}

void UserWindow::finishPromotion()
{
    if (m_promotionTimer) {
        m_promotionTimer->stop();
    }
    if (m_pages->currentIndex() == static_cast<int>(Promotion)) {
        showPage(Home);
    }
}

void UserWindow::updatePromotionSkipText()
{
    const auto *promotionPage = m_pages->widget(static_cast<int>(Promotion));
    if (const auto skipButton = promotionPage->findChild<QPushButton *>()) {
        skipButton->setText(QStringLiteral("跳过 %1s").arg(m_promotionSecondsRemaining));
    }
}

void UserWindow::selectHomeStation(int stationId)
{
    bool exists = false;
    for (const QJsonValue &value : m_displayStations) {
        if (value.toObject().value(QStringLiteral("stationId")).toInt() == stationId) {
            exists = true;
            break;
        }
    }
    if (!exists) {
        return;
    }
    m_selectedHomeStationId = stationId;
    if (m_stationMap) {
        m_stationMap->setSelectedStation(stationId);
    }
    for (auto it = m_stationCards.begin(); it != m_stationCards.end(); ++it) {
        QWidget *card = it.value();
        if (!card) {
            continue;
        }
        card->setProperty("selected", it.key() == stationId);
        card->style()->unpolish(card);
        card->style()->polish(card);
    }
    if (m_stationSheet) {
        m_stationSheet->ensureWidgetVisible(m_stationCards.value(stationId));
    }
}

void UserWindow::setHomeSheetHeight(int sheetHeight)
{
    if (!m_homeSplitter) {
        return;
    }
    const int totalHeight = m_homeSplitter->height();
    if (totalHeight <= 0) {
        return;
    }
    const int minimumSheetHeight = qMin(210, totalHeight);
    const int targetHeight = qBound(minimumSheetHeight, sheetHeight, totalHeight);
    m_homeSplitter->setSizes({qMax(0, totalHeight - targetHeight), targetHeight});
}

void UserWindow::snapHomeSheet()
{
    if (!m_homeSplitter || !m_stationSheet) {
        return;
    }
    const int totalHeight = m_homeSplitter->height();
    if (totalHeight <= 0) {
        return;
    }
    const int halfHeight = qMax(qMin(210, totalHeight), totalHeight / 2);
    const int threshold = (halfHeight + totalHeight) / 2;
    setHomeSheetHeight(m_stationSheet->height() >= threshold ? totalHeight : halfHeight);
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
    if (m_stationFavoriteButton) {
        const bool isFavorite = station.value(QStringLiteral("isFavorite")).toBool();
        m_stationFavoriteButton->setText(isFavorite ? QStringLiteral("★ 已收藏")
                                                     : QStringLiteral("☆ 收藏"));
        m_stationFavoriteButton->setProperty("kind",
            isFavorite ? "favoriteActive" : "favorite");
        m_stationFavoriteButton->setToolTip(isFavorite ? QStringLiteral("取消收藏")
                                                        : QStringLiteral("收藏站点"));
        m_stationFavoriteButton->style()->unpolish(m_stationFavoriteButton);
        m_stationFavoriteButton->style()->polish(m_stationFavoriteButton);
    }
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
        m_orderListLayout->addWidget(buildOrderCard(value.toObject()));
    }
}

void UserWindow::renderFavorites(const QJsonArray &stations)
{
    m_favoriteStations = stations;
    clearLayout(m_favoriteListLayout);
    if (stations.isEmpty()) {
        m_favoriteListLayout->addWidget(makeLabel(
            QStringLiteral("还没有收藏站点。可在首页或站点详情点击星标收藏。"), "hint"));
        return;
    }
    for (const QJsonValue &value : stations) {
        QJsonObject station = value.toObject();
        station.insert(QStringLiteral("isFavorite"), true);
        m_favoriteListLayout->addWidget(buildStationCard(station));
    }
}

void UserWindow::renderCoupons(const QJsonArray &coupons)
{
    if (!m_couponListLayout) return;
    clearLayout(m_couponListLayout);
    if (coupons.isEmpty()) {
        auto *emptyCard = makeCard();
        emptyCard->setProperty("variant", "emptyState");
        auto *emptyLayout = new QVBoxLayout(emptyCard);
        emptyLayout->setContentsMargins(24, 34, 24, 34);
        emptyLayout->setSpacing(8);
        auto *icon = makeLabel(QStringLiteral("券"), "emptyIcon");
        icon->setFixedSize(58, 58);
        icon->setAlignment(Qt::AlignCenter);
        emptyLayout->addWidget(icon, 0, Qt::AlignHCenter);
        auto *title = makeLabel(QStringLiteral("暂无优惠券"), "cardTitle");
        title->setAlignment(Qt::AlignCenter);
        emptyLayout->addWidget(title);
        auto *hint = makeLabel(QStringLiteral("获得优惠券后会在这里显示"), "caption");
        hint->setAlignment(Qt::AlignCenter);
        emptyLayout->addWidget(hint);
        m_couponListLayout->addWidget(emptyCard);
        return;
    }
    for (const QJsonValue &value : coupons) {
        const QJsonObject coupon = value.toObject();
        const QString status = coupon.value(QStringLiteral("status")).toString();
        const QString statusName = status == QStringLiteral("AVAILABLE") ? QStringLiteral("可使用")
            : status == QStringLiteral("LOCKED") ? QStringLiteral("订单已选用") : QStringLiteral("已使用");
        auto *card = makeCard();
        card->setProperty("variant", "coupon");
        card->setProperty("available", status == QStringLiteral("AVAILABLE"));
        auto *cardLayout = new QHBoxLayout(card);
        cardLayout->setContentsMargins(18, 16, 16, 16);
        cardLayout->setSpacing(14);
        auto *discount = makeLabel(QStringLiteral("8折"), "couponDiscount");
        discount->setFixedWidth(72);
        discount->setAlignment(Qt::AlignCenter);
        cardLayout->addWidget(discount);
        auto *textLayout = new QVBoxLayout;
        textLayout->setSpacing(5);
        textLayout->addWidget(makeLabel(QStringLiteral("充电服务费优惠券"), "cardTitle"));
        textLayout->addWidget(makeLabel(QStringLiteral("券号 #%1").arg(static_cast<qint64>(
            coupon.value(QStringLiteral("couponId")).toDouble())), "caption"));
        textLayout->addWidget(makeLabel(QStringLiteral("下发时间：%1").arg(
            coupon.value(QStringLiteral("issuedAt")).toString()), "caption"));
        cardLayout->addLayout(textLayout, 1);
        auto *badge = makeLabel(statusName, status == QStringLiteral("AVAILABLE")
            ? "badgeGood" : status == QStringLiteral("LOCKED") ? "badgeWarn" : "badgeNeutral");
        badge->setAlignment(Qt::AlignCenter);
        cardLayout->addWidget(badge, 0, Qt::AlignTop);
        m_couponListLayout->addWidget(card);
    }
}

void UserWindow::renderMembership()
{
    if (m_membershipStatusLabel) {
        m_membershipStatusLabel->setText(m_isMember ? QStringLiteral("VIP 会员 · 有效")
                                                    : QStringLiteral("普通用户"));
    }
    if (m_membershipPeriodLabel) {
        m_membershipPeriodLabel->setText(m_isMember
            ? QStringLiteral("剩余 %1 天 · 到期时间 %2").arg(m_membershipRemainingDays)
                  .arg(m_membershipExpiresAt)
            : QStringLiteral("暂无有效会员权益"));
    }
    if (m_membershipBalanceLabel) {
        m_membershipBalanceLabel->setText(QStringLiteral("钱包余额：%1").arg(
            displayMoney(m_balanceFenInFen)));
    }
    if (!m_membershipProductLayout) return;
    clearLayout(m_membershipProductLayout);
    if (m_membershipProducts.isEmpty()) {
        m_membershipProductLayout->addWidget(makeLabel(QStringLiteral("暂无可购买的会员卡"), "hint"));
        return;
    }
    for (const QJsonValue &value : m_membershipProducts) {
        const QJsonObject productData = value.toObject();
        const bool month = productData.value(QStringLiteral("cardType")).toString()
            == QStringLiteral("MONTH");
        const QString productNo = productData.value(QStringLiteral("productNo")).toString();
        const qint64 priceFen = static_cast<qint64>(
            productData.value(QStringLiteral("salePriceFen")).toDouble());
        const int discountBps = productData.value(QStringLiteral("serviceFeeDiscountBps")).toInt(8000);
        auto *product = makeCard();
        product->setProperty("variant", "membershipProduct");
        product->setProperty("cardType", month ? "month" : "season");
        auto *productLayout = new QHBoxLayout(product);
        productLayout->setContentsMargins(18, 16, 16, 16);
        productLayout->setSpacing(12);
        auto *textLayout = new QVBoxLayout;
        textLayout->setSpacing(5);
        textLayout->addWidget(makeLabel(productData.value(QStringLiteral("name")).toString(),
                                        "cardTitle"));
        textLayout->addWidget(makeLabel(QStringLiteral("有效期 %1 天 · 服务费 %2 折")
            .arg(productData.value(QStringLiteral("durationDays")).toInt())
            .arg(discountBps / 1000.0, 0, 'f', 1), "caption"));
        textLayout->addWidget(makeLabel(displayMoney(priceFen), "membershipPrice"));
        productLayout->addLayout(textLayout, 1);
        auto *buy = makeButton(QStringLiteral("立即购买"), "primary");
        buy->setMinimumWidth(96);
        buy->setProperty("productNo", productNo);
        productLayout->addWidget(buy, 0, Qt::AlignVCenter);
        connect(buy, &QPushButton::clicked, this, [this, buy]() {
            const QString requestId = sendRequest(MessageTypes::MembershipPurchase,
                {{QStringLiteral("productNo"), buy->property("productNo").toString()}});
            if (requestId.isEmpty()) showNotice(QStringLiteral("购买请求发送失败"), true);
            else { buy->setEnabled(false); showNotice(QStringLiteral("正在处理会员购买…")); }
        });
        m_membershipProductLayout->addWidget(product);
    }
}

void UserWindow::applyFavoriteState(int stationId, bool isFavorite)
{
    const auto updateArray = [stationId, isFavorite](QJsonArray &stations, bool removeMissing) {
        QJsonArray updated;
        for (const QJsonValue &value : stations) {
            QJsonObject station = value.toObject();
            if (station.value(QStringLiteral("stationId")).toInt() == stationId) {
                station.insert(QStringLiteral("isFavorite"), isFavorite);
                if (removeMissing && !isFavorite) continue;
            }
            updated.append(station);
        }
        stations = updated;
    };
    updateArray(m_nearbyStations, false);
    updateArray(m_recommendedStations, false);
    updateArray(m_favoriteStations, true);

    if (m_selectedStation.value(QStringLiteral("stationId")).toInt() == stationId) {
        m_selectedStation.insert(QStringLiteral("isFavorite"), isFavorite);
        if (m_stationFavoriteButton) {
            m_stationFavoriteButton->setText(isFavorite ? QStringLiteral("★ 已收藏")
                                                         : QStringLiteral("☆ 收藏"));
            m_stationFavoriteButton->setProperty("kind",
                isFavorite ? "favoriteActive" : "favorite");
            m_stationFavoriteButton->setToolTip(isFavorite ? QStringLiteral("取消收藏")
                                                            : QStringLiteral("收藏站点"));
            m_stationFavoriteButton->style()->unpolish(m_stationFavoriteButton);
            m_stationFavoriteButton->style()->polish(m_stationFavoriteButton);
        }
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
    if (m_pages && m_pages->currentIndex() == static_cast<int>(Favorites)) {
        requestFavoriteList();
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
        if (type == MessageTypes::UserStationFavoriteList && m_favoriteListLayout) {
            clearLayout(m_favoriteListLayout);
            m_favoriteListLayout->addWidget(makeLabel(
                QStringLiteral("收藏服务暂不可用，后端接口完成后即可加载。"), "hint"));
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
        m_couponSnapshotReady = false;
        m_knownCouponIds.clear();
        if (m_couponPollTimer) m_couponPollTimer->start();
        applyUser(data.value(QStringLiteral("user")).toObject());
        requestInitialData();
        showPromotion();
        showNotice(QStringLiteral("登录成功"));
    } else if (type == MessageTypes::MapGeocode) {
        m_originLongitude = data.value(QStringLiteral("longitude")).toDouble();
        m_originLatitude = data.value(QStringLiteral("latitude")).toDouble();
        const QString formatted = data.value(QStringLiteral("formattedAddress")).toString();
        if (!formatted.isEmpty()) m_originName = formatted;
        if (m_stationMap) {
            m_stationMap->setOrigin(m_originName, m_originLongitude, m_originLatitude);
        }
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
    } else if (type == MessageTypes::MembershipProductList) {
        m_membershipProducts = data.value(QStringLiteral("items")).toArray();
        renderMembership();
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
    } else if (type == MessageTypes::UserStationFavoriteList) {
        renderFavorites(data.value(QStringLiteral("stations")).toArray());
    } else if (type == MessageTypes::UserStationFavoriteToggle) {
        const int stationId = data.value(QStringLiteral("stationId")).toInt();
        const bool isFavorite = data.value(QStringLiteral("isFavorite")).toBool();
        applyFavoriteState(stationId, isFavorite);
        showNotice(isFavorite ? QStringLiteral("已加入我的收藏")
                              : QStringLiteral("已取消收藏"));
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
    } else if (type == MessageTypes::MembershipPurchase) {
        applyUser(data.value(QStringLiteral("user")).toObject());
        showNotice(QStringLiteral("VIP购买成功，服务费享受8折"));
    } else if (type == MessageTypes::UserOrderList) {
        renderOrders(data.value(QStringLiteral("items")).toArray());
    } else if (type == MessageTypes::UserCouponList) {
        const QJsonArray coupons = data.value(QStringLiteral("items")).toArray();
        QSet<qint64> currentIds;
        bool receivedNewCoupon = false;
        for (const QJsonValue &value : coupons) {
            const QJsonObject coupon = value.toObject();
            const qint64 id = static_cast<qint64>(
                coupon.value(QStringLiteral("couponId")).toDouble());
            currentIds.insert(id);
            if (m_couponSnapshotReady && !m_knownCouponIds.contains(id)) receivedNewCoupon = true;
        }
        m_coupons = coupons;
        renderCoupons(m_coupons);
        m_knownCouponIds = currentIds;
        if (m_couponSnapshotReady && receivedNewCoupon) {
            QMessageBox::information(this, QStringLiteral("收到优惠券"),
                                     QStringLiteral("您收到一张优惠券！请到我的优惠券查看"));
        }
        m_couponSnapshotReady = true;
    } else if (type == MessageTypes::UserAvatarUpload) {
        applyUser(data.value(QStringLiteral("user")).toObject());
        showNotice(QStringLiteral("头像上传成功"));
    } else if (type == MessageTypes::UserAvatarRemove) {
        applyUser(data.value(QStringLiteral("user")).toObject());
        showNotice(QStringLiteral("头像已移除"));
    }
}
