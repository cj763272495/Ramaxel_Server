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
    QTcpServer *tcpServer;//服务器套接字
    QTcpSocket *tcpSocket;//连接进服务器的套接字
    QTcpServer *tcpReg;//注册套接字

    int regestPort = 8887;  //注册端口号

private slots:
    void startTcpServer();
    void closeTcpServer();

    void newConnect();
    void newRegistConnect();

    void readLoginMessages();
    void readRegistMessages();

private:
    void initConnect();
    void sendLoginMessages(QString msg);
    void sendRegMessages(QString msg);

    //解析用户注册信息的json包
    void get_reg_info(QByteArray reg_buf,QString user, QString nick_name, QString pwd, QString tel, QString email);
    int user_regist(QByteArray reg_buf);

    QString GetLocalIPAddress();
    bool verify(QString msg);
    int set_token(const char *user, char *token);
};

#endif // MAINWINDOW_H
