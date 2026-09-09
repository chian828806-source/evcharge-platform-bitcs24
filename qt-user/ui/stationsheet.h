/*
 * 功能：首页站点列表抽屉。只在顶部把手区域处理鼠标/触摸拖动，避免干扰地图和列表滚动。
 */
#pragma once

#include <QFrame>

class QScrollArea;

class StationSheet final : public QFrame
{
    Q_OBJECT

public:
    explicit StationSheet(QWidget *parent = nullptr);

    void setContent(QWidget *content);
    void ensureWidgetVisible(QWidget *widget);

signals:
    void dragStarted();
    void dragMoved(int deltaY);
    void dragReleased();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void beginDrag(qreal globalY);
    void updateDrag(qreal globalY);
    void endDrag();

    QWidget *m_grip = nullptr;
    QScrollArea *m_scrollArea = nullptr;
    qreal m_dragStartY = 0.0;
    bool m_dragging = false;
};
