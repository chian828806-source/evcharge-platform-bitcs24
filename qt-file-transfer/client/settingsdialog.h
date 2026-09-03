#pragma once
#include <QDialog>
namespace Ui{class SettingsDialog;}
class SettingsDialog:public QDialog{Q_OBJECT public:explicit SettingsDialog(QWidget *parent = nullptr);QString host()const;int port()const;signals:void settingsReady(const QString&,int);private slots:void acceptAndEmit();private:Ui::SettingsDialog*ui;};
