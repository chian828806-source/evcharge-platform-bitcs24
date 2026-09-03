#include "dropzone.h"
#include <QMimeData>
#include <QUrl>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QStyle>
DropZone::DropZone(QWidget*p):QLabel(p){setAcceptDrops(true);setProperty("dragging",false);}
void DropZone::dragEnterEvent(QDragEnterEvent*e){if(e->mimeData()->hasUrls()){setProperty("dragging",true);style()->unpolish(this);style()->polish(this);e->acceptProposedAction();}}
void DropZone::dragLeaveEvent(QDragLeaveEvent*e){setProperty("dragging",false);style()->unpolish(this);style()->polish(this);e->accept();}
void DropZone::dropEvent(QDropEvent*e){setProperty("dragging",false);style()->unpolish(this);style()->polish(this);if(!e->mimeData()->urls().isEmpty())emit fileDropped(e->mimeData()->urls().first().toLocalFile());e->acceptProposedAction();}
