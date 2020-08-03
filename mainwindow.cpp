#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QNetworkInterface>
#include <QtSql>
#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlTableModel>
#include <QSqlError>
#include <QDebug>
#include <hiredis/hiredis.h>
#include <hiredis/read.h>
#include <hiredis/sds.h>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    this->tcpServer = NULL;
    this->tcpSocket = NULL;
    this->ui->label_IP->setText(this->GetLocalIPAddress());
    this->initConnect();
}

MainWindow::~MainWindow()
{
    delete ui;
    if(this->tcpSocket != NULL) {
        this->tcpSocket->close();
        delete this->tcpSocket;
    }
    if(this->tcpServer != NULL) {
        this->tcpServer->close();
        delete this->tcpServer;
    }
}

/**
 * @brief MainWindow::initConnect
 * @caption 初始化QObject::connect()函数
 */
void MainWindow::initConnect()
{
    connect(this->ui->pushButton_Start,SIGNAL(clicked()),this,SLOT(startTcpServer()));
    connect(this->ui->pushButton_Close,SIGNAL(clicked()),this,SLOT(closeTcpServer()));
}


/**
 * @brief MainWindow::startTcpServer 开启服务器
 */
void MainWindow::startTcpServer()
{
    this->tcpServer = new QTcpServer(this);
    int port = this->ui->port_edit->text().toUInt();
    this->tcpServer->listen(QHostAddress::Any,port);
    connect(this->tcpServer,SIGNAL(newConnection()),this,SLOT(newConnect()));
    qDebug()<<"服务器开启成功，"<<"端口号："<<port;
    this->ui->label_SS->setText("Running...");

}

/**
 * @brief MainWindow::closeTcpServer 关闭服务器
 */
void MainWindow::closeTcpServer()
{
    if(this->tcpSocket != NULL) {
        this->tcpSocket->close();
    }
    if(this->tcpServer != NULL) {
        this->tcpServer->close();
    }

    this->ui->label_SS->setText("Stop.");
    qDebug()<<"服务器成功关闭";
}

/**
 * @brief MainWindow::newConnect 新的Socket连接
 */
void MainWindow::newConnect()
{
    this->tcpSocket = this->tcpServer->nextPendingConnection();
    connect(this->tcpSocket,SIGNAL(readyRead()),this,SLOT(readLoginMessages()));
}

/**
 * @brief MainWindow::verify 执行登录验证的操作
 * @param msg 欲验证的加密报文
 * @return 验证成功，返回true；否则，返回false。
 */
bool MainWindow::verify(QString msg)
{
    //QStringList drivers = QSqlDatabase::drivers();

    //数据库连接
    QSqlDatabase db = QSqlDatabase::addDatabase("QMYSQL"); //database driver
    db.setHostName("127.0.0.1");  //database ip address
    db.setUserName("root");   //database username
    db.setPassword(" ");   //database password
    db.setDatabaseName("cloud_disk");     //database table name
    if(!db.open()){
        qDebug()<<"fail to connect mysql:"<< db.lastError().text();
        QMessageBox::information(this, "信息提示", "mysql数据库连接失败.",
                                 QMessageBox::Yes | QMessageBox::No,
                                 QMessageBox::Yes);
        db.close();
        exit(EXIT_FAILURE);

    }
    qDebug() << "mysql数据库连接成功";

    QSqlQuery query;//获取数据库信息
    query.exec("select * from user");

    char token[128] = {0};


    //服务器端的数据同样采取加密操作，比较的是加密后的密文是否相同
    QCryptographicHash md5(QCryptographicHash::Md5);
    //遍历数据库信息，查询判断是否有该用户
    while(query.next()) {
        QString tmp = query.value(1).toString() + query.value(3).toString();
        qDebug()<<"name:"<< query.value(1).toString()<<"pwd"<<query.value(3).toString();
        if(tmp == msg) {
            //产生token码,QString转char*类型
            std::string name = query.value(1).toString().toStdString();
            const char* user = name.c_str();
            //qDebug() << "user: "<< user;
            set_token(user,token);
            return true;
        }
    }
    db.close();
    return false;
}

int MainWindow::set_token(const char* user, char *token)
{

    redisContext* pRedisContext=(redisContext*)redisConnect("127.0.0.1",6379);
    if(pRedisContext==NULL)
        {
            printf("Error:连接redis失败\n");
            return false;
        }
    if(pRedisContext->err!=0)
        {
            printf("Error:%s\n",pRedisContext->errstr);
            redisFree(pRedisContext);
        }

    int rand_num[4]={0};
    srand((int)time(nullptr));
    for(int i =0; i< 4;i++){
        rand_num[i] = rand()%1000;
    }
    char tmp[1024] = {0};

    sprintf(tmp, "%s%d%d%d%d", user, rand_num[0], rand_num[1], rand_num[2], rand_num[3]);
    qDebug() << "sprintf tmp::"<<tmp;

    char str[100] = { 0 };
    for (int i = 0; i < 16; i++)
    {
         sprintf(str, "%02x", rand_num[i]);
         strcat(token, str);
        }
    int ret =0;
    // redis保存此字符串，用户名：token,
    redisReply * reply = (redisReply*)redisCommand(pRedisContext,"SET %s %s",user,token);
    qDebug()<< "user and token: " << user<<"    " << token;
    if (NULL != reply)
        {
            freeReplyObject(reply);
        }
    return ret;

}

/**
 * @brief MainWindow::readMessages 读取发送来的数据
 */
void MainWindow::readLoginMessages()
{
    QByteArray qba = this->tcpSocket->readAll();
    QString msg = QVariant(qba).toString();
    //this->setWindowTitle(msg);
    if(this->verify(msg)) {
        msg = "true";
    } else {
        msg = "false";
    }
    this->sendLoginMessages(msg);
}

/**
 * @brief MainWindow::sendMessages 发送回复消息，以说明登录验证是否成功。
 *      如果回复"true"，表示验证成功；否则，回复为"false",表示验证失败。
 */
void MainWindow::sendLoginMessages(QString msg)
{
    this->tcpSocket->write(msg.toStdString().c_str(),
                           strlen(msg.toStdString().c_str()));
}

/**
 * @brief QT获取本机IP地址
 */
QString MainWindow::GetLocalIPAddress()
{
    QString vAddress;
#ifdef _WIN32
    QHostInfo vHostInfo = QHostInfo::fromName(QHostInfo::localHostName());
    QList<QHostAddress> vAddressList = vHostInfo.addresses();
#else
    QList<QHostAddress> vAddressList = QNetworkInterface::allAddresses();
#endif
    for(int i = 0; i < vAddressList.size(); i++) {
        if(!vAddressList.at(i).isNull() &&
                vAddressList.at(i) != QHostAddress::LocalHost &&
                vAddressList.at(i).protocol() ==  QAbstractSocket::IPv4Protocol)
        {
            vAddress = vAddressList.at(i).toString();
            break;
        }
    }

    return vAddress;
}





