#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostInfo>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QVariant>
#include <QMessageBox>
#include <QCryptographicHash>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;
    QTcpServer *tcpServer;
    QTcpSocket *tcpSocket;

private slots:
    void startTcpServer();
    void closeTcpServer();
    void newConnect();
    void readLoginMessages();

private:
    void initConnect();
    void sendLoginMessages(QString msg);
    QString GetLocalIPAddress();
    bool verify(QString msg);
    int set_token(const char *user, char *token);
};

#endif // MAINWINDOW_H
