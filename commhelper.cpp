#include "commhelper.h"
#include "globalsettings.h"

#include <QDataStream>
#include <QNetworkSession>
#include <QNetworkConfigurationManager>

CommHelper::CommHelper(QObject *parent)
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
    connect(this, &CommHelper::destroyed, [=]() {
        dataProcessThread->exit(0);
        dataProcessThread->wait(500);
    });
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

    // 终止线程
    mTerminatedThead = true;
    mDataReady = true;
    mCondition.wakeAll();
    dataProcessThread->wait();
}

void CommHelper::initCommand()
{
    dataHeadCmd = QByteArray::fromHex(QString("ab ab ff").toUtf8());;// 数据头
    dataTailCmd = QByteArray::fromHex(QString("cd cd").toUtf8());;// 数据尾

    // 查询继电器电源状态
    {
        askQueryRelayPowerStatusCmd = QByteArray::fromHex(QString("01 03 10 00 00 05 81 09").toUtf8());
    }

    // 继电器电源状态-闭合
    {
        ackRelayPowerStatusOnCmd = QByteArray::fromHex(QString("01 03 0A 00 00 00 01 86 A0 00 0F 42 40 AA 9A").toUtf8());
    }

    // 继电器电源状态-断开
    {
        ackRelayPowerStatusOffCmd = QByteArray::fromHex(QString("01 03 0A 00 01 00 01 86 A0 00 0F 42 40 A7 0A").toUtf8());
    }

    // 继电器电源开
    {
        askRelayPowerOnCmd = QByteArray::fromHex(QString("01 05 00 00 FF 00 8C 3A").toUtf8());
    }

    // 继电器电源关
    {
        askRelayPowerOffCmd = QByteArray::fromHex(QString("01 05 00 00 00 00 CD CA").toUtf8());
    }

    // 硬件触发指令
    {
        ackHardTriggerCmd = QByteArray::fromHex(QString("12 34 00 AA 00 0C 00 00 00 00 AB CD").toUtf8());
    }

    // 测距模块返回数据格式
    {
        ackDistanceDataCmd = QByteArray::fromHex(QString("12 34 00 AB 00 00 00 00 00 00 AB CD").toUtf8());
    }
}

void CommHelper::initSocket(QTcpSocket **s)
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

    //连接成功
    connect(socket, SIGNAL(connected()), this, SLOT(socketConnected()));
    connect(socket, SIGNAL(readyRead()), this, SLOT(socketReadyRead()));

    *s = socket;
}

void CommHelper::errorOccurred(QAbstractSocket::SocketError)
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
}

void CommHelper::socketConnected()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (socket == socketRelay){
        socketConectedStatus |= ssRelay;
        emit relayConnected();
    }
    else if (socket == socketDetector1){
        socketConectedStatus |= ssDetector1;
        emit detectorConnected(1);
    }
    else if (socket == socketDetector2){
        socketConectedStatus |= ssDetector2;
        emit detectorConnected(2);
    }
    else if (socket == socketDetector3){
        socketConectedStatus |= ssDetector3;
        emit detectorConnected(3);
    }
}


void CommHelper::socketReadyRead()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (socket->bytesAvailable() <= 0)
        return;

    QByteArray rawData = socketRelay->readAll();
    if (socket == socketRelay){
        qDebug()<<"Recv HEX: "<<rawData.toHex(' ');
        if (rawData.contains(askRelayPowerOnCmd)){
            // 继电器控制闭合返回
        }
        else if (rawData.contains(askRelayPowerOffCmd)){
            // 继电器控制断开返回
        }
        else if (rawData.contains(ackRelayPowerStatusOnCmd)){
            // 继电器闭合状态返回
        }
        else if (rawData.contains(ackRelayPowerStatusOffCmd)){
            // 继电器断开状态返回
        }
    }
    else if (socket == socketDetector1){
        if (measuring){
            // 已经点击了开始测量
            //单个波形：0xABAB + 0xFFXY+ 波形长度*16bit +0xCDCD
            QMutexLocker locker(&mReceivePoolLocker);
            rawWaveData[0].append(rawData);

            mDataReady = true;
            mCondition.wakeAll();
        }
        else {
            // 指令阶段

            // 测距模块
            if (rawData.contains(ackHardTriggerCmd.left(6))){
                // 硬件触发反馈
            }
            else if (rawData.contains(ackDistanceDataCmd.left(4))){
                QByteArray data = rawData.mid(4, 4);
                QString string = QString::fromUtf8(data);
                string.insert(5, '.');
                float distance = string.toFloat();

                data = rawData.mid(8, 2);
                bool ok = false;
                quint16 quality = data.toShort(&ok, 16);

                qDebug()<<"Distance: "<< distance << ", quality: " << quality;
                emit distanceRespond(distance, quality);
            }

            // 探测器
            else if (rawData.startsWith(QByteArray::fromHex(QString("aa bb").toUtf8())) &&
                     rawData.endsWith(QByteArray::fromHex(QString("cc dd").toUtf8()))){
                //0xAABB + 16bit（温度） + 0xCCDD
                QByteArray data = rawData.mid(2, 2);
                qint16 t = data.toShort();
                float temperature = data.toShort() * 0.0078125;// 换算系数

                qDebug()<<"Temperature: "<<temperature;
                emit temperatureRespond(temperature);
            }
            else if (rawData.startsWith(QByteArray::fromHex(QString("ac ac").toUtf8())) &&
                     rawData.endsWith(QByteArray::fromHex(QString("ef ef").toUtf8()))){
                //版本号
                QByteArray year = rawData.mid(2, 2);
                QByteArray month = rawData.mid(6, 1);
                QByteArray day = rawData.mid(8, 1);
                QByteArray serialNumber = rawData.mid(10, 2);

                QString version = QString::fromUtf8(year) + QString::fromUtf8(month) + QString::fromUtf8(day) ;

                qDebug()<<"AppVersion: "<<version<< ", serialNumber: " << serialNumber;
                emit appVersopmRespond(version, QString::fromUtf8(serialNumber));
            }
        }
    }
    else if (socket == socketDetector2){
        if (measuring){
            // 已经点击了开始测量
            //单个波形：0xABAB + 0xFFXY+ 波形长度*16bit +0xCDCD
            QMutexLocker locker(&mReceivePoolLocker);
            rawWaveData[1].append(rawData);

            mDataReady = true;
            mCondition.wakeAll();
        }
    }
    else if (socket == socketDetector3){
        if (measuring){
            // 已经点击了开始测量
            //单个波形：0xABAB + 0xFFXY+ 波形长度*16bit +0xCDCD
            QMutexLocker locker(&mReceivePoolLocker);
            rawWaveData[2].append(rawData);

            mDataReady = true;
            mCondition.wakeAll();
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
        sockets[i]->waitForConnected(500);
        if (!socketRelay->isOpen() || sockets[i]->state() != QAbstractSocket::ConnectedState){
            ipSettings->finish();
            return false;
        }
    }

    ipSettings->finish();
    return true;
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
    qDebug()<<"Send HEX: "<<askCurrentCmd.toHex(' ');
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
    qDebug()<<"Send HEX: "<<askCurrentCmd.toHex(' ');
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
void CommHelper::sendTransferModeCmd(quint8 mode)
{
    QTcpSocket* sockets[] = {socketDetector1, socketDetector2, socketDetector3};
    for (auto socket : sockets){
        if (nullptr == socket || socket->state() != QAbstractSocket::ConnectedState)
            return;

        askCurrentCmd = QByteArray::fromHex(QString("12 34 00 0f fa 13 00 00 00 05 ab cd").toUtf8());
        askCurrentCmd[9] = mode;
        socket->write(askCurrentCmd);
        qDebug()<<"Send HEX: "<<askCurrentCmd.toHex(' ');
    }
}

/*
 开始测量
*/
void CommHelper::sendMeasureCmd(quint8 mode)
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
void CommHelper::sendQueryAppVersionCmd()
{
    QTcpSocket* sockets[] = {socketDetector1, socketDetector2, socketDetector3};
    for (auto socket : sockets){
        if (nullptr == socket || socket->state() != QAbstractSocket::ConnectedState)
            return;

        askCurrentCmd = QByteArray::fromHex(QString("12 34 00 0f fc 11 00 00 00 00 ab cd").toUtf8());
        socket->write(askCurrentCmd);
        qDebug()<<"Send HEX: "<<askCurrentCmd.toHex(' ');
    }
}

/*
 温度查询
*/
void CommHelper::sendQueryTemperaturCmd(quint8 on/* = 0x01*/)
{
    QTcpSocket* sockets[] = {socketDetector1, socketDetector2, socketDetector3};
    for (auto socket : sockets){
        if (nullptr == socket || socket->state() != QAbstractSocket::ConnectedState)
            return;

        askCurrentCmd = QByteArray::fromHex(QString("12 34 00 0f fc 12 00 00 00 01 ab cd").toUtf8());
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
void CommHelper::sendPowerSwitcherCmd(quint8 on/* = 0x01*/)
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
void CommHelper::sendLaserSwitcherCmd(quint8 on/* = 0x01*/)
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
void CommHelper::sendSingleMeasureCmd(quint8 on/* = 0x01*/)
{
    if (nullptr == socketDetector1 || socketDetector1->state() != QAbstractSocket::ConnectedState)
        return;

    singleMeasure = true;
    askCurrentCmd = QByteArray::fromHex(QString("12 34 00 0f af 12 00 00 00 00 ab cd").toUtf8());
    socketDetector1->write(askCurrentCmd);
    qDebug()<<"Send HEX: "<<askCurrentCmd.toHex(' ');

    QMetaObject::invokeMethod(this, "measureStart", Qt::QueuedConnection);
}

/*
 开始连续测量
*/
void CommHelper::sendContinueMeasureCmd(quint8 on/* = 0x01*/)
{
    if (nullptr == socketDetector1 || socketDetector1->state() != QAbstractSocket::ConnectedState)
        return;

    singleMeasure = false;
    askCurrentCmd = QByteArray::fromHex(QString("12 34 00 0f af 13 00 00 00 00 ab cd").toUtf8());
    askCurrentCmd[9] = on;
    socketDetector1->write(askCurrentCmd);
    qDebug()<<"Send HEX: "<<askCurrentCmd.toHex(' ');

    QMetaObject::invokeMethod(this, "measureStart", Qt::QueuedConnection);
}


void CommHelper::OnDataProcessThread()
{
    qDebug() << "OnDataProcessThread id:" << QThread::currentThreadId();
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
            measuring = false;

            QMetaObject::invokeMethod(this, "measureEnd", Qt::QueuedConnection);
        }
    }
}
