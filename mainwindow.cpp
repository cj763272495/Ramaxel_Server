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

    this->tcpServer = nullptr;
    this->tcpSocket = nullptr;
    this->tcpReg = nullptr;

    this->ui->label_IP->setText(this->GetLocalIPAddress());
    this->initConnect();
}

MainWindow::~MainWindow()
{
    delete ui;
    if(this->tcpSocket != nullptr) {
        this->tcpSocket->close();
        delete this->tcpSocket;
    }
    if(this->tcpServer != nullptr) {
        this->tcpServer->close();
        delete this->tcpServer;
    }
    if(this->tcpReg != nullptr) {
        this->tcpReg->close();
        delete this->tcpReg;
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
    this->tcpReg = new QTcpServer(this);
    int port = this->ui->port_edit->text().toUInt();

    //登录端口,可在控制台输入端口号
    if(this->tcpServer->listen(QHostAddress::Any,port))
        connect(this->tcpServer,SIGNAL(newConnection()),this,SLOT(newConnect()));
    qDebug()<<"登录服务开启成功，"<<"端口号："<<port;
    this->ui->label_SS->setText("Running...");

    //注册端口，目前固定端口号
    if(this->tcpReg->listen(QHostAddress::Any,regestPort))
        connect(this->tcpReg,SIGNAL(newConnection()),this,SLOT(newRegistConnect()));
    qDebug()<<"注册服务开启成功，"<<"端口号："<<regestPort;
}

/**
 * @brief MainWindow::closeTcpServer 关闭服务器
 */
void MainWindow::closeTcpServer()
{
    if(this->tcpSocket != nullptr) {
        this->tcpSocket->close();
    }
    if(this->tcpServer != nullptr) {
        this->tcpServer->close();
    }
    if(this->tcpServer != nullptr) {
        this->tcpReg->close();
    }

    this->ui->label_SS->setText("Stop.");
    qDebug()<<"服务器成功关闭";
}

/**
 * @brief MainWindow::newConnect 新的Socket连接
 */
void MainWindow::newConnect()
{
    this->tcpSocket = this->tcpServer->nextPendingConnection();//获取连接进来的socket
    connect(this->tcpSocket,SIGNAL(readyRead()),this,SLOT(readLoginMessages()));
}

void MainWindow::newRegistConnect()
{
    this->tcpSocket = this->tcpReg->nextPendingConnection();//获取连接进来的socket
    connect(this->tcpSocket,SIGNAL(readyRead()),this,SLOT(readRegistMessages()));
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
    db.setDatabaseName("cloud_disk");     //database name
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
            std::string name = query.value(1).toString().toStdString();
            const char* user = name.c_str();
            //qDebug() << "user: "<< user;
            //产生token码,QString转char*类型
            set_token(user,token);
            db.close();
            return true;
        }
    }
    db.close();
    return false;
}

int MainWindow::set_token(const char* user, char *token)
{
    //链接redis
    redisContext* pRedisContext=(redisContext*)redisConnect("127.0.0.1",6379);
    if(pRedisContext==nullptr)
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
    // redis保存此字符串，用户名，token,
    redisReply * reply = (redisReply*)redisCommand(pRedisContext,"SET %s %s",user,token);
    qDebug()<< "user and token: " << user<<"    " << token;
    if (nullptr != reply)
        {
            freeReplyObject(reply);
        }
    return 0;
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


void MainWindow::readRegistMessages()
{
    QTextCodec *codec = QTextCodec::codecForName("utf8");
    QByteArray allData = this->tcpSocket->readAll();
    //使用utf8编码，这样才可以显示中文
    QString all = codec->toUnicode(allData);
    int ret = user_regist(allData);
    QString str = "001";
    if (ret == 0) //注册成功
    {
        qDebug()<<"注册成功";
        sendLoginMessages(str);
    }
}

/**
 * @brief MainWindow::sendMessages 发送回复消息，以说明登录验证是否成功。
 *      如果回复"true"，表示验证成功；否则，回复为"false",表示验证失败。
 */
void MainWindow::sendLoginMessages(QString msg)
{
    if(msg == nullptr)
        return;
    this->tcpSocket->write(msg.toStdString().c_str(),
                           strlen(msg.toStdString().c_str()));
}


int MainWindow::get_reg_info(QByteArray reg_buf,QString user, QString nick_name, QString pwd, QString tel, QString email)
{
        /*json数据如下
        {
            userName:xxxx,
            nickName:xxx,
            firstPwd:xxx,
            phone:xxx,
            email:xxx
        }
        */
        //解析json包
    QJsonParseError jsonError;
    QJsonDocument doucment = QJsonDocument::fromJson(reg_buf, &jsonError);  // 转化为 JSON 文档
    if (!doucment.isNull() && (jsonError.error == QJsonParseError::NoError)) {  // 解析未发生错误
        if (doucment.isObject()) { // JSON 文档为对象
            QJsonObject object = doucment.object();  // 转化为对象
            if (object.contains("userName")) {  // 包含指定的 key
                QJsonValue value = object.value("userName");  // 获取指定 key 对应的 value
                if (value.isString()) {  // 判断 value 是否为字符串
                    user = value.toString();  // 将 value 转化为字符串
                }
            }
            if (object.contains("nickName")) {
                QJsonValue value = object.value("nickName");
                if (value.isString()) {
                    nick_name = value.toString();
                }
            }
            if (object.contains("firstPwd")) {
                QJsonValue value = object.value("firstPwd");
                if (value.isString()) {
                    pwd = value.toString();
                }
            }
            if (object.contains("phone")) {
                QJsonValue value = object.value("phone");
                if (value.isString()) {
                    tel = value.toString();
                }
            }
            if (object.contains("email")) {
                QJsonValue value = object.value("email");
                if (value.isString()) {
                    email = value.toString();
                }
            }
        }
        return 0;
    }
    return -1;
    qDebug()<<"注册信息包："<<user<<" "<<nick_name<<" "<<pwd<<" "<<tel<<" "<<email;
}

int MainWindow::user_regist(QByteArray reg_buf)
{
    //获取注册信息
    QString user;
    QString nick_name;
    QString pwd;
    QString phone;
    QString email;
    return get_reg_info(reg_buf,user,nick_name,pwd,phone,email);
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





