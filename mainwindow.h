#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTcpSocket>
#include <QTcpServer>
#include <QUdpSocket>
#include <QNetworkDatagram>
#include <QAction>

namespace Ui {
    class MainWindow;
}

struct SockInfo {
    QString name;
    QString ip;
    int port;
    bool send;
    bool recv;
    QTcpSocket *tcp;
    QUdpSocket *udp;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void UDP_Bind();
    void UDP_Unbind();

    void TCP_Connect();
    void TCP_Disconnect();
    void TCP_Server_Reset();
    void TCP_Server_Connect();

    void on_sendMSG_clicked();
    void TCP_RecvMSG();
    void UDP_RecvMSG();

    void on_selectTestSuite_clicked();
    void on_runTestSuite_clicked();

private:
    Ui::MainWindow *ui;

    QTcpSocket *TCPclient;
    QTcpServer *TCPserver;
    QTcpSocket *TCPserver_Client;

    QUdpSocket *UDPclient;
    QUdpSocket *UDPserver;

    void appendMSG(QByteArray msg);
    void sendMSG(QByteArray msg);
    void getInfo(QObject *sender, SockInfo *info);
};

#endif // MAINWINDOW_H
