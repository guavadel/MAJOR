#include "settingsdialog.h"
#include "ui_settingsdialog.h"
#include <QFileDialog>
#include <QDebug>

SettingsDialog::SettingsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SettingsDialog),
    settings("xAI", "GCSApp")
{
    ui->setupUi(this);
    loadSettings();

    connect(ui->checkBoxEnableLogging, &QCheckBox::toggled, this, &SettingsDialog::on_checkBoxEnableLogging_toggled);
    connect(ui->btnBrowseLogFile, &QPushButton::clicked, this, &SettingsDialog::on_btnBrowseLogFile_clicked);
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::on_buttonBox_accepted);
}

SettingsDialog::~SettingsDialog()
{
    delete ui;
}

int SettingsDialog::getPort() const
{
    return ui->spinBoxPort->value();
}

bool SettingsDialog::isLoggingEnabled() const
{
    return ui->checkBoxEnableLogging->isChecked();
}

QString SettingsDialog::getLogFilePath() const
{
    return ui->lineEditLogFilePath->text();
}

void SettingsDialog::on_checkBoxEnableLogging_toggled(bool checked)
{
    ui->lineEditLogFilePath->setEnabled(checked);
    ui->btnBrowseLogFile->setEnabled(checked);
}

void SettingsDialog::on_btnBrowseLogFile_clicked()
{
    QString filePath = QFileDialog::getSaveFileName(this, "Select Log File", QDir::homePath(), "Log Files (*.log);;All Files (*)");
    if (!filePath.isEmpty()) {
        ui->lineEditLogFilePath->setText(filePath);
    }
}

void SettingsDialog::on_buttonBox_accepted()
{
    saveSettings();
}

void SettingsDialog::loadSettings()
{
    ui->spinBoxPort->setValue(settings.value("server/port", 12345).toInt());
    ui->checkBoxEnableLogging->setChecked(settings.value("logging/enabled", false).toBool());
    ui->lineEditLogFilePath->setText(settings.value("logging/filePath", QDir::homePath() + "/gcs.log").toString());
    on_checkBoxEnableLogging_toggled(ui->checkBoxEnableLogging->isChecked());
}

void SettingsDialog::saveSettings()
{
    settings.setValue("server/port", ui->spinBoxPort->value());
    settings.setValue("logging/enabled", ui->checkBoxEnableLogging->isChecked());
    settings.setValue("logging/filePath", ui->lineEditLogFilePath->text());
    settings.sync();
}
