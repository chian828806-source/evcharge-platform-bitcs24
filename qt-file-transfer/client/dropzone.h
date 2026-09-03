#pragma once
#include <QLabel>
class DropZone: public QLabel { Q_OBJECT public: explicit DropZone(QWidget *p=nullptr); signals: void fileDropped(const QString &); protected: void dragEnterEvent(QDragEnterEvent*) override; void dragLeaveEvent(QDragLeaveEvent*) override; void dropEvent(QDropEvent*) override; };
