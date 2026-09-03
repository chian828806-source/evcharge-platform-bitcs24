#include "dropzone.h"
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QStyle>
DropZone::DropZone(QWidget*p):QLabel(p){setAcceptDrops(true);}
void DropZone::dragEnterEvent(QDragEnterEvent*e){if(e->mimeData()->hasUrls())e->acceptProposedAction();}
void DropZone::dragLeaveEvent(QDragLeaveEvent*e){e->accept();}
void DropZone::dropEvent(QDropEvent*e){if(!e->mimeData()->urls().isEmpty())emit fileDropped(e->mimeData()->urls().first().toLocalFile());e->acceptProposedAction();}
