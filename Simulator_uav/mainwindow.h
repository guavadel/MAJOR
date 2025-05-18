#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStyle>
#include <QHostAddress>
#include "DeviceController.h"
#include <QTimer>
#include <QTcpServer>
#include <QTcpSocket>


QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE


struct Telemetry {
    float latitude;
    float longitude;
    float altitude;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    MainWindow(Ui::MainWindow *ui, const DeviceController &controller, QTcpSocket *socket, const QTimer &telemetryTimer, const QTimer &imageTimer, const Telemetry &currentPosition);
    ~MainWindow();

private slots:
    void on_lnIPAddress_textChanged(const QString &arg1);
    void on_btnConnect_clicked();
    void device_connected();
    void device_disconnected();
    void device_stateChanged(QAbstractSocket::SocketState);
    void device_errorOccurred(QAbstractSocket::SocketError);
    void device_dataReady(QByteArray data);
    void on_btnSend_clicked();
    // void sendTelemetryAndImage();

    void on_sendImageButton_clicked();

    void on_sendTelemetryButton_clicked();
    void sendTelemetryData();


private:
    Ui::MainWindow *ui;
    DeviceController _controller;

    QTcpSocket *socket;
    QTcpServer server;

    QTimer imageTimer;
    QTimer* telemetryTimer;
    Telemetry currentPosition;
    QList<QTcpSocket*> _socketsList;

    //methods
    void setDeviceContoller();
};
#endif // MAINWINDOW_H
