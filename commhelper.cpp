#include "commhelper.h"
#include "globalsettings.h"

#include <QTimer>
#include <QDataStream>
#include <QNetworkSession>
#include <QNetworkConfigurationManager>

CommHelper::CommHelper(QObject *parent)
    : QObject{parent}
{
    /*初始化指令*/
    initCommand();

    /*初始化网络*/
    initSocket(&socketRelay);
    initSocket(&socketDetector1);
    initSocket(&socketDetector2);
    initSocket(&socketDetector3);

    /*创建数据处理器*/
    initDataProcessor(&detector1DataProcessor, socketDetector1, 1);
    initDataProcessor(&detector2DataProcessor, socketDetector2, 2);
    initDataProcessor(&detector3DataProcessor, socketDetector3, 3);

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
}

CommHelper::~CommHelper()
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
}

void CommHelper::initCommand()
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

void CommHelper::initSocket(QTcpSocket **s)
{
    QTcpSocket *socket = new QTcpSocket(/*this*/);
    *s = socket;

    int bufferSize = 4 * 1024 * 1024;
    socket->setSocketOption(QAbstractSocket::SendBufferSizeSocketOption, bufferSize);
    socket->setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption, bufferSize);

    //网络异常
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    connect(socket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(errorOccurred(QAbstractSocket::SocketError)));
#else
    connect(socket, SIGNAL(errorOccurred(QAbstractSocket::SocketError)), this, SLOT(errorOccurred(QAbstractSocket::SocketError)));
#endif

    connect(socket, &QAbstractSocket::stateChanged, this, &CommHelper::stateChanged);

    //连接成功
    connect(socket, SIGNAL(connected()), this, SLOT(socketConnected()));

    // 关联继电器槽函数，其它探测器交给数据处理器(DataProcessor)取处理吧
    if (socket == socketRelay){
        connect(socket, SIGNAL(readyRead()), this, SLOT(socketReadyRead()));
    }
}

void CommHelper::initDataProcessor(DataProcessor** processor, QTcpSocket *socket, quint8 index)
{
    DataProcessor* detectorDataProcessor = new DataProcessor(index, socket, this);
    *processor = detectorDataProcessor;

    connect(detectorDataProcessor, &DataProcessor::relayConnected, this, &CommHelper::relayConnected);
    connect(detectorDataProcessor, &DataProcessor::relayDisconnected, this, &CommHelper::relayDisconnected);
    connect(detectorDataProcessor, &DataProcessor::relayPowerOn, this, &CommHelper::relayPowerOn);
    connect(detectorDataProcessor, &DataProcessor::relayPowerOff, this, &CommHelper::relayPowerOff);

    connect(detectorDataProcessor, &DataProcessor::detectorConnected, this, &CommHelper::detectorConnected);
    connect(detectorDataProcessor, &DataProcessor::detectorDisconnected, this, &CommHelper::detectorDisconnected);
    connect(detectorDataProcessor, &DataProcessor::temperatureRespond, this, &CommHelper::temperatureRespond);
    connect(detectorDataProcessor, &DataProcessor::appVersionRespond, this, &CommHelper::appVersionRespond);
    connect(detectorDataProcessor, &DataProcessor::distanceRespond, this, &CommHelper::distanceRespond);

    connect(detectorDataProcessor, &DataProcessor::measureStart, this, [=](quint8 index){
        waveMeasuring = true;
    });
    connect(detectorDataProcessor, &DataProcessor::measureStart, this, &CommHelper::measureStart);

    connect(detectorDataProcessor, &DataProcessor::measureEnd, this, [=](quint8 index){
        waveMeasuring = false;

        // 测量结束，可以开始温度查询了
        this->queryTemperature(index);
    });
    connect(detectorDataProcessor, &DataProcessor::measureEnd, this, &CommHelper::measureEnd);

    connect(detectorDataProcessor, &DataProcessor::showRealCurve, this, [=](const QMap<quint8, QVector<quint16>>& data){
        // 将map1的内容添加到map2
        for (auto it = data.constBegin(); it != data.constEnd(); ++it) {
            waveAllData[it.key()] = it.value();
        }

        if (waveAllData.size() >= 11){
            //11个通道都收集完毕，可以进行反能谱计算了
            QVector<QPair<float, float>> result;



            //显示反能谱曲线图
            showEnerygySpectrumCurve(result);
        }
    });
    connect(detectorDataProcessor, &DataProcessor::showRealCurve, this, &CommHelper::showRealCurve);
    connect(detectorDataProcessor, &DataProcessor::showEnerygySpectrumCurve, this, &CommHelper::showEnerygySpectrumCurve);
}

void CommHelper::errorOccurred(QAbstractSocket::SocketError)
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (socket == socketRelay){
        if (socketConectedStatus & ssRelay){
            emit relayDisconnected();
        }
        socketConectedStatus ^= ssRelay;
    }
    else if (socket == socketDetector1){
        if (socketConectedStatus & ssDetector1){
            emit detectorDisconnected(1);
        }
        socketConectedStatus ^= ssDetector1;        
    }
    else if (socket == socketDetector2){
        if (socketConectedStatus & ssDetector2){
            emit detectorDisconnected(2);
        }
        socketConectedStatus ^= ssDetector2;
    }
    else if (socket == socketDetector3){
        if (socketConectedStatus & ssDetector3){
            emit detectorDisconnected(3);
        }
        socketConectedStatus ^= ssDetector3;        
    }

    relayIsConnected = socketConectedStatus & 0x01;
    detectorsIsConnected = socketConectedStatus & 0x0E;
}

void CommHelper::stateChanged(QAbstractSocket::SocketState state)
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (state == QAbstractSocket::SocketState::UnconnectedState){
        if (socket == socketRelay){
            if (socketConectedStatus & ssRelay){
                emit relayDisconnected();
            }

            socketConectedStatus ^= ssRelay;
        }
        else if (socket == socketDetector1){
            if (socketConectedStatus & ssDetector1){
                emit detectorDisconnected(1);
            }
            socketConectedStatus ^= ssDetector1;
        }
        else if (socket == socketDetector2){
            if (socketConectedStatus & ssDetector2){
                emit detectorDisconnected(2);
            }
            socketConectedStatus ^= ssDetector2;
        }
        else if (socket == socketDetector3){
            if (socketConectedStatus & ssDetector3){
                emit detectorDisconnected(3);
            }
            socketConectedStatus ^= ssDetector3;
        }

        relayIsConnected = socketConectedStatus & 0x01;
        detectorsIsConnected = socketConectedStatus & 0x0E;    
    }
}


void CommHelper::socketConnected()
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
                //firstConnected = false;
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
                //firstConnected = false;
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
                //firstConnected = false;
            }

            emit detectorConnected(3);
        });
    }

    relayIsConnected = socketConectedStatus & 0x01;
    detectorsIsConnected = socketConectedStatus & 0x0E;
}

#include <QtEndian>
void CommHelper::socketReadyRead()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (socket->bytesAvailable() <= 0)
        return;

    QByteArray rawData = socket->readAll();
    if (socket == socketRelay){
        qDebug()<< "[" << 0 << "] "<<"Recv HEX: "<<rawData.toHex(' ');

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
}

bool CommHelper::connectRelay()
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

void CommHelper::disconnectRelay()
{
    socketRelay->abort();
}

bool CommHelper::connectDetectors()
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

void CommHelper::disconnectDetectors()
{
    QTcpSocket* sockets[] = {socketRelay, socketDetector1, socketDetector2, socketDetector3};
    for (int i=1; i<=3; ++i){
        sockets[i]->abort();
    }
}

/*
 控制单路通断
*/
void CommHelper::sendRelayPowerSwitcherCmd(quint8 on/* = 0x01*/)
{
    if (nullptr == socketRelay || socketRelay->state() != QAbstractSocket::ConnectedState)
        return;

    if (on == 0x00)
        askCurrentCmd = askRelayPowerOffCmd;
    else
        askCurrentCmd = askRelayPowerOnCmd;
    socketRelay->write(askCurrentCmd);
    qDebug()<< "[" << 0 << "] "<<"Send HEX: "<<askCurrentCmd.toHex(' ');
}

/*
 查询状态
*/
void CommHelper::sendQueryRelayStatusCmd()
{
    if (nullptr == socketRelay || socketRelay->state() != QAbstractSocket::ConnectedState)
        return;

    askCurrentCmd = askQueryRelayPowerStatusCmd;
    socketRelay->write(askCurrentCmd);
    qDebug()<< "[" << 0 << "] "<<"Send HEX: "<<askCurrentCmd.toHex(' ');
}

/*
 触发阈值
*/
void CommHelper::sendTriggerTholdCmd()
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
            continue;

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
void CommHelper::sendWaveTriggerModeCmd()
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
            continue;

        askCurrentCmd = QByteArray::fromHex(QString("12 34 00 0f fe 14 00 00 00 00 ab cd").toUtf8());
        askCurrentCmd[9] = triggerMode;
        socket->write(askCurrentCmd);
        qDebug()<<"Send HEX: "<<askCurrentCmd.toHex(' ');
    }
}

/*
 波形长度
*/
void CommHelper::sendWaveLengthCmd()
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
            continue;

        askCurrentCmd = QByteArray::fromHex(QString("12 34 00 0f fe 15 00 00 00 00 ab cd").toUtf8());
        askCurrentCmd[9] = waveLength;
        socket->write(askCurrentCmd);
        qDebug()<<"Send HEX: "<<askCurrentCmd.toHex(' ');
    }
}

/*
 程控增益
*/
void CommHelper::sendGainCmd()
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
            continue;

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
void CommHelper::sendTransferModeCmd(quint8 index, quint8 mode)
{
    QTcpSocket* sockets[] = {socketRelay, socketDetector1, socketDetector2, socketDetector3};
    if (nullptr == sockets[index] || sockets[index]->state() != QAbstractSocket::ConnectedState)
        return;

    askCurrentCmd = QByteArray::fromHex(QString("12 34 00 0f fa 13 00 00 00 00 ab cd").toUtf8());
    askCurrentCmd[9] = mode;
    sockets[index]->write(askCurrentCmd);
    qDebug()<< "[" << index << "] "<<"Send HEX: "<<askCurrentCmd.toHex(' ');
}

/*
 开始测量
*/
void CommHelper::sendMeasureCmd(quint8 mode)
{
    QTcpSocket* sockets[] = {socketDetector1, socketDetector2, socketDetector3};
    for (auto socket : sockets){
        if (nullptr == socket || socket->state() != QAbstractSocket::ConnectedState)
            continue;

        askCurrentCmd = QByteArray::fromHex(QString("12 34 00 0f ff 10 11 11 00 00 ab cd").toUtf8());
        askCurrentCmd[9] = mode;
        socket->write(askCurrentCmd);
        qDebug()<<"Send HEX: "<<askCurrentCmd.toHex(' ');
    }
}

/*
 程序版本查询
*/
void CommHelper::sendQueryAppVersionCmd(quint8 index/* = 0x01*/)
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
void CommHelper::sendQueryTemperaturCmd(quint8 index, quint8 on/* = 0x01*/)
{
    if (waveMeasuring || distanceMeasuring)
        return;

    QTcpSocket* sockets[] = {socketDetector1, socketDetector2, socketDetector3};

    // 先设置传输模式
    this->sendTransferModeCmd(index, tmTemperature);
}

/*
 测距模块指令
*/

/*
 模块电源打开/关闭
*/
void CommHelper::sendPowerSwitcherCmd(quint8 on/* = 0x01*/)
{
    if (nullptr == socketDetector1 || socketDetector1->state() != QAbstractSocket::ConnectedState)
        return;

    askCurrentCmd = QByteArray::fromHex(QString("12 34 00 0f af 10 00 00 00 00 ab cd").toUtf8());
    askCurrentCmd[9] = on;
    socketDetector1->write(askCurrentCmd);
    qDebug()<< "[" << 1 << "] "<<"Send HEX: "<<askCurrentCmd.toHex(' ');
}

/*
 激光打开/关闭
*/
void CommHelper::sendLaserSwitcherCmd(quint8 on/* = 0x01*/)
{
    if (nullptr == socketDetector1 || socketDetector1->state() != QAbstractSocket::ConnectedState)
        return;

    askCurrentCmd = QByteArray::fromHex(QString("12 34 00 0f af 11 00 00 00 00 ab cd").toUtf8());
    askCurrentCmd[9] = on;
    socketDetector1->write(askCurrentCmd);
    qDebug()<< "[" << 1 << "] "<<"Send HEX: "<<askCurrentCmd.toHex(' ');
}

/*
 开始单次测量
*/
void CommHelper::sendSingleMeasureCmd()
{
    if (nullptr == socketDetector1 || socketDetector1->state() != QAbstractSocket::ConnectedState)
        return;

    chWaveDataValidTag = 0x00;
    singleMeasure = true;
    askCurrentCmd = QByteArray::fromHex(QString("12 34 00 0f af 12 00 00 00 00 ab cd").toUtf8());
    socketDetector1->write(askCurrentCmd);
    qDebug()<< "[" << 1 << "] "<<"Send HEX: "<<askCurrentCmd.toHex(' ');

    QMetaObject::invokeMethod(this, "measureStart", Qt::QueuedConnection);
}

/*
 开始连续测量
*/
void CommHelper::sendContinueMeasureCmd(quint8 on/* = 0x01*/)
{
    if (nullptr == socketDetector1 || socketDetector1->state() != QAbstractSocket::ConnectedState)
        return;

    chWaveDataValidTag = 0x00;
    singleMeasure = false;
    askCurrentCmd = QByteArray::fromHex(QString("12 34 00 0f af 13 00 00 00 00 ab cd").toUtf8());
    askCurrentCmd[9] = on;
    socketDetector1->write(askCurrentCmd);
    qDebug()<< "[" << 1 << "] "<<"Send HEX: "<<askCurrentCmd.toHex(' ');

    QMetaObject::invokeMethod(this, "measureStart", Qt::QueuedConnection);
}

/////////////////////////////////////////////////////////////////////////////////
/*
 连接网络
*/
void CommHelper::connectNet()
{
    this->connectRelay();
}
/*
 断开网络
*/
void CommHelper::disconnectNet()
{
    this->disconnectDetectors();
    this->disconnectRelay();
}

/*
 打开电源
*/
void CommHelper::openPower()
{
    if (relayIsConnected)
        this->sendRelayPowerSwitcherCmd();
}
/*
 断开电源
*/
void CommHelper::closePower()
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
void CommHelper::startMeasure(quint8 mode)
{
    DataProcessor* detectorsDataProcessor[] = {detector1DataProcessor, detector2DataProcessor, detector3DataProcessor};
    for (auto detectorDataProcessor : detectorsDataProcessor){
        detectorDataProcessor->startMeasureWave(mode);
    }

    for (int i=1; i<=3; ++i){
        this->sendTransferModeCmd(i, tmWaveMode);
    }
}

/*
 停止测量
*/
void CommHelper::stopMeasure()
{
    this->sendMeasureCmd(TriggerMode::tmStop);
}

/*
 温度查询
*/
void CommHelper::queryTemperature(quint8 index, quint8 on)
{
    this->sendQueryTemperaturCmd(index, on);
}

/*
 继电器状态查询
*/
void CommHelper::queryRelayStatus()
{
    if (relayIsConnected)
        this->sendQueryRelayStatusCmd();
}

/*
 开始测距
*/
void CommHelper::startMeasureDistance(bool isContinue/* = false*/)
{
    distanceMeasuring = true;
    if (isContinue)
        this->sendContinueMeasureCmd(0x01);
    else
        this->sendSingleMeasureCmd();
}

/*
 停止测距
*/
void CommHelper::stopMeasureDistance()
{
    this->sendContinueMeasureCmd(0x00);
}
