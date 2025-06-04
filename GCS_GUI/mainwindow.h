#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "MyTCPServer.h"
#include <QFile> // Added for QFile
#include <QGeoCoordinate>
#include <QTimer>
#include <QGridLayout>
#include <QResizeEvent>
#include <QDateTime>
#include <QSettings>
#include <QTextStream>
#include "settingsdialog.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_btnStartServer_clicked();
    void newClinetConnected();
    void clientDisconnected();
    void clientDataReceived(QString message);
    void on_btnSendToAll_clicked();
    void on_btnSendToSelected_clicked(); // New slot
    void onTelemetryReceived(float latitude, float longitude, float altitude);
    void onImageReceived(const QImage& image);
    void setupGoogleMap(float latitude, float longitude);
    void resizeEvent(QResizeEvent* event) override;
    void updateClientList();
    void on_btnDisconnectClient_clicked();
    void logCommand(const QString& command);
    void updateStatusBar();
    void on_btnSettings_clicked(); // New slot for settings button

private:
    Ui::MainWindow *ui;
    QGridLayout *gridLayout;
    MyTCPServer* _server;
    QTcpServer *server;
    QTcpSocket *clientSocket;
    QTcpSocket *socket;
    QTimer timer;
    QTimer *graphTimer;
    QTimer *statusTimer;
    QTimer *blinkTimer; // Added for blinking effect
    QDateTime serverStartTime;
    QString lastCommand;
    struct Position {
        float latitude;
        float longitude;
        float altitude;
    } currentPosition, data;
    QGeoCoordinate lastOpenedLocation;
    QVector<double> timeData; // For x-axis (time in seconds)
    QVector<double> altitudeData; // For y-axis (altitude)
    double startTime;
    QMap<int, QTcpSocket*> clientSockets;
    QSettings settings;
    QFile logFile;
    QTextStream logStream;
    bool loggingEnabled;
    bool isBlinking; // Added to track blinking state


    void loadSettings();
    void applySettings();
    void setupLogging();
    void logToFile(const QString& message);
    void startBlink(const QString& color); // Added to start blinking
};

#endif // MAINWINDOW_H
