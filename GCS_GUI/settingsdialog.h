#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QSettings>

namespace Ui {
class SettingsDialog;
}

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    ~SettingsDialog();

    int getPort() const;
    bool isLoggingEnabled() const;
    QString getLogFilePath() const;

private slots:
    void on_checkBoxEnableLogging_toggled(bool checked);
    void on_btnBrowseLogFile_clicked();
    void on_buttonBox_accepted();

private:
    Ui::SettingsDialog *ui;
    QSettings settings;

    void loadSettings();
    void saveSettings();
};

#endif // SETTINGSDIALOG_H
