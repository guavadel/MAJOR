#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "MyTCPServer.h"
#include <QFile> // Added for QFile
#include <QGeoCoordinate>
#include <QTimer>
#include <QGridLayout>
#include <QResizeEvent>


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
    void onTelemetryReceived(float latitude, float longitude, float altitude);
    void onImageReceived(const QImage& image);
    void setupGoogleMap(float latitude, float longitude);
    void resizeEvent(QResizeEvent* event) override;


private:
    Ui::MainWindow *ui;
    QGridLayout *gridLayout;
    MyTCPServer* _server;
    QTcpServer *server;
    QTcpSocket *clientSocket;
    QTcpSocket *socket;
    QTimer timer;
    struct Position {
        float latitude;
        float longitude;
        float altitude;
    } currentPosition, data;
    QGeoCoordinate lastOpenedLocation;
};

#endif // MAINWINDOW_H
