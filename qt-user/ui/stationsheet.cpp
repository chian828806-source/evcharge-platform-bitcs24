/*
 * 功能：实现首页站点抽屉把手的鼠标与触摸拖动。
 */
#include "stationsheet.h"

#include <QEvent>
#include <QFrame>
#include <QMouseEvent>
#include <QScrollArea>
#include <QTouchEvent>
#include <QVBoxLayout>

namespace {

qreal mouseGlobalY(const QMouseEvent *event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return event->globalPosition().y();
#else
    return event->globalPos().y();
#endif
}

qreal touchGlobalY(const QTouchEvent *event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return event->points().isEmpty() ? 0.0 : event->points().first().globalPosition().y();
#else
    return event->touchPoints().isEmpty() ? 0.0 : event->touchPoints().first().screenPos().y();
#endif
}

}

StationSheet::StationSheet(QWidget *parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("stationSheet"));
    setFrameShape(QFrame::NoFrame);
    setMinimumHeight(180);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_grip = new QWidget(this);
    m_grip->setObjectName(QStringLiteral("stationSheetGrip"));
    m_grip->setFixedHeight(32);
    m_grip->setCursor(Qt::SizeVerCursor);
    m_grip->setAttribute(Qt::WA_AcceptTouchEvents, true);
    auto *gripLayout = new QVBoxLayout(m_grip);
    gripLayout->setContentsMargins(0, 8, 0, 8);
    auto *bar = new QFrame(m_grip);
    bar->setObjectName(QStringLiteral("stationSheetGripBar"));
    bar->setFixedSize(46, 5);
    gripLayout->addWidget(bar, 0, Qt::AlignHCenter | Qt::AlignVCenter);
    m_grip->installEventFilter(this);
    layout->addWidget(m_grip);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    layout->addWidget(m_scrollArea, 1);
}

void StationSheet::setContent(QWidget *content)
{
    if (!content || !m_scrollArea) {
        return;
    }
    if (QWidget *oldContent = m_scrollArea->takeWidget()) {
        oldContent->deleteLater();
    }
    m_scrollArea->setWidget(content);
}

void StationSheet::ensureWidgetVisible(QWidget *widget)
{
    if (m_scrollArea && widget) {
        m_scrollArea->ensureWidgetVisible(widget, 14, 58);
    }
}

bool StationSheet::eventFilter(QObject *watched, QEvent *event)
{
    if (watched != m_grip) {
        return QFrame::eventFilter(watched, event);
    }
    switch (event->type()) {
    case QEvent::MouseButtonPress: {
        const auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            beginDrag(mouseGlobalY(mouseEvent));
            m_grip->grabMouse();
            event->accept();
            return true;
        }
        break;
    }
    case QEvent::MouseMove:
        if (m_dragging) {
            updateDrag(mouseGlobalY(static_cast<QMouseEvent *>(event)));
            event->accept();
            return true;
        }
        break;
    case QEvent::MouseButtonRelease:
        if (m_dragging) {
            m_grip->releaseMouse();
            endDrag();
            event->accept();
            return true;
        }
        break;
    case QEvent::TouchBegin:
        beginDrag(touchGlobalY(static_cast<QTouchEvent *>(event)));
        event->accept();
        return true;
    case QEvent::TouchUpdate:
        if (m_dragging) {
            updateDrag(touchGlobalY(static_cast<QTouchEvent *>(event)));
            event->accept();
            return true;
        }
        break;
    case QEvent::TouchEnd:
    case QEvent::TouchCancel:
        if (m_dragging) {
            endDrag();
            event->accept();
            return true;
        }
        break;
    default:
        break;
    }
    return QFrame::eventFilter(watched, event);
}

void StationSheet::beginDrag(qreal globalY)
{
    m_dragStartY = globalY;
    m_dragging = true;
    emit dragStarted();
}

void StationSheet::updateDrag(qreal globalY)
{
    emit dragMoved(qRound(globalY - m_dragStartY));
}

void StationSheet::endDrag()
{
    m_dragging = false;
    emit dragReleased();
}
