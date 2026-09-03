#include "mainwindow.h"
#include <QApplication>
#include <QFile>
int main(int argc,char**argv){QApplication a(argc,argv);QFile q(":/style.qss");if(q.open(QIODevice::ReadOnly))a.setStyleSheet(q.readAll());MainWindow w;w.show();return a.exec();}
