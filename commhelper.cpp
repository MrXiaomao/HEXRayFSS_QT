#include "commhelper.h"
#include "globalsettings.h"

#include <QTimer>
#include <QDataStream>
#include <QNetworkSession>
#include <QNetworkConfigurationManager>

CommHelper::CommHelper(QObject *parent)
    : QObject{parent}
#ifdef ENABLE_MATLAB
    , m_mwT(1, 1, mxDOUBLE_CLASS)
    , m_mwSeq(500, 1, mxDOUBLE_CLASS)
    , m_mwResponce_matrix(11, 500, mxDOUBLE_CLASS)
#endif // ENABLE_MATLAB
{
#ifdef ENABLE_MATLAB
    //mbuild --setup
    //mex --setup
    //mcc -W cpplib:UnfolddingAlgorithm_Gravel  -T link:lib UnfolddingAlgorithm_Gravel.m -C
    if (gMatlabInited){
        try{
            m_mwT = 1E-8;

            double* seq_cpp = new double[500];
            loadSeq(seq_cpp);
            m_mwSeq.SetData(seq_cpp, 500);
            delete[] seq_cpp;
            seq_cpp = nullptr;

            double* responce_matrix_cpp = new double[11 * 500];
            loadResponceMatrix(responce_matrix_cpp);
            m_mwResponce_matrix.SetData(responce_matrix_cpp, 11 * 500);
            delete[] responce_matrix_cpp;
            responce_matrix_cpp = nullptr;
        }
        catch(mwException e){
            gMatlabInited = false;
            qDebug().noquote() << "*** matlab程序DLL初始化失败. Error msg:" << e.what();
        }
    }
#endif //ENABLE_MATLAB

    /*初始化指令*/
    initCommand();

    /*初始化网络*/
    initSocket(&mSocketRelay);
    initSocket(&mSocketDetector1);
    initSocket(&mSocketDetector2);
    initSocket(&mSocketDetector3);

    /*创建数据处理器*/
    initDataProcessor(&mDetector1DataProcessor, mSocketDetector1, 1);
    initDataProcessor(&mDetector2DataProcessor, mSocketDetector2, 2);
    initDataProcessor(&mDetector3DataProcessor, mSocketDetector3, 3);

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
    mSocketRelay->setProperty("_q_networksession", QVariant::fromValue(spNetworkSession));
    mSocketDetector1->setProperty("_q_networksession", QVariant::fromValue(spNetworkSession));
    mSocketDetector2->setProperty("_q_networksession", QVariant::fromValue(spNetworkSession));
    mSocketDetector3->setProperty("_q_networksession", QVariant::fromValue(spNetworkSession));
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

    QTcpSocket* sockets[] = {mSocketRelay, mSocketDetector1, mSocketDetector2, mSocketDetector3};
    for (auto socket : sockets){
        closeSocket(socket);
    }
}

#ifdef ENABLE_MATLAB
bool CommHelper::loadSeq(double* seq)
{
    QFile file("./seq_energy.csv");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    int count = 0;
    QTextStream stream(&file);
    while (!stream.atEnd())
    {
        QString line = stream.readLine();
        seq[count++] = line.toDouble();
    }

    file.close();
    return true;
}

bool CommHelper::reloadResponceMatrix()
{
    bool ret = false;
    double* responce_matrix_cpp = new double[11 * 500];
    if (loadResponceMatrix(responce_matrix_cpp))
    {
        m_mwResponce_matrix.SetData(responce_matrix_cpp, 11 * 500);
        ret = true;
    }
    delete[] responce_matrix_cpp;
    responce_matrix_cpp = nullptr;
    return ret;
}

bool CommHelper::loadResponceMatrix(double* responceMatrix)
{
    QFile file("./responce_matrix.csv");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    int count = 0;
    QTextStream stream(&file);
    while (!stream.atEnd())
    {
        QString line = stream.readLine();
        QStringList row = line.split(',', Qt::SkipEmptyParts);
        for (int i = 0; i < row.size(); i++)
        {
            responceMatrix[count++] = row.at(i).toDouble();
        }
    }

    file.close();
    return true;

    char* old_locale = _strdup(setlocale(LC_CTYPE, NULL));
    setlocale(LC_CTYPE, ("chs"));
}

bool CommHelper::loadRom(double* rom)
{
    QFile file("./rom_wave_corr.csv");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    int count = 0;
    QTextStream stream(&file);
    while (!stream.atEnd())
    {
        QString line = stream.readLine();
        QStringList row = line.split(',', Qt::SkipEmptyParts);
        for (int i = 0; i < row.size(); i++)
        {
            rom[count++] = row.at(i).toDouble();
        }
    }

    file.close();
    return true;
}

bool CommHelper::loadData(double* data)
{
    QFile file("./data.csv");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    int count = 0;
    QTextStream stream(&file);
    while (!stream.atEnd())
    {
        QString line = stream.readLine();
        data[count++] = line.toDouble();
    }

    file.close();
    return true;
}
#endif //ENABLE_MATLAB

void CommHelper::initCommand()
{
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
    if (socket == mSocketRelay){
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
        mWaveMeasuring = true;
    });
    connect(detectorDataProcessor, &DataProcessor::measureStart, this, &CommHelper::measureStart);

    connect(detectorDataProcessor, &DataProcessor::measureEnd, this, [=](quint8 index){
        mWaveMeasuring = false;

        // 测量结束，可以开始温度查询了
        this->queryTemperature(index);
    });
    connect(detectorDataProcessor, &DataProcessor::measureEnd, this, &CommHelper::measureEnd);

    connect(detectorDataProcessor, &DataProcessor::showRealCurve, this, [=](const QMap<quint8, QVector<quint16>>& data){
        // 将map1的内容添加到map2
        for (auto iterator = data.constBegin(); iterator != data.constEnd(); ++iterator) {
            // 这里只保留前11个通道数据
            if (iterator.key() >=1 && iterator.key() <= 11){
                mWaveAllData[iterator.key()] = iterator.value();
            }
        }

        /*
         计算反解能谱
        */
        calEnerygySpectrumCurve();
    });
    connect(this, &CommHelper::showHistoryCurve, this, [=](const QMap<quint8, QVector<quint16>>& data){
        // 将map1的内容添加到map2
        for (auto iterator = data.constBegin(); iterator != data.constEnd(); ++iterator) {
            // 这里只保留前11个通道数据
            if (iterator.key() >=1 && iterator.key() <= 11){
                mWaveAllData[iterator.key()] = iterator.value();
            }
        }

        /*
         计算反解能谱
        */
        calEnerygySpectrumCurve(false);
    });
    connect(detectorDataProcessor, &DataProcessor::showEnerygySpectrumCurve, this, &CommHelper::showEnerygySpectrumCurve);
}

void CommHelper::errorOccurred(QAbstractSocket::SocketError)
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (socket == mSocketRelay){
        if (mSocketConectedStatus & ssRelay){
            emit relayDisconnected();
        }
        mSocketConectedStatus ^= ssRelay;
    }
    else if (socket == mSocketDetector1){
        if (mSocketConectedStatus & ssDetector1){
            emit detectorDisconnected(1);
        }
        mSocketConectedStatus ^= ssDetector1;
    }
    else if (socket == mSocketDetector2){
        if (mSocketConectedStatus & ssDetector2){
            emit detectorDisconnected(2);
        }
        mSocketConectedStatus ^= ssDetector2;
    }
    else if (socket == mSocketDetector3){
        if (mSocketConectedStatus & ssDetector3){
            emit detectorDisconnected(3);
        }
        mSocketConectedStatus ^= ssDetector3;
    }

    mRelayIsConnected = mSocketConectedStatus & 0x01;
    mDetectorsIsConnected = mSocketConectedStatus & 0x0E;
}

void CommHelper::stateChanged(QAbstractSocket::SocketState state)
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (state == QAbstractSocket::SocketState::UnconnectedState){
        if (socket == mSocketRelay){
            if (mSocketConectedStatus & ssRelay){
                emit relayDisconnected();
            }

            mSocketConectedStatus ^= ssRelay;
        }
        else if (socket == mSocketDetector1){
            if (mSocketConectedStatus & ssDetector1){
                emit detectorDisconnected(1);
            }
            mSocketConectedStatus ^= ssDetector1;
        }
        else if (socket == mSocketDetector2){
            if (mSocketConectedStatus & ssDetector2){
                emit detectorDisconnected(2);
            }
            mSocketConectedStatus ^= ssDetector2;
        }
        else if (socket == mSocketDetector3){
            if (mSocketConectedStatus & ssDetector3){
                emit detectorDisconnected(3);
            }
            mSocketConectedStatus ^= ssDetector3;
        }

        mRelayIsConnected = mSocketConectedStatus & 0x01;
        mDetectorsIsConnected = mSocketConectedStatus & 0x0E;
    }
}


void CommHelper::socketConnected()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (socket == mSocketRelay){
        mSocketConectedStatus |= ssRelay;
        QTimer::singleShot(0, this, [=]{
            emit relayConnected();
        });
    }
    else if (socket == mSocketDetector1){
        mSocketConectedStatus |= ssDetector1;
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
    else if (socket == mSocketDetector2){
        mSocketConectedStatus |= ssDetector2;
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
    else if (socket == mSocketDetector3){
        mSocketConectedStatus |= ssDetector3;
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

    mRelayIsConnected = mSocketConectedStatus & 0x01;
    mDetectorsIsConnected = mSocketConectedStatus & 0x0E;
}

#include <QtEndian>
void CommHelper::socketReadyRead()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (socket->bytesAvailable() <= 0)
        return;

    QByteArray rawData = socket->readAll();
    if (socket == mSocketRelay){
        qDebug().noquote()<< "[" << 0 << "] "<<"Recv HEX: "<<rawData.toHex(' ');

        if (rawData.contains(askRelayPowerOnCmd)){
            // 继电器控制闭合返回
            emit relayPowerOn();
        }
        else if (rawData.contains(askRelayPowerOffCmd)){
            // 继电器控制断开返回
            emit relayPowerOff();
        }
        else if (rawData.contains(ackRelayPowerStatusOnCmd)){
            // 继电器闭合状态返回
            emit relayPowerOn();
        }
        else if (rawData.contains(ackRelayPowerStatusOffCmd)){
            // 继电器断开状态返回
            emit relayPowerOff();
        }
    }
}

bool CommHelper::connectRelay()
{
    GlobalSettings settings(CONFIG_FILENAME);
    QString ip = settings.value("Relay/ip").toString();
    qint32 port = settings.value("Relay/port").toInt();

    //断开网络连接
    if (mSocketRelay->isOpen() && mSocketRelay->state() == QAbstractSocket::ConnectedState){
        if (mSocketRelay->peerAddress().toString() != ip || mSocketRelay->peerPort() != port)
            mSocketRelay->close();
        else
            return true;
    }

    mSocketRelay->connectToHost(ip, port);
    mSocketRelay->waitForConnected(500);
    qDebug().noquote() << QObject::tr("连接继电器，%1:%2").arg(ip).arg(port);
    return mSocketRelay->isOpen() && mSocketRelay->state() == QAbstractSocket::ConnectedState;
}

void CommHelper::disconnectRelay()
{
    mSocketRelay->abort();
}

bool CommHelper::connectDetectors()
{
    GlobalSettings settings(CONFIG_FILENAME);
    QTcpSocket* sockets[] = {mSocketRelay, mSocketDetector1, mSocketDetector2, mSocketDetector3};
    for (int i=1; i<=3; ++i){        
        QString ip = settings.value(QString("Detector/%1/ip").arg(i)).toString();
        qint32 port = settings.value(QString("Detector/%1/port").arg(i)).toInt();

        if (sockets[i]->isOpen() && sockets[i]->state() == QAbstractSocket::ConnectedState){
            if (sockets[i]->peerAddress().toString() != ip || sockets[i]->peerPort() != port)
                sockets[i]->close();
            else
                continue;
        }

        sockets[i]->connectToHost(ip, port);
        qDebug().noquote() << QObject::tr("连接探测器%1，%2:%3").arg(i).arg(ip).arg(port);
    }
    return true;
}

void CommHelper::disconnectDetectors()
{
    QTcpSocket* sockets[] = {mSocketRelay, mSocketDetector1, mSocketDetector2, mSocketDetector3};
    for (int i=1; i<=3; ++i){        
        sockets[i]->abort();
    }
}

/*
 控制单路通断
*/
void CommHelper::sendRelayPowerSwitcherCmd(quint8 on/* = 0x01*/)
{
    if (nullptr == mSocketRelay || mSocketRelay->state() != QAbstractSocket::ConnectedState)
        return;

    if (on == 0x00)
        askCurrentCmd = askRelayPowerOffCmd;
    else
        askCurrentCmd = askRelayPowerOnCmd;
    mSocketRelay->write(askCurrentCmd);
    mSocketRelay->waitForBytesWritten();
    qDebug().noquote()<< "[" << 0 << "] "<<"Send HEX: "<<askCurrentCmd.toHex(' ');
}

/*
 查询状态
*/
void CommHelper::sendQueryRelayStatusCmd()
{
    if (nullptr == mSocketRelay || mSocketRelay->state() != QAbstractSocket::ConnectedState)
        return;

    askCurrentCmd = askQueryRelayPowerStatusCmd;
    mSocketRelay->write(askCurrentCmd);
    mSocketRelay->waitForBytesWritten();
    qDebug().noquote()<< "[" << 0 << "] "<<"Send HEX: "<<askCurrentCmd.toHex(' ');
}

/*
 触发阈值
*/
void CommHelper::sendTriggerTholdCmd()
{
    GlobalSettings settings(CONFIG_FILENAME);
    QString triggerThold = settings.value("Fpga/TriggerThold").toString();
    QList<QString> triggerTholds = triggerThold.split(',', Qt::SkipEmptyParts);
    while (triggerTholds.size() < 4)
        triggerTholds.push_back("200");

    QList<quint16> value = {triggerTholds[0].toUShort(), triggerTholds[1].toUShort(), triggerTholds[2].toUShort(), triggerTholds[3].toUShort()};
    QTcpSocket* sockets[] = {mSocketDetector1, mSocketDetector2, mSocketDetector3};
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
        qDebug().noquote()<<"Send HEX: "<<askCurrentCmd.toHex(' ');

        // CH4 CH3
        askCurrentCmd = QByteArray::fromHex(QString("12 34 00 0f fe 12 00 00 00 00 ab cd").toUtf8());
        askCurrentCmd[6] = value[2] >> 8;
        askCurrentCmd[7] = value[2] & 0x000FF;
        askCurrentCmd[8] = value[3] >> 8;
        askCurrentCmd[9] = value[3] & 0x000FF;
        socket->write(askCurrentCmd);
        qDebug().noquote()<<"Send HEX: "<<askCurrentCmd.toHex(' ');
    }
}

/*
 波形触发模式
*/
void CommHelper::sendWaveTriggerModeCmd()
{
    GlobalSettings settings(CONFIG_FILENAME);
    quint8 triggerMode = settings.value("Fpga/TriggerMode").toUInt();
    QTcpSocket* sockets[] = {mSocketDetector1, mSocketDetector2, mSocketDetector3};
    for (auto socket : sockets){
        if (nullptr == socket || socket->state() != QAbstractSocket::ConnectedState)
            continue;

        askCurrentCmd = QByteArray::fromHex(QString("12 34 00 0f fe 14 00 00 00 00 ab cd").toUtf8());
        askCurrentCmd[9] = triggerMode;
        socket->write(askCurrentCmd);
        qDebug().noquote()<<"Send HEX: "<<askCurrentCmd.toHex(' ');
    }
}

/*
 波形长度
*/
void CommHelper::sendWaveLengthCmd()
{
    GlobalSettings settings(CONFIG_FILENAME);
    quint8 waveLength = settings.value("Fpga/WaveLength").toUInt();
    QTcpSocket* sockets[] = {mSocketDetector1, mSocketDetector2, mSocketDetector3};
    for (auto socket : sockets){
        if (nullptr == socket || socket->state() != QAbstractSocket::ConnectedState)
            continue;

        askCurrentCmd = QByteArray::fromHex(QString("12 34 00 0f fe 15 00 00 00 00 ab cd").toUtf8());
        askCurrentCmd[9] = waveLength;
        socket->write(askCurrentCmd);
        qDebug().noquote()<<"Send HEX: "<<askCurrentCmd.toHex(' ');
    }
}

/*
 程控增益
*/
void CommHelper::sendGainCmd()
{
    GlobalSettings settings(CONFIG_FILENAME);
    QString gain = settings.value("Fpga/Gain").toString();
    QList<QString> gains = gain.split(',', Qt::SkipEmptyParts);
    while (gains.size() < 4)
        gains.push_back("5");

    QList<quint16> value = {gains[0].toUShort(), gains[1].toUShort(), gains[2].toUShort(), gains[3].toUShort()};
    QTcpSocket* sockets[] = {mSocketDetector1, mSocketDetector2, mSocketDetector3};
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
        qDebug().noquote()<<"Send HEX: "<<askCurrentCmd.toHex(' ');
    }
}

/*
 传输模式
*/
void CommHelper::sendTransferModeCmd(quint8 index, quint8 mode)
{
    QTcpSocket* sockets[] = {mSocketRelay, mSocketDetector1, mSocketDetector2, mSocketDetector3};
    if (nullptr == sockets[index] || sockets[index]->state() != QAbstractSocket::ConnectedState)
        return;

    askCurrentCmd = QByteArray::fromHex(QString("12 34 00 0f fa 13 00 00 00 00 ab cd").toUtf8());
    askCurrentCmd[9] = mode;
    sockets[index]->write(askCurrentCmd);
    sockets[index]->waitForBytesWritten();
    qDebug().noquote()<< "[" << index << "] "<<"Send HEX: "<<askCurrentCmd.toHex(' ');
    qDebug().noquote()<< "[" << index << "] " << QString::fromUtf8(">> 发送指令：传输模式-查询温度");
}

/*
 开始测量
*/
void CommHelper::sendMeasureCmd(quint8 mode)
{
    QTcpSocket* sockets[] = {mSocketDetector1, mSocketDetector2, mSocketDetector3};
    int index = 1;
    for (auto socket : sockets){
        if (nullptr == socket || socket->state() != QAbstractSocket::ConnectedState){
            index++;
            continue;
        }

        askCurrentCmd = QByteArray::fromHex(QString("12 34 00 0f ff 10 11 11 00 00 ab cd").toUtf8());
        askCurrentCmd[9] = mode;
        socket->write(askCurrentCmd);
        qDebug().noquote()<<"Send HEX: "<<askCurrentCmd.toHex(' ');
        qDebug().noquote()<< "[" << index++ << "] " << QString::fromUtf8(">> 发送指令：传输模式-波形测量");
    }
}

/*
 程序版本查询
*/
void CommHelper::sendQueryAppVersionCmd(quint8 index/* = 0x01*/)
{
    QTcpSocket* sockets[] = {mSocketRelay, mSocketDetector1, mSocketDetector2, mSocketDetector3};
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
    if (on == 0x01){
        // 直接发送温度传输模式
        this->sendTransferModeCmd(index, tmTemperature);
    }
    else {
        QTcpSocket* sockets[] = {mSocketRelay, mSocketDetector1, mSocketDetector2, mSocketDetector3};
        if (nullptr == sockets[index] || sockets[index]->state() != QAbstractSocket::ConnectedState)
            return;

        askCurrentCmd = QByteArray::fromHex(QString("12 34 00 0f fc 12 00 00 00 00 ab cd").toUtf8());
        sockets[index]->write(askCurrentCmd);
        sockets[index]->waitForBytesWritten();
        qDebug().noquote()<< "[" << 1 << "] "<<"Send HEX: "<<askCurrentCmd.toHex(' ');
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
    if (nullptr == mSocketDetector1 || mSocketDetector1->state() != QAbstractSocket::ConnectedState)
        return;

    askCurrentCmd = QByteArray::fromHex(QString("12 34 00 0f af 10 00 00 00 00 ab cd").toUtf8());
    askCurrentCmd[9] = on;
    mSocketDetector1->write(askCurrentCmd);
    qDebug().noquote()<< "[" << 1 << "] "<<"Send HEX: "<<askCurrentCmd.toHex(' ');
}

/*
 激光打开/关闭
*/
void CommHelper::sendLaserSwitcherCmd(quint8 on/* = 0x01*/)
{
    if (nullptr == mSocketDetector1 || mSocketDetector1->state() != QAbstractSocket::ConnectedState)
        return;

    askCurrentCmd = QByteArray::fromHex(QString("12 34 00 0f af 11 00 00 00 00 ab cd").toUtf8());
    askCurrentCmd[9] = on;
    mSocketDetector1->write(askCurrentCmd);
    qDebug().noquote()<< "[" << 1 << "] "<<"Send HEX: "<<askCurrentCmd.toHex(' ');
}

/*
 开始单次测量
*/
void CommHelper::sendSingleMeasureCmd()
{
    if (nullptr == mSocketDetector1 || mSocketDetector1->state() != QAbstractSocket::ConnectedState)
        return;

    mSingleMeasure = true;
    askCurrentCmd = QByteArray::fromHex(QString("12 34 00 0f af 12 00 00 00 00 ab cd").toUtf8());
    mSocketDetector1->write(askCurrentCmd);
    qDebug().noquote()<< "[" << 1 << "] "<<"Send HEX: "<<askCurrentCmd.toHex(' ');

    QMetaObject::invokeMethod(this, "measureDistanceStart", Qt::QueuedConnection);
}

/*
 开始连续测量
*/
void CommHelper::sendContinueMeasureCmd(quint8 on/* = 0x01*/)
{
    if (nullptr == mSocketDetector1 || mSocketDetector1->state() != QAbstractSocket::ConnectedState)
        return;

    mSingleMeasure = false;
    askCurrentCmd = QByteArray::fromHex(QString("12 34 00 0f af 13 00 00 00 00 ab cd").toUtf8());
    askCurrentCmd[9] = on;
    mSocketDetector1->write(askCurrentCmd);
    qDebug().noquote()<< "[" << 1 << "] "<<"Send HEX: "<<askCurrentCmd.toHex(' ');

    QMetaObject::invokeMethod(this, "measureDistanceEnd", Qt::QueuedConnection);
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
    // 关闭电源
    this->closePower();

    // 断开网络
    this->disconnectDetectors();
    this->disconnectRelay();
}

/*
 打开电源
*/
void CommHelper::openPower()
{
    if (mRelayIsConnected)
        this->sendRelayPowerSwitcherCmd();
}
/*
 断开电源
*/
void CommHelper::closePower()
{
    if (mRelayIsConnected){
        //先关闭探测器
        this->disconnectDetectors();

        //再发送关闭指令
        this->sendRelayPowerSwitcherCmd(0x00);
    }
}

/*
 设置发次信息
*/
#include <QDir>
void CommHelper::setShotInformation(const QString shotDir, const quint32 shotNum)
{
    this->mShotDir = shotDir;
    this->mShotNum = QString::number(shotNum);
}

void CommHelper::setResultInformation(const QString reverseValue, const QString dadiationDose, const QString dadiationDoseRate)
{
    this->mReverseValue = reverseValue;
    this->mDadiationDose = dadiationDose;
    this->mDadiationDoseRate = dadiationDoseRate;
}

/*
 开始测量
*/
void CommHelper::startMeasure(quint8 mode)
{
    DataProcessor* detectorsDataProcessor[] = {mDetector1DataProcessor, mDetector2DataProcessor, mDetector3DataProcessor};
    for (auto detectorDataProcessor : detectorsDataProcessor){
        detectorDataProcessor->startMeasureWave(mode);
    }

    /*清空波形数据*/
    if (mode == CommHelper::tmWaveMode)
        mWaveAllData.clear();

    /*开始测量，只需要发数据传输模式就行了*/
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
    if (on){
        // 先设置传输模式
        this->sendTransferModeCmd(index, tmTemperature);
    }
    else{
        this->sendQueryTemperaturCmd(index, on);
    }
}

/*
 继电器状态查询
*/
void CommHelper::queryRelayStatus()
{
    if (mRelayIsConnected)
        this->sendQueryRelayStatusCmd();
}

/*
 开始测距
*/
void CommHelper::startMeasureDistance(bool isContinue/* = false*/)
{
    mDistanceMeasuring = true;
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

/*
 打开测距模块电源
*/
void CommHelper::openDistanceModulePower()
{
    // 先设置传输模式
    this->sendTransferModeCmd(1, tmLaserDistance);

    this->sendPowerSwitcherCmd(0x01);
}
/*
 断开测距模块电源
*/
void CommHelper::closeDistanceModulePower()
{
    this->sendPowerSwitcherCmd(0x00);
}

/*
 打开测距模块激光
*/
void CommHelper::openDistanceModuleLaser()
{
    this->sendLaserSwitcherCmd(0x01);
}
/*
 断开测距模块激光
*/
void CommHelper::closeDistanceModuleLaser()
{
    this->sendLaserSwitcherCmd(0x00);
}

/*解析历史文件*/
bool CommHelper::openHistoryWaveFile(const QString &filePath)
{
    QFile file(filePath);
    if (file.open(QIODevice::ReadWrite)){
        mWaveAllData.clear();

        if (filePath.endsWith(".dat")){
            QVector<quint16> rawWaveData;
            QMap<quint8, QVector<quint16>> realCurve;// 4路通道实测曲线数据
            rawWaveData.resize(512);
            for (int i=1; i<=11; ++i){
                int rSize = file.read((char *)rawWaveData.data(), rawWaveData.size() * sizeof(quint16));
                if (rSize == 1024){
                    realCurve[i] = rawWaveData;

                    if (i == 4 || i == 8 || i == 11){
                        // 实测曲线
                        QMetaObject::invokeMethod(this, [=]() {
                            emit showHistoryCurve(realCurve);
                        }, Qt::DirectConnection);

                        realCurve.clear();
                    }
                }
                else
                {
                    file.close();
                    return false;
                }
            }
        }
        else{
            QVector<quint16> rawWaveData;
            QMap<quint8, QVector<quint16>> realCurve;// 4路通道实测曲线数据
            int chIndex = 1;
            while (!file.atEnd()){
                QByteArray lines = file.readLine();
                lines = lines.replace("\r\n", "");
                QList<QByteArray> listLine = lines.split(',');
                for( auto line : listLine){
                    //rawWaveData.push_back(qRound((line.toDouble() - 10996) * 0.9));
                    rawWaveData.push_back(qRound(line.toDouble() * 0.8));
                }

                if (rawWaveData.size() == 512){
                    realCurve[chIndex++] = rawWaveData;
                    // 实测曲线
                    QMetaObject::invokeMethod(this, [=]() {
                        emit showHistoryCurve(realCurve);
                    }, Qt::DirectConnection);

                    rawWaveData.clear();
                    realCurve.clear();
                }
            }

            // 尾巴数据（无效数据）
            if (rawWaveData.size() > 0){
                realCurve[chIndex++] = rawWaveData;
                QMetaObject::invokeMethod(this, [=]() {
                    emit showHistoryCurve(realCurve);
                }, Qt::DirectConnection);

                rawWaveData.clear();
                realCurve.clear();
            }
        }

        file.close();
        return true;
    }

    return false;
}

/*
 反解能谱
*/
void CommHelper::calEnerygySpectrumCurve(bool needSave)
{
    if (mWaveAllData.size() < 11)
        return;

    emit showRealCurve(mWaveAllData);

    //11个通道都收集完毕，可以进行反能谱计算了
    QVector<QPair<double, double>> result;

    QVector<quint16> rawWaveData;
    for (int i=1; i<=mWaveAllData.size(); ++i){
        rawWaveData.append(mWaveAllData[i]);
    }

    QString triggerTime = QDateTime::currentDateTime().toString("yyyy-MM-dd_HHmmss");
    if (needSave)
    {
        {
            QString oldFilePath = QString("%1/%2/测量数据/Settings.ini").arg(mShotDir).arg(mShotNum);
            QString newFilePath = QString("%1/%2/测量数据/%3_Settings.ini").arg(mShotDir).arg(mShotNum).arg(triggerTime);
            QFile::rename(oldFilePath, newFilePath);
        }

        /*保存波形数据*/
        /*二进制*/
        {
            QString filePath = QString("%1/%2/测量数据/%3_Wave.dat").arg(mShotDir).arg(mShotNum).arg(triggerTime);
            QFile file(filePath);
            if (file.open(QIODevice::WriteOnly)){
                file.write((const char *)rawWaveData.constData(), rawWaveData.size()*sizeof(quint16));
                file.close();
            }
        }

        /*csv*/
        {
            QString filePath = QString("%1/%2/测量数据/%3_Wave.csv").arg(mShotDir).arg(mShotNum).arg(triggerTime);
            QFile file(filePath);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)){
                QTextStream stream(&file);
                for (int i=1; i<=mWaveAllData.size(); ++i){
                    QVector<quint16> waveData = mWaveAllData[i];
                    for (int j = 0; j < waveData.size(); ++j){
                        stream << waveData.at(j);
                        if (j < waveData.size() - 1)
                            stream << ",";
                    }
                    stream << "\n";
                }

                file.close();
            }
        }
    }

#ifdef ENABLE_MATLAB
    if (gMatlabInited){
        if (mWaveAllData.size() >= 11){            

            // 反解能谱波形数据
            mwArray waveData(5632, 1, mxDOUBLE_CLASS);
            waveData.SetData(rawWaveData.constData(), 5632);

            // 输出数组
            int nargout = 2;//输出变量的个数是2
            mwArray unfold_seq(mxDOUBLE_CLASS, mxREAL);
            mwArray unfold_spec(mxDOUBLE_CLASS, mxREAL);
            UnfolddingAlgorithm_Gravel(nargout, unfold_seq, unfold_spec, m_mwT, m_mwSeq, waveData, m_mwResponce_matrix);

            double dbX[1024] = { 0 }, dbY[1024] = { 0 };
            int rowCnt = 0, colCnt = 0;
            {
                //读取结果数组
                mwSize  dims = unfold_seq.NumberOfDimensions();//矩阵的维数,2表示二维数组

                mwArray arrayDim = unfold_seq.GetDimensions();//各维的具体大小
                rowCnt = arrayDim.Get(dims, 1); //行数
                colCnt = arrayDim.Get(dims, 2); //列数

                double* unfold_seq_cpp = new double[rowCnt * colCnt];
                unfold_seq.GetData(unfold_seq_cpp, rowCnt * colCnt);
                for (int i = 0; i < rowCnt; ++i) {
                    dbX[i] = unfold_seq_cpp[i];
                }
                delete[] unfold_seq_cpp;
                unfold_seq_cpp = NULL;
            }

            {
                //读取结果数组
                mwSize  dims = unfold_spec.NumberOfDimensions();//矩阵的维数,2表示二维数组

                mwArray arrayDim = unfold_spec.GetDimensions();//各维的具体大小
                rowCnt = arrayDim.Get(dims, 1); //行数
                colCnt = arrayDim.Get(dims, 2); //列数

                double* unfold_spec_cpp = new double[rowCnt * colCnt];
                unfold_spec.GetData(unfold_spec_cpp, rowCnt * colCnt);

                for (int i = 0; i < rowCnt; ++i) {
                    dbY[i] = unfold_spec_cpp[i];
                }

                delete[] unfold_spec_cpp;
                unfold_spec_cpp = NULL;
            }

            //显示反能谱曲线图
            for (int i = 0; i < rowCnt; ++i) {
                result.push_back(qMakePair<double,double>(dbX[i], dbY[i]));
            }
            emit showEnerygySpectrumCurve(result);

            /*保存能谱数据*/
            if (needSave)
            {
                /*csv*/
                {
                    QString filePath = QString("%1/%2/处理数据/%3_En.csv").arg(mShotDir).arg(mShotNum).arg(triggerTime);
                    QFile file(filePath);
                    if (file.open(QIODevice::WriteOnly | QIODevice::Text)){
                        QTextStream stream(&file);
                        for (int i = 0; i < rowCnt; i++)
                        {
                            stream << dbX[i] << "," << dbY[i] << "\n";
                        }

                        file.close();
                    }
                }

                {
                    QString filePath = QString("%1/%2/处理数据/%3_Result.ini").arg(mShotDir).arg(mShotNum).arg(triggerTime);
                    GlobalSettings settings(filePath);
                    settings.setValue("Result/ReverseValue", mReverseValue);//反解能谱不确定值
                    settings.setValue("Result/DadiationDose", mDadiationDose);//辐照剂量(μGy)
                    settings.setValue("Result/DadiationDoseRate", mDadiationDoseRate);//辐照剂量率(μGy*h-1)
                }

                QString fileDir = QString("%1/%2").arg(mShotDir).arg(mShotNum);
                emit exportEnergyPlot(fileDir, triggerTime);
            }
        }
    }
#endif //ENABLE_MATLAB

    mWaveAllData.clear();
}

bool copyDir(const QString &src, const QString &dst, bool overwrite = true) {
    QDir srcDir(src);
    if (!srcDir.exists()) return false;

    QDir dstDir(dst);
    if (!dstDir.exists() && !dstDir.mkpath(".")) return false;

    foreach (QFileInfo info, srcDir.entryInfoList(QDir::NoDotAndDotDot | QDir::Files | QDir::Dirs)) {
        QString dstPath = dst + QDir::separator() + info.fileName();
        if (info.isDir()) {
            if (!copyDir(info.filePath(), dstPath, overwrite)) return false;
        } else {
            if (overwrite && QFile::exists(dstPath)) QFile::remove(dstPath);
            if (!QFile::copy(info.filePath(), dstPath)) return false;
        }
    }
    return true;
}

bool CommHelper::saveAs(QString dstPath)
{
    QString srcPath = QString("%1/%2").arg(mShotDir).arg(mShotNum);
    return copyDir(srcPath, dstPath);
}
