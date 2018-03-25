#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QFile>
#include <QFileDialog>
#include <QHostAddress>
#include <QTextStream>
#include <QProcess>
#include <qglobal.h>
#include <QRegExp>

#include <windows.h>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    // Setup & create basics
    ui->setupUi(this);
    UDPclient = new QUdpSocket(this);
    UDPserver = new QUdpSocket(this);
    TCPclient = new QTcpSocket(this);
    TCPserver_Client = new QTcpSocket(this);
    TCPserver = new QTcpServer(this);
    TCPserver->setMaxPendingConnections(1);

    // Connect desired SIGNALs and SLOTs
    connect(UDPserver, SIGNAL(readyRead()), this, SLOT(UDP_RecvMSG()));
    connect(ui->sUDP_Bind, SIGNAL(clicked()), this, SLOT(UDP_Bind()));

    connect(TCPclient, SIGNAL(readyRead()), this, SLOT(TCP_RecvMSG()));
    connect(TCPclient, SIGNAL(disconnected()), this, SLOT(TCP_Disconnect()));
    connect(ui->cTCP_Connect, SIGNAL(clicked()), this, SLOT(TCP_Connect()));
    connect(ui->cTCP_Disconnect, SIGNAL(clicked()), this, SLOT(TCP_Disconnect()));

    connect(ui->sTCP_Reset, SIGNAL(clicked()), this, SLOT(TCP_Server_Reset()));
    connect(TCPserver, SIGNAL(newConnection()), this, SLOT(TCP_Server_Connect()));

    connect(ui->clearRECV, SIGNAL(clicked()), ui->recvData, SLOT(clear()));

    // Start servers listening
    ui->sUDP_Bind->clicked();
    ui->sTCP_Reset->clicked();

    // Clear label
    ui->infoLabel->clear();
}

MainWindow::~MainWindow()
{
    delete TCPclient;
    delete TCPserver_Client;
    delete TCPserver;

    delete UDPclient;
    delete UDPserver;

    delete ui;
}

void MainWindow::UDP_Bind()
{
    // Get current info
    SockInfo info;
    getInfo(sender(), &info);

    // Unbind if already bound
    UDP_Unbind();
    if (ui->infoLabel->text().contains("Failed"))
    {
        return;
    }

    // Bind to socket
    info.udp->bind(info.port);

    // Need to wait for connection
    QString msg = ui->infoLabel->text();
    QString conPeer = QString::number(info.port) + "! ";
    if ((info.udp->state() != QUdpSocket::BoundState)
            && !info.udp->waitForConnected(5000))
    {
        msg += "Failed to bind to " + conPeer;
    } else
    {
        msg += "Bound UDP Server to " + conPeer;
    }

    // Update info label
    ui->infoLabel->setText(msg);
}

void MainWindow::UDP_Unbind()
{
    // Get current info
    SockInfo info;
    getInfo(sender(), &info);

    // Collect connection info before disconnect
    QString conPeer = QString::number(info.udp->localPort()) + "! ";

    // Unbind if port bound
    if (info.udp->state() == QUdpSocket::BoundState)
    {
        info.udp->disconnectFromHost();
    } else
    {
        ui->infoLabel->setText(info.name + " not currently bound! ");
        return;
    }

    // Need to wait for disconnection
    QString disMsg;
    if ((info.udp->state() == QUdpSocket::BoundState)
            && !info.udp->waitForDisconnected(5000))
    {
        disMsg = "Failed to unbind UDP Server from " + conPeer;
    } else
    {
        disMsg = "Unbound UDP Server from " + conPeer;
    }

    ui->infoLabel->setText(disMsg);
}

void MainWindow::TCP_Connect()
{
    // Disconnect from existing TCP socket
    TCP_Disconnect();
    if (ui->infoLabel->text().contains("Failed"))
    {
        return;
    }

    // Get current info
    SockInfo info;
    getInfo(sender(), &info);

    // Setup & connect to new TCP socket
    info.tcp->connectToHost(info.ip, info.port, QIODevice::ReadWrite);

    // Need to wait for TCP connection
    QString msg = ui->infoLabel->text();
    QString conPeer = info.ip + ":" + QString::number(info.port) + "! ";
    if ((info.tcp->state() != QUdpSocket::ConnectedState)
            && !info.tcp->waitForConnected(5000))
    {
        msg += "Failed to connect TCP Client to " + conPeer;
    } else
    {
        msg += "Connected TCP Client to " + conPeer;
    }

    // Update info label
    ui->infoLabel->setText(msg);
}

void MainWindow::TCP_Disconnect()
{
    // Get current info
    SockInfo info;
    getInfo(sender(), &info);
    QString conPeer;

    // Set error if lost connection
    if ((sender() == info.tcp) && (info.tcp->state() != QTcpSocket::ConnectedState))
    {
        conPeer = info.tcp->peerAddress().toString() + ":" + QString::number(info.tcp->peerPort());
        ui->infoLabel->setText(info.name + " Connection Lost to " + conPeer + "! ");
        return;
    }

    // Collect connection info before disconnect
    conPeer = info.tcp->peerAddress().toString() + ":";
    conPeer += QString::number(info.tcp->localPort()) + "! ";

    // Disconnect if connected
    if (info.tcp->state() == QTcpSocket::ConnectedState)
    {
        info.tcp->disconnectFromHost();
    } else
    {
        ui->infoLabel->setText(info.name + " not currently connected! ");
        return;
    }

    // Need to wait for disconnection
    QString disMsg = "";
    if ((info.tcp->state() != QTcpSocket::UnconnectedState)
            && !info.tcp->waitForDisconnected(5000))
    {
        disMsg = "Failed to disconnect TCP from " + conPeer;
    } else
    {
        disMsg = "TCP disconnected from " + conPeer;
    }

    ui->infoLabel->setText(disMsg);
}

void MainWindow::TCP_Server_Reset()
{
    // Get current info
    SockInfo info;
    getInfo(sender(), &info);

    // Disconnect from existing TCP socket
    TCP_Disconnect();
    if (ui->infoLabel->text().contains("Failed"))
    {
        return;
    }

    TCPserver->close();
    TCPserver->listen(QHostAddress::Any, info.port);
}

void MainWindow::TCP_Server_Connect()
{
    // Disconnect old connection (should not be called)
    if (TCPserver_Client->state() == QTcpSocket::ConnectedState)
    {
        TCP_Disconnect();
        if (ui->infoLabel->text().contains("Failed"))
        {
            return;
        }
        delete TCPserver_Client;
    }

    // Assign the waiting connection to TCPserver_Client
    if (TCPserver->hasPendingConnections())
    {
        TCPserver_Client = TCPserver->nextPendingConnection();
        TCPserver->close();

        connect(TCPserver_Client, SIGNAL(readyRead()), this, SLOT(TCP_RecvMSG()));
        connect(TCPserver_Client, SIGNAL(disconnected()), this, SLOT(TCP_Server_Reset()));

        QString curInfo = ui->infoLabel->text();
        QString peerIP = TCPserver_Client->peerAddress().toString();
        ui->infoLabel->setText(curInfo + "TCP Server connected to " + peerIP + "! ");
    }
}

void MainWindow::on_sendMSG_clicked()
{
    int sendAs = ui->sendAsSelect->currentIndex();

    QByteArray msg;
    switch (sendAs) {
        case 1: {
                // Send Custom byte array (assumes no ending)
                QString edited = ui->msgEdit->toPlainText().simplified();
                if (!edited.contains(QRegExp("[ A-F0-9]*", Qt::CaseInsensitive)))
                {
                    ui->infoLabel->setText("ERROR: Invalid Byte Array Input!");
                    return;
                }

                QString val_i;
                QStringList editedSplit = edited.split(QRegExp("\\s"));
                for (int i = 0; i < editedSplit.length(); i++)
                {
                    val_i = editedSplit[i];
                    if ((val_i.length() % 2) != 0) val_i = "0" + val_i;
                    msg += QByteArray::fromHex(val_i.toLatin1());
                }
            }
            break;
        default:
            // Send typed message to generic sender (assumes CRLF ending)
            msg = ui->msgEdit->toPlainText().replace("\n", "\r\n").toLocal8Bit();
            break;
    }

    sendMSG(msg);
}

void MainWindow::TCP_RecvMSG()
{
    // Get current info
    SockInfo info;
    getInfo(sender(), &info);

    // Exit if no display
    if (!info.recv) return;

    // Post received data
    QByteArray data;
    while (!info.tcp->atEnd())
    {
        data = info.tcp->readAll();
        appendMSG(info.name.toUtf8() + ": " + data);
    }
}

void MainWindow::UDP_RecvMSG()
{
    // Get current info
    SockInfo info;
    getInfo(sender(), &info);

    // Exit if no display
    if (!info.recv) return;

    // Post received data
    QNetworkDatagram data;
    while (info.udp->hasPendingDatagrams())
    {
        data = info.udp->receiveDatagram();
        appendMSG(info.name.toUtf8() + ": " + data.data());
    }
}

void MainWindow::on_selectTestSuite_clicked()
{
    // Setup and parse inputs
    QStringList filePaths;
    QFileDialog newTestFS(this);
    newTestFS.setFileMode(QFileDialog::AnyFile);
    newTestFS.setViewMode(QFileDialog::Detail);

    // Verify file
    if (newTestFS.exec())
    {
        filePaths = newTestFS.selectedFiles();
    }

    // Append to test suite list
    ui->testPathEdit->setText(filePaths[0]);
}

void MainWindow::on_runTestSuite_clicked()
{
    // Verify there is a selected test suite
    QString filePath = ui->testPathEdit->toPlainText();
    if (filePath.isEmpty())
    {
        ui->testOut->setText("Must select a suite before running! ");
        return;
    }

    ui->recvData->clear();
    ui->testOut->clear();

    int progress = 0;
    ui->testProgress->setValue(progress);

    QFile testFile(filePath);
    if (testFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QTextStream testStream(&testFile);

        int testLines = 0;
        QString testMSG = testStream.readLine();;
        while (!testStream.atEnd() && (testMSG == "END TESTS"))
        {
            testMSG = testStream.readLine();
            testLines += 1;
        }
        testStream.seek(0);

        int curLine = 0;
        while (curLine < testLines)
        {
            curLine += 1;
            sendMSG(testStream.readLine().toLocal8Bit());

            ui->testProgress->setValue((100*curLine)/testLines);
            qApp->processEvents();
            Sleep(100);
        }
        testStream.readLine();

        QStringList testRECV = ui->recvData->toPlainText().split('\n');
        QStringList testRESP;
        while (!testStream.atEnd())
        {
            testRESP << testStream.readLine();
        }

        int i, tRSP, tRCV, mRSL;
        QString msg;
        tRSP = testRESP.length();
        tRCV = testRECV.length();
        mRSL = qMin(tRSP, tRCV);
        for (i = 0; i < mRSL; i++)
        {
            if (testRESP[i] != testRECV[i])
            {
                msg = "Test " + QString::number(i) + ": received ";
                msg += testRECV[i] + " expected " + testRESP[i];
                ui->testOut->append(msg);
            }
        }

        if (mRSL < tRSP)
        {
            for (i = mRSL; i < tRSP; i++)
            {
                msg = QString::number(i) + " received [NOTHING]";
                msg += " expected " + testRESP[i];
                ui->testOut->append(msg);
            }
        } else if (mRSL < tRCV)
        {
            for (i = mRSL; i < tRCV; i++)
            {
                msg = QString::number(i) + " Received " + testRECV[i];
                msg += " expected [NOTHING]";
                ui->testOut->append(msg);
            }
        }

        if (ui->testOut->toPlainText().isEmpty())
        {
            ui->testOut->setText("Test Suite Passed! ");
        }

        testFile.close();
    } else
    {
        ui->infoLabel->setText("Error opening test file! ");
        return;
    }
}

void MainWindow::appendMSG(QByteArray msg)
{
    char c;
    QString final_msg;
    for (int i = 0; i < msg.length(); i++)
    {
        c = msg[i];
        if ((c < ' ') || ('~' < c)) final_msg.append("\\x" + QString::number(c, 16));
        else final_msg.append(c);
    }

    ui->recvData->appendPlainText(final_msg);
}

void MainWindow::sendMSG(QByteArray msg)
{
    // Send message to checked boxes
    if (ui->cTCP_SCheck->isChecked())
    {
        TCPclient->write(msg);
    }

    if (ui->sTCP_SCheck->isChecked())
    {
        TCPserver_Client->write(msg);
    }

    if (ui->cUDP_SCheck->isChecked())
    {
        // Get current info
        SockInfo info;
        getInfo(ui->cUDP_Label, &info);

        info.udp->writeDatagram(msg, QHostAddress(info.ip), info.port);
    }
}

void MainWindow::getInfo(QObject *sender, SockInfo *info)
{
    // Get infor for associated socket
    if ((sender == TCPclient) || (sender == ui->cTCP_Connect)
            || (sender == ui->cTCP_Disconnect))
    {
        info->name = "TCP Client";
        info->ip = ui->cTCP_ipEdit->toPlainText();
        info->port = (ui->cTCP_PortEdit->toPlainText()).toInt();
        info->send = ui->cTCP_SCheck->isChecked();
        info->recv = ui->cTCP_RCheck->isChecked();
        info->tcp = TCPclient;
        info->udp = Q_NULLPTR;
    }
    else if ((sender == TCPserver) || (sender == TCPserver_Client)
             || (sender == ui->sTCP_Reset))
    {
        info->name = "TCP Server";
        info->ip = "Localhost";
        info->port = (ui->sTCP_PortEdit->toPlainText()).toInt();
        info->send = ui->sTCP_SCheck->isChecked();
        info->recv = ui->sTCP_RCheck->isChecked();
        info->tcp = TCPserver_Client;
        info->udp = Q_NULLPTR;
    }
    else if (sender == ui->cUDP_Label)
    {
        info->name = "UDP Client";
        info->ip = ui->cUDP_ipEdit->toPlainText();
        info->port = (ui->cUDP_PortEdit->toPlainText()).toInt();
        info->send = ui->cUDP_SCheck->isChecked();
        info->recv = false;
        info->tcp = Q_NULLPTR;
        info->udp = UDPclient;
    }
    else if ((sender == UDPserver) || (sender == ui->sUDP_Bind))
    {
        info->name = "UDP Server";
        info->ip = "Localhost";
        info->port = (ui->sUDP_PortEdit->toPlainText()).toInt();
        info->send = false;
        info->recv = ui->sUDP_RCheck->isChecked();
        info->tcp = Q_NULLPTR;
        info->udp = UDPserver;
    }
    else
    {
        info->name = "Unknown";
        info->ip = "Unknown";
        info->port = 0;
        info->send = false;
        info->recv = false;
        info->tcp = Q_NULLPTR;
        info->udp = Q_NULLPTR;
    }
}
