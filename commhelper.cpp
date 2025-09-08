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
    , m_mwResponce_matrix(15, 500, mxDOUBLE_CLASS)
    , m_mwRom(1024,16, mxDOUBLE_CLASS)
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

            double* rom_cpp = new double[1024 * 16];
            loadRom(rom_cpp);
            m_mwRom.SetData(rom_cpp, 1024 * 16);
            delete[] rom_cpp;
            rom_cpp = nullptr;

            double* responce_matrix_cpp = new double[15 * 500];
            loadResponceMatrix(responce_matrix_cpp);
            m_mwResponce_matrix.SetData(responce_matrix_cpp, 15 * 500);
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


#ifdef ENABLE_MATLAB
    UnfolddingAlgorithm_GravelTerminate();
    mclTerminateApplication();
#endif //ENABLE_MATLAB
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
        for (auto it = data.constBegin(); it != data.constEnd(); ++it) {
            waveAllData[it.key()] = it.value();
        }

#ifdef ENABLE_MATLAB
        if (gMatlabInited){
            if (waveAllData.size() >= 11){
                //11个通道都收集完毕，可以进行反能谱计算了
                QVector<QPair<float, float>> result;

                QVector<quint16> rawWaveData;
                for (int i=1; i<=waveAllData.size(); ++i){
                    rawWaveData.append(data[i]);
                }

                // 反解能谱波形数据
                mwArray waveData(11264, 1, mxDOUBLE_CLASS);
                waveData.SetData(rawWaveData.constData(), 11264);

                // 输出数组
                int nargout = 2;//输出变量的个数是2
                mwArray unfold_seq(mxDOUBLE_CLASS, mxREAL);
                mwArray unfold_spec(mxDOUBLE_CLASS, mxREAL);
                //UnfolddingAlgorithm_Gravel(nargout, unfold_seq, unfold_spec, m_mwT, m_mwSeq, waveData, m_mwResponce_matrix);

                double dbX[1024] = { 0 }, dbY[1024] = { 0 };
                {
                    //读取结果数组
                    mwSize  dims = unfold_seq.NumberOfDimensions();//矩阵的维数,2表示二维数组

                    mwArray arrayDim = unfold_seq.GetDimensions();//各维的具体大小
                    int rowCnt = arrayDim.Get(dims, 1); //行数
                    int colCnt = arrayDim.Get(dims, 2); //列数

                    double* unfold_seq_cpp = new double[rowCnt * colCnt];
                    unfold_seq.GetData(unfold_seq_cpp, rowCnt * colCnt);
                    for (int i = 0; i < 342; ++i) {
                        dbX[i] = unfold_seq_cpp[i];
                    }
                    delete[] unfold_seq_cpp;
                    unfold_seq_cpp = NULL;
                }

                {
                    //读取结果数组
                    mwSize  dims = unfold_spec.NumberOfDimensions();//矩阵的维数,2表示二维数组

                    mwArray arrayDim = unfold_spec.GetDimensions();//各维的具体大小
                    int rowCnt = arrayDim.Get(dims, 1); //行数
                    int colCnt = arrayDim.Get(dims, 2); //列数

                    double* unfold_spec_cpp = new double[rowCnt * colCnt];
                    unfold_spec.GetData(unfold_spec_cpp, rowCnt * colCnt);

                    for (int i = 0; i < 342; ++i) {
                        dbY[i] = unfold_spec_cpp[i];
                    }

                    delete[] unfold_spec_cpp;
                    unfold_spec_cpp = NULL;
                }

                //显示反能谱曲线图
                for (int i = 0; i < 342; ++i) {
                    result.push_back(qMakePair<float,float>(dbX[i], dbY[i]));
                }
                showEnerygySpectrumCurve(result);
            }
        }
#endif //ENABLE_MATLAB
    });
    connect(detectorDataProcessor, &DataProcessor::showRealCurve, this, &CommHelper::showRealCurve);
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
    JsonSettings* ipSettings = GlobalSettings::instance()->mIpSettings;
    ipSettings->prepare();
    ipSettings->beginGroup("Relay");
    QString ip = ipSettings->value("ip").toString();
    qint32 port = ipSettings->value("port").toInt();
    ipSettings->endGroup();
    ipSettings->finish();

    //断开网络连接
    if (mSocketRelay->isOpen() && mSocketRelay->state() == QAbstractSocket::ConnectedState)
        mSocketRelay->abort();

    mSocketRelay->connectToHost(ip, port);
    mSocketRelay->waitForConnected(500);
    return mSocketRelay->isOpen() && mSocketRelay->state() == QAbstractSocket::ConnectedState;
}

void CommHelper::disconnectRelay()
{
    mSocketRelay->abort();
}

bool CommHelper::connectDetectors()
{
    JsonSettings* ipSettings = GlobalSettings::instance()->mIpSettings;
    ipSettings->prepare();

    QTcpSocket* sockets[] = {mSocketRelay, mSocketDetector1, mSocketDetector2, mSocketDetector3};
    for (int i=1; i<=3; ++i){
        ipSettings->beginGroup(QString("Detector%1").arg(i));
        QString ip = ipSettings->value("ip").toString();
        qint32 port = ipSettings->value("port").toInt();
        ipSettings->endGroup();

        if (sockets[i]->isOpen() && sockets[i]->state() == QAbstractSocket::ConnectedState)
            sockets[i]->abort();

        sockets[i]->connectToHost(ip, port);
        // sockets[i]->waitForConnected(500);
        // if (!mSocketRelay->isOpen() || sockets[i]->state() != QAbstractSocket::ConnectedState){
        //     ipSettings->finish();
        //     return false;
        // }
    }

    ipSettings->finish();
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
    qDebug().noquote()<< "[" << 0 << "] "<<"Send HEX: "<<askCurrentCmd.toHex(' ');
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
    JsonSettings* fpgaSettings = GlobalSettings::instance()->mFpgaSettings;
    fpgaSettings->prepare();

    fpgaSettings->beginGroup();
    quint8 triggerMode = fpgaSettings->value("TriggerMode").toUInt();
    fpgaSettings->endGroup();
    fpgaSettings->finish();

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
    JsonSettings* fpgaSettings = GlobalSettings::instance()->mFpgaSettings;
    fpgaSettings->prepare();

    fpgaSettings->beginGroup();
    quint8 waveLength = fpgaSettings->value("WaveLength").toUInt();
    fpgaSettings->endGroup();
    fpgaSettings->finish();

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
void CommHelper::setShotInformation(QString shotDir, QString shotNum)
{
    this->mShotDir = shotDir;
    this->mShotNum = shotNum;
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
        waveAllData.clear();

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
