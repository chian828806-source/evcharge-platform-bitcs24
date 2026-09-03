#include "settingsdialog.h"
#include "ui_settingsdialog.h"
SettingsDialog::SettingsDialog(QWidget*p):QDialog(p),ui(new Ui::SettingsDialog){ui->setupUi(this);connect(ui->buttonBox,&QDialogButtonBox::accepted,this,&SettingsDialog::acceptAndEmit);connect(ui->buttonBox,&QDialogButtonBox::rejected,this,&QDialog::reject);}
QString SettingsDialog::host()const{return ui->hostEdit->text();} int SettingsDialog::port()const{return ui->portSpin->value();}
void SettingsDialog::acceptAndEmit(){emit settingsReady(host(),port());accept();}
