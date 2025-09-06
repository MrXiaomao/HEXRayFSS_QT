#include "devicemanager.h"
#include "globalsettings.h"

#include <QTimer>
#include <QDataStream>
#include <QNetworkSession>
#include <QNetworkConfigurationManager>

DeviceManager::DeviceManager(QObject *parent)
    : QObject{parent}
{
    initCommand();

    initSocket(&socketRelay);
    initSocket(&socketDetector1);
    initSocket(&socketDetector2);
    initSocket(&socketDetector3);

    //更改系统默认超时时长，让网络连接返回能够快点
    QNetworkConfigurationManager manager;
    QNetworkConfiguration config = manager.defaultConfiguration();
    QList<QNetworkConfiguration> cfg_list = manager.allConfigurations();
    if (cfg_list.size() > 0)
    {
        cfg_list.first().setConnectTimeout(1000);
        config = cfg_list.first();
    }
    QSharedPointer<QNetworkSession> spNetworkSession(new QNetworkSession(config));
    socketRelay->setProperty("_q_networksession", QVariant::fromValue(spNetworkSession));
    socketDetector1->setProperty("_q_networksession", QVariant::fromValue(spNetworkSession));
    socketDetector2->setProperty("_q_networksession", QVariant::fromValue(spNetworkSession));
    socketDetector3->setProperty("_q_networksession", QVariant::fromValue(spNetworkSession));

    dataProcessThread = new QLiteThread(this);
    dataProcessThread->setObjectName("dataProcessThread");
    dataProcessThread->setWorkThreadProc([=](){
        OnDataProcessThread();
    });
    dataProcessThread->start();
    connect(this, &DeviceManager::destroyed, [=]() {
        dataProcessThread->exit(0);
        dataProcessThread->wait(500);
    });
}

DeviceManager::~DeviceManager()
{
    auto closeSocket = [&](QTcpSocket* socket){
        if (socket){
            socket->disconnectFromHost();
            socket->close();
            socket->deleteLater();
            socket = nullptr;
        }
    };

    QTcpSocket* sockets[] = {socketRelay, socketDetector1, socketDetector2, socketDetector3};
    for (auto socket : sockets){
        closeSocket(socket);
    }

    // 终止线程
    mTerminatedThead = true;
    mDataReady = true;
    mCondition.wakeAll();
    dataProcessThread->wait();
}

void DeviceManager::initCommand()
{
    dataHeadCmd = QByteArray::fromHex(QString("ab ab ff").toUtf8());;// 数据头
    dataTailCmd = QByteArray::fromHex(QString("cd cd").toUtf8());;// 数据尾

    // 查询继电器电源状态
    askQueryRelayPowerStatusCmd = QByteArray::fromHex(QString("01 03 10 00 00 05 81 09").toUtf8());

    // 继电器电源状态-闭合
    ackRelayPowerStatusOnCmd = QByteArray::fromHex(QString("01 03 0A 00 01 00 01 86 A0 00 0F 42 40 A7 0A").toUtf8());

    // 继电器电源状态-断开
    ackRelayPowerStatusOffCmd = QByteArray::fromHex(QString("01 03 0A 00 00 00 01 86 A0 00 0F 42 40 AA 9A").toUtf8());

    // 继电器电源开
    askRelayPowerOnCmd = QByteArray::fromHex(QString("01 05 00 00 FF 00 8C 3A").toUtf8());

    // 继电器电源关
    askRelayPowerOffCmd = QByteArray::fromHex(QString("01 05 00 00 00 00 CD CA").toUtf8());

    // 硬件触发指令
    ackHardTriggerCmd = QByteArray::fromHex(QString("12 34 00 AA 00 0C 00 00 00 00 AB CD").toUtf8());

    // 测距模块返回数据格式
    ackDistanceDataCmd = QByteArray::fromHex(QString("12 34 00 AB 00 00 00 00 00 00 AB CD").toUtf8());

    // 查询程序版本
    askAppVersionCmd = QByteArray::fromHex(QString("12 34 00 0f fc 11 00 00 00 00 ab cd").toUtf8());

    // 温度查询
    askTemperatureCmd = QByteArray::fromHex(QString("12 34 00 0f fc 12 00 00 00 01 ab cd").toUtf8());
}

void DeviceManager::initSocket(QTcpSocket **s)
{
    QTcpSocket *socket = new QTcpSocket(/*this*/);
    int bufferSize = 4 * 1024 * 1024;
    socket->setSocketOption(QAbstractSocket::SendBufferSizeSocketOption, bufferSize);
    socket->setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption, bufferSize);

    //网络异常
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    connect(socket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(errorOccurred(QAbstractSocket::SocketError)));
#else
    connect(socket, SIGNAL(errorOccurred(QAbstractSocket::SocketError)), this, SLOT(errorOccurred(QAbstractSocket::SocketError)));
#endif

    connect(socket, &QAbstractSocket::stateChanged, this, &DeviceManager::stateChanged);

    //连接成功
    connect(socket, SIGNAL(connected()), this, SLOT(socketConnected()));
    connect(socket, SIGNAL(readyRead()), this, SLOT(socketReadyRead()));

    *s = socket;
}

void DeviceManager::errorOccurred(QAbstractSocket::SocketError)
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (socket == socketRelay){
        socketConectedStatus ^= ssRelay;
        emit relayDisconnected();
    }
    else if (socket == socketDetector1){
        socketConectedStatus ^= ssDetector1;
        emit detectorDisconnected(1);
    }
    else if (socket == socketDetector2){
        socketConectedStatus ^= ssDetector2;
        emit detectorDisconnected(2);
    }
    else if (socket == socketDetector3){
        socketConectedStatus ^= ssDetector3;
        emit detectorDisconnected(3);
    }

    relayIsConnected = socketConectedStatus & 0x01;
    detectorsIsConnected = socketConectedStatus & 0x0E;
}

void DeviceManager::stateChanged(QAbstractSocket::SocketState state)
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (state == QAbstractSocket::SocketState::UnconnectedState){
        if (socket == socketRelay){
            socketConectedStatus ^= ssRelay;
        }
        else if (socket == socketDetector1){
            socketConectedStatus ^= ssDetector1;
        }
        else if (socket == socketDetector2){
            socketConectedStatus ^= ssDetector2;
        }
        else if (socket == socketDetector3){
            socketConectedStatus ^= ssDetector3;
        }

        relayIsConnected = socketConectedStatus & 0x01;
        detectorsIsConnected = socketConectedStatus & 0x0E;

        if (socket == socketRelay){
            emit relayDisconnected();
        }
        else if (socket == socketDetector1){
            emit detectorDisconnected(1);
        }
        else if (socket == socketDetector2){
            emit detectorDisconnected(2);
        }
        else if (socket == socketDetector3){
            emit detectorDisconnected(3);
        }
    }
}


void DeviceManager::socketConnected()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (socket == socketRelay){
        socketConectedStatus |= ssRelay;
        QTimer::singleShot(0, this, [=]{
            emit relayConnected();
        });
    }
    else if (socket == socketDetector1){
        socketConectedStatus |= ssDetector1;
        QTimer::singleShot(0, this, [=]{
            emit detectorConnected(1);

            static bool firstConnected = true;
            if (firstConnected){
                //查询程序版本号
                this->sendQueryAppVersionCmd(1);
                firstConnected = false;
            }
        });
    }
    else if (socket == socketDetector2){
        socketConectedStatus |= ssDetector2;
        QTimer::singleShot(0, this, [=]{
            static bool firstConnected = true;
            if (firstConnected){
                //查询程序版本号
                this->sendQueryAppVersionCmd(2);
                firstConnected = false;
            }

            emit detectorConnected(2);
        });
    }
    else if (socket == socketDetector3){
        socketConectedStatus |= ssDetector3;
        QTimer::singleShot(0, this, [=]{
            static bool firstConnected = true;
            if (firstConnected){
                //查询程序版本号
                this->sendQueryAppVersionCmd(3);
                firstConnected = false;
            }

            emit detectorConnected(3);
        });
    }

    relayIsConnected = socketConectedStatus & 0x01;
    detectorsIsConnected = socketConectedStatus & 0x0E;

    // if (socket == socketRelay){
    //     emit relayConnected();
    // }
    // else if (socket == socketDetector1){
    //     emit detectorConnected(1);
    // }
    // else if (socket == socketDetector2){
    //     emit detectorConnected(2);
    // }
    // else if (socket == socketDetector3){
    //     emit detectorConnected(3);
    // }
}

#include <QtEndian>
void DeviceManager::socketReadyRead()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (socket->bytesAvailable() <= 0)
        return;

    QByteArray rawData = socket->readAll();
    if (socket == socketRelay){
        qDebug()<<"Recv HEX: "<<rawData.toHex(' ');

        if (rawData.contains(askRelayPowerOnCmd)){
            // 继电器控制闭合返回
            emit relayPowerOn();

            // 继电器开，再连接探测器
            QTimer::singleShot(0, this, [=]{
                this->connectDetectors();
            });
        }
        else if (rawData.contains(askRelayPowerOffCmd)){
            // 继电器控制断开返回
            emit relayPowerOff();
        }
        else if (rawData.contains(ackRelayPowerStatusOnCmd)){
            // 继电器闭合状态返回
            emit relayPowerOn();

            // 继电器开，再连接探测器
            QTimer::singleShot(0, this, [=]{
                this->connectDetectors();
            });
        }
        else if (rawData.contains(ackRelayPowerStatusOffCmd)){
            // 继电器断开状态返回
            emit relayPowerOff();
        }
    }
    else if (socket == socketDetector1 || socket == socketDetector2 || socket == socketDetector3){
        // 指令阶段，指令长度为12Bytes
        qDebug()<<"Recv HEX: "<<rawData.toHex(' ');
        static const int BASE_CMD_LENGTH = 12;

        while (true){
            if (waveMeasuring){
                // 已经点击了开始测量
                //单个波形：0xABAB + 0xFFXY+ 波形长度*16bit +0xCDCD
                QMutexLocker locker(&mReceivePoolLocker);
                if (socket == socketDetector1)
                    rawWaveData[0].append(rawData);
                else if (socket == socketDetector2)
                    rawWaveData[1].append(rawData);
                else if (socket == socketDetector3)
                    rawWaveData[2].append(rawData);

                mDataReady = true;
                mCondition.wakeAll();
                return;
            }

            // 特殊指令集判断
            // 0xAABB + 16bit（温度） + 0xCCDD
            if (rawData.startsWith(QByteArray::fromHex(QString("AA BB").toUtf8())) &&
                rawData.mid(4, 2) == QByteArray::fromHex(QString("CC DD").toUtf8())){
                //0xAABB + 16bit（温度） + 0xCCDD
                QByteArray data = rawData.mid(2, 2);
                qint16 t = qFromBigEndian<qint16>(data.constData());
                float temperature = t * 0.0078125;// 换算系数

                if (socket == socketDetector1){
                    qDebug()<<"Temperature 1: "<<temperature;
                    emit temperatureRespond(1, temperature);
                }
                if (socket == socketDetector2){
                    qDebug()<<"Temperature 2: "<<temperature;
                    emit temperatureRespond(2, temperature);
                }
                if (socket == socketDetector3){
                    qDebug()<<"Temperature 3: "<<temperature;
                    emit temperatureRespond(3, temperature);
                }

                rawData.remove(0, 6);
                continue;
            }

            // 0xACAC + 48bit（版本号） + 0xEFEF
            if (rawData.startsWith(QByteArray::fromHex(QString("AC AC").toUtf8())) &&
                rawData.mid(8, 2) == QByteArray::fromHex(QString("EF EF").toUtf8())){
                // 0xACAC + 48bit（版本号） + 0xEFEF
                QByteArray year = rawData.mid(2, 2);
                QByteArray month = rawData.mid(4, 1);
                QByteArray day = rawData.mid(5, 1);
                QByteArray serialNumber = rawData.mid(6, 2);

                bool ok = false;
                QString version = year.toHex().toUpper() +
                                  month.toHex().toUpper() +
                                  day.toHex().toUpper();

                if (socket == socketDetector1){
                    qDebug()<<"AppVersion 1: "<<version<< ", serialNumber: " << serialNumber.toHex().toUpper();
                    emit appVersionRespond(1, version, QString(serialNumber.toHex().toUpper()));
                }
                else if (socket == socketDetector2){
                    qDebug()<<"AppVersion 2: "<<version<< ", serialNumber: " << serialNumber.toHex().toUpper();
                    emit appVersionRespond(2, version, QString(serialNumber.toHex().toUpper()));
                }
                else if (socket == socketDetector3){
                    qDebug()<<"AppVersion 3: "<<version<< ", serialNumber: " << serialNumber.toHex().toUpper();
                    emit appVersionRespond(3, version, QString(serialNumber.toHex().toUpper()));
                }

                rawData.remove(0, 10);
                continue;
            }

            if (rawData.size() == 0)
                break;

            // 指令起始字节是12 34 00 0F/AB，尾字节是AB CD
            while (rawData.size() >= BASE_CMD_LENGTH){
                if (!rawData.startsWith(QByteArray::fromHex(QString("12 34 00").toUtf8()))){
                    rawData.remove(0, 1);
                }

                if (rawData.size() >= BASE_CMD_LENGTH){
                    if (rawData.mid(10, 2) != QByteArray::fromHex(QString("AB CD").toUtf8())){
                        rawData.remove(0, 3);
                    }
                    else{
                        break;
                    }
                }
            }

            if (rawData.size() < BASE_CMD_LENGTH){
                qDebug()<<"data length isn't full!";
                return;
            }

            if (distanceMeasuring){
                // 指令返回-距离测量
                if ((rawData.startsWith(QByteArray::fromHex(QString("12 34 00 0F AF 12").toUtf8())) ||  //单次
                     rawData.startsWith(QByteArray::fromHex(QString("12 34 00 0F AF 13").toUtf8()))) &&  //连续
                    rawData.mid(10, 2) != QByteArray::fromHex(QString("AB CD").toUtf8())){
                    rawData.remove(0, BASE_CMD_LENGTH);
                    continue;
                }

                // 数据返回-测量距离
                if (rawData.startsWith(QByteArray::fromHex(QString("12 34 00 AB").toUtf8())) &&
                    rawData.mid(10, 2) != QByteArray::fromHex(QString("AB CD").toUtf8())){
                    QByteArray data = rawData.mid(4, 4);
                    QString string = QString::fromUtf8(data);
                    string.insert(5, '.');
                    float distance = string.toFloat();

                    data = rawData.mid(8, 2);
                    bool ok = false;
                    quint16 quality = data.toShort(&ok, 16);

                    qDebug()<<"Distance: "<< distance << ", quality: " << quality;
                    emit distanceRespond(distance, quality);

                    rawData.remove(0, BASE_CMD_LENGTH);
                    if (singleMeasure){
                        distanceMeasuring = false;
                    }

                    continue;
                }
                else{
                    qDebug()<<"(1) Unknown cmd: "<<rawData.toHex(' ');
                    rawData.remove(0, BASE_CMD_LENGTH);
                    continue;
                }
            }
            else {
                // 硬件触发反馈
                if (rawData.startsWith(QByteArray::fromHex(QString("12 34 00 AA 00 0C 00 00 00 00 AB CD").toUtf8()))){
                    waveMeasuring = true;
                    rawData.remove(0, BASE_CMD_LENGTH);
                    continue;
                }

                // 指令返回-探测器-传输模式-程序版本号
                if (rawData.startsWith(QByteArray::fromHex(QString("12 34 00 0F FA 13 00 00 00 00 01 AB CD").toUtf8()))){
                    rawData.remove(0, BASE_CMD_LENGTH);

                    // 再发送查询指令
                    askCurrentCmd = askAppVersionCmd;
                    socket->write(askCurrentCmd);
                    qDebug()<<"Send HEX: "<<askCurrentCmd.toHex(' ');
                    continue;
                }

                // 指令返回-探测器-传输模式-波形
                if (rawData.startsWith(QByteArray::fromHex(QString("12 34 00 0F FA 13 00 00 00 03 AB CD").toUtf8()))){
                    rawData.remove(0, BASE_CMD_LENGTH);
                    continue;
                }

                // 指令返回-探测器-传输模式-温度
                if (rawData.startsWith(QByteArray::fromHex(QString("12 34 00 0F FA 13 00 00 00 05 AB CD").toUtf8()))){
                    rawData.remove(0, BASE_CMD_LENGTH);
                    continue;
                }

                // 指令返回-探测器-传输模式-激光测距
                if (rawData.startsWith(QByteArray::fromHex(QString("12 34 00 0F FA 13 00 00 00 09 AB CD").toUtf8()))){
                    rawData.remove(0, BASE_CMD_LENGTH);
                    continue;
                }

                // 指令返回-探测器-程控增益
                if (rawData.startsWith(QByteArray::fromHex(QString("12 34 00 0F FB 11").toUtf8())) &&
                    rawData.mid(10, 2) != QByteArray::fromHex(QString("AB CD").toUtf8())){
                    rawData.remove(0, BASE_CMD_LENGTH);
                    continue;
                }

                // 指令返回-探测器-程序版本查询
                if (rawData.startsWith(QByteArray::fromHex(QString("12 34 00 0F FC 11 00 00 00 00 AB CD").toUtf8()))){
                    rawData.remove(0, BASE_CMD_LENGTH);
                    continue;
                }

                // 指令返回-探测器-温度查询-开始
                if (rawData.startsWith(QByteArray::fromHex(QString("12 34 00 0F FC 12 00 00 00 01 AB CD").toUtf8()))){
                    rawData.remove(0, BASE_CMD_LENGTH);
                    continue;
                }

                // 指令返回-探测器-温度查询-停止
                if (rawData.startsWith(QByteArray::fromHex(QString("12 34 00 0F FC 12 00 00 00 00 AB CD").toUtf8()))){
                    rawData.remove(0, BASE_CMD_LENGTH);
                    continue;
                }

                // 指令返回-探测器-触发阈值
                if ((rawData.startsWith(QByteArray::fromHex(QString("12 34 00 0F FE 11").toUtf8())) ||
                     rawData.startsWith(QByteArray::fromHex(QString("12 34 00 0F FE 12").toUtf8()))) &&
                    rawData.mid(10, 2) != QByteArray::fromHex(QString("AB CD").toUtf8())){
                    rawData.remove(0, BASE_CMD_LENGTH);
                    continue;
                }

                // 指令返回-探测器-波形触发模式
                if (rawData.startsWith(QByteArray::fromHex(QString("12 34 00 0F FE 14").toUtf8())) &&
                    rawData.mid(10, 2) != QByteArray::fromHex(QString("AB CD").toUtf8())){
                    rawData.remove(0, BASE_CMD_LENGTH);
                    continue;
                }

                // 指令返回-探测器-波形长度
                if (rawData.startsWith(QByteArray::fromHex(QString("12 34 00 0F FE 15").toUtf8())) &&
                    rawData.mid(10, 2) != QByteArray::fromHex(QString("AB CD").toUtf8())){
                    rawData.remove(0, BASE_CMD_LENGTH);
                    continue;
                }

                // 指令返回-探测器-触发模式-停止
                if (rawData.startsWith(QByteArray::fromHex(QString("12 34 00 0F FF 10 11 11 00 00 AB CD").toUtf8()))){
                    rawData.remove(0, BASE_CMD_LENGTH);
                    continue;
                }

                // 指令返回-探测器-触发模式-软件触发
                if (rawData.startsWith(QByteArray::fromHex(QString("12 34 00 0F FF 10 11 11 00 01 AB CD").toUtf8()))){
                    rawData.remove(0, BASE_CMD_LENGTH);
                    continue;
                }

                // 指令返回-探测器-触发模式-硬件触发
                if (rawData.startsWith(QByteArray::fromHex(QString("12 34 00 0F FF 10 11 11 00 02 AB CD").toUtf8()))){
                    rawData.remove(0, BASE_CMD_LENGTH);
                    continue;
                }

                // 指令返回-测距模块-电源闭合
                if (rawData.startsWith(QByteArray::fromHex(QString("12 34 00 0F AF 10 00 00 00 01 AB CD").toUtf8()))){
                    rawData.remove(0, BASE_CMD_LENGTH);
                    continue;
                }

                // 指令返回-测距模块-电源断开
                if (rawData.startsWith(QByteArray::fromHex(QString("12 34 00 0F AF 10 00 00 00 00 AB CD").toUtf8()))){
                    rawData.remove(0, BASE_CMD_LENGTH);
                    continue;
                }

                // 指令返回-测距模块-激光打开
                if (rawData.startsWith(QByteArray::fromHex(QString("12 34 00 0F AF 11 00 00 00 01 AB CD").toUtf8()))){
                    rawData.remove(0, BASE_CMD_LENGTH);
                    continue;
                }

                // 指令返回-测距模块-激光关闭
                if (rawData.startsWith(QByteArray::fromHex(QString("12 34 00 0F AF 11 00 00 00 00 AB CD").toUtf8()))){
                    rawData.remove(0, BASE_CMD_LENGTH);
                    continue;
                }

                // 指令返回-测距模块-开始单次测量
                if (rawData.startsWith(QByteArray::fromHex(QString("12 34 00 0F AF 12 00 00 00 00 AB CD").toUtf8()))){
                    rawData.remove(0, BASE_CMD_LENGTH);
                    continue;
                }

                // 指令返回-测距模块-连续测量-开始
                if (rawData.startsWith(QByteArray::fromHex(QString("12 34 00 0F AF 13 00 00 00 01 AB CD").toUtf8()))){
                    rawData.remove(0, BASE_CMD_LENGTH);
                    continue;
                }

                // 指令返回-测距模块-连续测量-停止
                if (rawData.startsWith(QByteArray::fromHex(QString("12 34 00 0F AF 13 00 00 00 00 AB CD").toUtf8()))){
                    rawData.remove(0, BASE_CMD_LENGTH);
                    continue;
                }

                if (rawData.size() != 0)
                {
                    qDebug()<<"(2) Unknown cmd: "<<rawData.toHex(' ');
                    rawData.remove(0, BASE_CMD_LENGTH);
                    continue;
                }
                else{
                    break;
                }
            }
        }
    }
}

bool DeviceManager::connectRelay()
{
    JsonSettings* ipSettings = GlobalSettings::instance()->mIpSettings;
    ipSettings->prepare();
    ipSettings->beginGroup("Relay");
    QString ip = ipSettings->value("ip").toString();
    qint32 port = ipSettings->value("port").toInt();
    ipSettings->endGroup();
    ipSettings->finish();

    //断开网络连接
    if (socketRelay->isOpen() && socketRelay->state() == QAbstractSocket::ConnectedState)
        socketRelay->abort();

    socketRelay->connectToHost(ip, port);
    socketRelay->waitForConnected(500);
    return socketRelay->isOpen() && socketRelay->state() == QAbstractSocket::ConnectedState;
}

void DeviceManager::disconnectRelay()
{
    socketRelay->abort();
}

bool DeviceManager::connectDetectors()
{
    JsonSettings* ipSettings = GlobalSettings::instance()->mIpSettings;
    ipSettings->prepare();

    QTcpSocket* sockets[] = {socketRelay, socketDetector1, socketDetector2, socketDetector3};
    for (int i=1; i<=3; ++i){
        ipSettings->beginGroup(QString("Detector%1").arg(i));
        QString ip = ipSettings->value("ip").toString();
        qint32 port = ipSettings->value("port").toInt();
        ipSettings->endGroup();

        if (sockets[i]->isOpen() && sockets[i]->state() == QAbstractSocket::ConnectedState)
            sockets[i]->abort();

        sockets[i]->connectToHost(ip, port);
        // sockets[i]->waitForConnected(500);
        // if (!socketRelay->isOpen() || sockets[i]->state() != QAbstractSocket::ConnectedState){
        //     ipSettings->finish();
        //     return false;
        // }
    }

    ipSettings->finish();
    return true;
}

void DeviceManager::disconnectDetectors()
{
    QTcpSocket* sockets[] = {socketRelay, socketDetector1, socketDetector2, socketDetector3};
    for (int i=1; i<=3; ++i){
        sockets[i]->abort();
    }
}

/*
 控制单路通断
*/
void DeviceManager::sendRelayPowerSwitcherCmd(quint8 on/* = 0x01*/)
{
    if (nullptr == socketRelay || socketRelay->state() != QAbstractSocket::ConnectedState)
        return;

    if (on == 0x00)
        askCurrentCmd = askRelayPowerOffCmd;
    else
        askCurrentCmd = askRelayPowerOnCmd;
    socketRelay->write(askCurrentCmd);
    qDebug()<<"Send HEX: "<<askCurrentCmd.toHex(' ');
}

/*
 查询状态
*/
void DeviceManager::sendQueryRelayStatusCmd()
{
    if (nullptr == socketRelay || socketRelay->state() != QAbstractSocket::ConnectedState)
        return;

    askCurrentCmd = askQueryRelayPowerStatusCmd;
    socketRelay->write(askCurrentCmd);
    qDebug()<<"Send HEX: "<<askCurrentCmd.toHex(' ');
}

/*
 触发阈值
*/
void DeviceManager::sendTriggerTholdCmd()
{
    JsonSettings* fpgaSettings = GlobalSettings::instance()->mFpgaSettings;
    fpgaSettings->prepare();

    fpgaSettings->beginGroup();
    QString triggerThold = fpgaSettings->value("TriggerThold").toString();
    QList<QString> triggerTholds = triggerThold.split(',', Qt::SkipEmptyParts);
    while (triggerTholds.size() < 4)
        triggerTholds.push_back("200");
    fpgaSettings->endGroup();
    fpgaSettings->finish();

    QList<quint16> value = {triggerTholds[0].toUShort(), triggerTholds[1].toUShort(), triggerTholds[2].toUShort(), triggerTholds[3].toUShort()};
    QTcpSocket* sockets[] = {socketDetector1, socketDetector2, socketDetector3};
    for (auto socket : sockets){
        if (nullptr == socket || socket->state() != QAbstractSocket::ConnectedState)
            return;

        // CH2 CH1
        askCurrentCmd = QByteArray::fromHex(QString("12 34 00 0f fe 11 00 00 00 00 ab cd").toUtf8());
        askCurrentCmd[6] = value[0] >> 8;
        askCurrentCmd[7] = value[0] & 0x000FF;
        askCurrentCmd[8] = value[1] >> 8;
        askCurrentCmd[9] = value[1] & 0x000FF;
        socket->write(askCurrentCmd);
        qDebug()<<"Send HEX: "<<askCurrentCmd.toHex(' ');

        // CH4 CH3
        askCurrentCmd = QByteArray::fromHex(QString("12 34 00 0f fe 12 00 00 00 00 ab cd").toUtf8());
        askCurrentCmd[6] = value[2] >> 8;
        askCurrentCmd[7] = value[2] & 0x000FF;
        askCurrentCmd[8] = value[3] >> 8;
        askCurrentCmd[9] = value[3] & 0x000FF;
        socket->write(askCurrentCmd);
        qDebug()<<"Send HEX: "<<askCurrentCmd.toHex(' ');
    }
}

/*
 波形触发模式
*/
void DeviceManager::sendWaveTriggerModeCmd()
{
    JsonSettings* fpgaSettings = GlobalSettings::instance()->mFpgaSettings;
    fpgaSettings->prepare();

    fpgaSettings->beginGroup();
    quint8 triggerMode = fpgaSettings->value("TriggerMode").toUInt();
    fpgaSettings->endGroup();
    fpgaSettings->finish();

    QTcpSocket* sockets[] = {socketDetector1, socketDetector2, socketDetector3};
    for (auto socket : sockets){
        if (nullptr == socket || socket->state() != QAbstractSocket::ConnectedState)
            return;

        askCurrentCmd = QByteArray::fromHex(QString("12 34 00 0f fe 14 00 00 00 00 ab cd").toUtf8());
        askCurrentCmd[9] = triggerMode;
        socket->write(askCurrentCmd);
        qDebug()<<"Send HEX: "<<askCurrentCmd.toHex(' ');
    }
}

/*
 波形长度
*/
void DeviceManager::sendWaveLengthCmd()
{
    JsonSettings* fpgaSettings = GlobalSettings::instance()->mFpgaSettings;
    fpgaSettings->prepare();

    fpgaSettings->beginGroup();
    quint8 waveLength = fpgaSettings->value("WaveLength").toUInt();
    fpgaSettings->endGroup();
    fpgaSettings->finish();

    QTcpSocket* sockets[] = {socketDetector1, socketDetector2, socketDetector3};
    for (auto socket : sockets){
        if (nullptr == socket || socket->state() != QAbstractSocket::ConnectedState)
            return;

        askCurrentCmd = QByteArray::fromHex(QString("12 34 00 0f fe 15 00 00 00 00 ab cd").toUtf8());
        askCurrentCmd[9] = waveLength;
        socket->write(askCurrentCmd);
        qDebug()<<"Send HEX: "<<askCurrentCmd.toHex(' ');
    }
}

/*
 程控增益
*/
void DeviceManager::sendGainCmd()
{
    JsonSettings* fpgaSettings = GlobalSettings::instance()->mFpgaSettings;
    fpgaSettings->prepare();

    fpgaSettings->beginGroup();
    QString gain = fpgaSettings->value("Gain").toString();
    QList<QString> gains = gain.split(',', Qt::SkipEmptyParts);
    while (gains.size() < 4)
        gains.push_back("5");
    fpgaSettings->endGroup();
    fpgaSettings->finish();

    QList<quint16> value = {gains[0].toUShort(), gains[1].toUShort(), gains[2].toUShort(), gains[3].toUShort()};
    QTcpSocket* sockets[] = {socketDetector1, socketDetector2, socketDetector3};
    for (auto socket : sockets){
        if (nullptr == socket || socket->state() != QAbstractSocket::ConnectedState)
            return;

        // CH2 CH1
        askCurrentCmd = QByteArray::fromHex(QString("12 34 00 0f fb 11 00 00 00 00 ab cd").toUtf8());
        askCurrentCmd[6] = value[3];
        askCurrentCmd[7] = value[2];
        askCurrentCmd[8] = value[1];
        askCurrentCmd[9] = value[0];
        socket->write(askCurrentCmd);
        qDebug()<<"Send HEX: "<<askCurrentCmd.toHex(' ');
    }
}

/*
 传输模式
*/
void DeviceManager::sendTransferModeCmd(quint8 index, quint8 mode)
{
    QTcpSocket* sockets[] = {socketRelay, socketDetector1, socketDetector2, socketDetector3};
    if (nullptr == sockets[index] || sockets[index]->state() != QAbstractSocket::ConnectedState)
        return;

    askCurrentCmd = QByteArray::fromHex(QString("12 34 00 0f fa 13 00 00 00 05 ab cd").toUtf8());
    askCurrentCmd[9] = mode;
    sockets[index]->write(askCurrentCmd);
    qDebug()<<"Send HEX: "<<askCurrentCmd.toHex(' ');
}

/*
 开始测量
*/
void DeviceManager::sendMeasureCmd(quint8 mode)
{
    QTcpSocket* sockets[] = {socketDetector1, socketDetector2, socketDetector3};
    for (auto socket : sockets){
        if (nullptr == socket || socket->state() != QAbstractSocket::ConnectedState)
            return;

        askCurrentCmd = QByteArray::fromHex(QString("12 34 00 0f ff 10 11 11 00 00 ab cd").toUtf8());
        askCurrentCmd[9] = mode;
        socket->write(askCurrentCmd);
        qDebug()<<"Send HEX: "<<askCurrentCmd.toHex(' ');
    }
}

/*
 程序版本查询
*/
void DeviceManager::sendQueryAppVersionCmd(quint8 index/* = 0x01*/)
{
    QTcpSocket* sockets[] = {socketRelay, socketDetector1, socketDetector2, socketDetector3};
    if (nullptr == sockets[index] || sockets[index]->state() != QAbstractSocket::ConnectedState)
        return;

    // 先设置传输模式
    this->sendTransferModeCmd(index, tmAppVersion);
}

/*
 温度查询
*/
void DeviceManager::sendQueryTemperaturCmd(quint8 on/* = 0x01*/)
{
    if (waveMeasuring || distanceMeasuring)
        return;

    QTcpSocket* sockets[] = {socketDetector1, socketDetector2, socketDetector3};

    // 先设置传输模式
    for (int i=1; i<=3; ++i){
        this->sendTransferModeCmd(i, tmTemperature);
    }

    // 再发送查询请求指令
    for (auto socket : sockets){
        if (nullptr == socket || socket->state() != QAbstractSocket::ConnectedState)
            return;

        askCurrentCmd = askTemperatureCmd;
        askCurrentCmd[9] = on;
        socket->write(askCurrentCmd);
        qDebug()<<"Send HEX: "<<askCurrentCmd.toHex(' ');
    }
}

/*
 测距模块指令
*/

/*
 模块电源打开/关闭
*/
void DeviceManager::sendPowerSwitcherCmd(quint8 on/* = 0x01*/)
{
    if (nullptr == socketDetector1 || socketDetector1->state() != QAbstractSocket::ConnectedState)
        return;

    askCurrentCmd = QByteArray::fromHex(QString("12 34 00 0f af 10 00 00 00 00 ab cd").toUtf8());
    askCurrentCmd[9] = on;
    socketDetector1->write(askCurrentCmd);
    qDebug()<<"Send HEX: "<<askCurrentCmd.toHex(' ');
}

/*
 激光打开/关闭
*/
void DeviceManager::sendLaserSwitcherCmd(quint8 on/* = 0x01*/)
{
    if (nullptr == socketDetector1 || socketDetector1->state() != QAbstractSocket::ConnectedState)
        return;

    askCurrentCmd = QByteArray::fromHex(QString("12 34 00 0f af 11 00 00 00 00 ab cd").toUtf8());
    askCurrentCmd[9] = on;
    socketDetector1->write(askCurrentCmd);
    qDebug()<<"Send HEX: "<<askCurrentCmd.toHex(' ');
}

/*
 开始单次测量
*/
void DeviceManager::sendSingleMeasureCmd()
{
    if (nullptr == socketDetector1 || socketDetector1->state() != QAbstractSocket::ConnectedState)
        return;

    chWaveDataValidTag = 0x00;
    singleMeasure = true;
    askCurrentCmd = QByteArray::fromHex(QString("12 34 00 0f af 12 00 00 00 00 ab cd").toUtf8());
    socketDetector1->write(askCurrentCmd);
    qDebug()<<"Send HEX: "<<askCurrentCmd.toHex(' ');

    QMetaObject::invokeMethod(this, "measureStart", Qt::QueuedConnection);
}

/*
 开始连续测量
*/
void DeviceManager::sendContinueMeasureCmd(quint8 on/* = 0x01*/)
{
    if (nullptr == socketDetector1 || socketDetector1->state() != QAbstractSocket::ConnectedState)
        return;

    chWaveDataValidTag = 0x00;
    singleMeasure = false;
    askCurrentCmd = QByteArray::fromHex(QString("12 34 00 0f af 13 00 00 00 00 ab cd").toUtf8());
    askCurrentCmd[9] = on;
    socketDetector1->write(askCurrentCmd);
    qDebug()<<"Send HEX: "<<askCurrentCmd.toHex(' ');

    QMetaObject::invokeMethod(this, "measureStart", Qt::QueuedConnection);
}


void DeviceManager::OnDataProcessThread()
{
    qDebug() << "OnDataProcessThread id:" << QThread::currentThreadId();
    chWaveDataValidTag = 0x00;
    while (!mTerminatedThead)
    {
        //单个波形：0xABAB + 0xFFXY+ 波形长度*16bit +0xCDCD
        quint32 baseWaveTotalSize = (waveLength + 3) * 2;

        {
            QMutexLocker locker(&mReceivePoolLocker);
            if (rawWaveData[0].size() < baseWaveTotalSize ||
                rawWaveData[1].size() < baseWaveTotalSize ||
                rawWaveData[2].size() < baseWaveTotalSize){
                while (!mDataReady){
                    mCondition.wait(&mReceivePoolLocker);
                }
            }

            if (rawWaveData[0].size() >= baseWaveTotalSize ||
                rawWaveData[1].size() >= baseWaveTotalSize ||
                rawWaveData[2].size() >= baseWaveTotalSize){
                rawWaveAllData.append(rawWaveData[0]);
                rawWaveAllData.append(rawWaveData[1]);
                rawWaveAllData.append(rawWaveData[2]);

                rawWaveData[0].clear();
                rawWaveData[1].clear();
                rawWaveData[2].clear();
                mDataReady = false;
            }
        }

        if (rawWaveAllData.size() == 0)
            continue;

        QMap<quint8, QVector<quint16>> realCurve;// 实测曲线
        QVector<QPair<float, float>> calResult;// 反解能谱
        while (rawWaveAllData.size() >= baseWaveTotalSize * 3){
            if (rawWaveAllData.startsWith(dataHeadCmd)){
                // 指令包

                //继续检查包尾
                QByteArray chunk = rawWaveAllData.left(baseWaveTotalSize);
                if (chunk.endsWith(dataTailCmd)){
                    //单个波形：0xABAB + 0xFFXY+ 波形长度*16bit +0xCDCD
                    //X:数采板序号 Y:通道号
                    quint8 ch = chunk[3] & 0x0F;
                    QVector<quint16> data;

                    for (int i = 0; i < waveLength * 2; i += 2) {
                        quint16 value = static_cast<quint8>(chunk[i + 4]) << 8 | static_cast<quint8>(chunk[i + 5]);
                        data.append(value);
                    }

                    chWaveDataValidTag |= (0x01 << (ch - 1));
                    realCurve[ch] = data;
                }
                else {
                    // 包尾不正确，继续寻找包头
                    rawWaveAllData.remove(0, 3);
                }
            }
            else{
                // 包头不正确，继续寻找包头
                rawWaveAllData.remove(0, 1);
            }
        }

        if (chWaveDataValidTag == 0x0FFF){
            QVector<quint16> sortRawWaveAllData;
            sortRawWaveAllData.resize((waveLength + 3) * 2 * 12);
            for (int i=1; i<=12; ++i){
                sortRawWaveAllData.append(realCurve[i]);
            }

            QString strTime = QDateTime::currentDateTime().toString("yyyy-MM-dd_HHmmss");
            QString filePath = QString("%1").arg(shotDir + "/" + strTime + ".dat");
            QFile *pfSaveNet = new QFile(filePath);
            if (pfSaveNet->open(QIODevice::WriteOnly)) {
                qDebug().noquote() << tr("创建网口数据缓存文件成功，文件名：%1").arg(filePath);
            } else {
                qDebug().noquote() << tr("创建网口数据缓存文件失败，文件名：%1").arg(filePath);
            }

            if (nullptr != pfSaveNet){
                pfSaveNet->write((const char *)sortRawWaveAllData.constData(), sortRawWaveAllData.size() * sizeof(quint16));
                pfSaveNet->flush();
                pfSaveNet->close();
            }

            // 实测曲线
            QMetaObject::invokeMethod(this, [=]() {
                emit showRealCurve(realCurve);
            }, Qt::QueuedConnection);

            // 反解能谱
            QMetaObject::invokeMethod(this, [=]() {
                emit showEnerygySpectrumCurve(calResult);
            }, Qt::QueuedConnection);

            if (singleMeasure){
                // 波形数据处理完成，重设测量状态
                waveMeasuring = false;

                QMetaObject::invokeMethod(this, "measureEnd", Qt::QueuedConnection);
            }
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////
/*
 连接网络
*/
void DeviceManager::connectNet()
{
    this->connectRelay();
}
/*
 断开网络
*/
void DeviceManager::disconnectNet()
{
    this->disconnectDetectors();
    this->disconnectRelay();
}

/*
 打开电源
*/
void DeviceManager::openPower()
{
    if (relayIsConnected)
        this->sendRelayPowerSwitcherCmd();
}
/*
 断开电源
*/
void DeviceManager::closePower()
{
    if (relayIsConnected){
        //先关闭探测器
        this->disconnectDetectors();

        //再发送关闭指令
        this->sendRelayPowerSwitcherCmd(0x00);
    }
}

/*
 开始测量
*/
void DeviceManager::startMeasure(quint8 mode)
{
    if (!detectorsIsConnected)
        return;

    this->sendTriggerTholdCmd();
    this->sendWaveTriggerModeCmd();
    this->sendWaveTriggerModeCmd();
    this->sendWaveLengthCmd();
    this->sendGainCmd();
    //this->sendTransferModeCmd(tmWaveMode);
    this->sendMeasureCmd(mode);
}

/*
 停止测量
*/
void DeviceManager::stopMeasure()
{
    if (!detectorsIsConnected)
        return;

    this->sendMeasureCmd(TriggerMode::tmStop);
}

/*
 温度查询
*/
void DeviceManager::queryTemperature()
{
    if (!detectorsIsConnected)
        return;

    this->sendQueryTemperaturCmd();
}

/*
 继电器状态查询
*/
void DeviceManager::queryRelayStatus()
{
    if (relayIsConnected)
        this->sendQueryRelayStatusCmd();
}

/*
 开始测距
*/
void DeviceManager::startMeasureDistance(bool isContinue/* = false*/)
{
    if (!detectorsIsConnected)
        return;

    distanceMeasuring = true;
    if (isContinue)
        this->sendContinueMeasureCmd(0x01);
    else
        this->sendSingleMeasureCmd();
}

/*
 停止测距
*/
void DeviceManager::stopMeasureDistance()
{
    if (!detectorsIsConnected)
        return;

    this->sendContinueMeasureCmd(0x00);
}
