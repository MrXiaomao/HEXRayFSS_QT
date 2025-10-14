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

    /*初始化网络*/
    initSocket(&mSocketDetector);

    /*创建数据处理器*/
    initDataProcessor(&mDetectorDataProcessor, mSocketDetector);

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
    mSocketDetector->setProperty("_q_networksession", QVariant::fromValue(spNetworkSession));
}

CommHelper::~CommHelper()
{
    if(unfoldData != nullptr ){
        delete unfoldData;
        unfoldData = nullptr;
    }

    auto closeSocket = [&](QTcpSocket* socket){
        if (socket){
            socket->disconnectFromHost();
            socket->close();
            socket->deleteLater();
            socket = nullptr;
        }
    };

    closeSocket(mSocketDetector);
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
}

void CommHelper::initDataProcessor(DataProcessor** processor, QTcpSocket *socket)
{
    DataProcessor* detectorDataProcessor = new DataProcessor(socket, this);
    *processor = detectorDataProcessor;

    connect(detectorDataProcessor, &DataProcessor::detectorConnected, this, &CommHelper::detectorConnected);
    connect(detectorDataProcessor, &DataProcessor::detectorDisconnected, this, &CommHelper::detectorDisconnected);

    connect(detectorDataProcessor, &DataProcessor::measureStart, this, [=](){
        mWaveMeasuring = true;
    });
    connect(detectorDataProcessor, &DataProcessor::measureStart, this, &CommHelper::measureStart);

    connect(detectorDataProcessor, &DataProcessor::measureEnd, this, [=](){
        mWaveMeasuring = false;
    });
    connect(detectorDataProcessor, &DataProcessor::measureEnd, this, &CommHelper::measureEnd);

    connect(detectorDataProcessor, &DataProcessor::showRealCurve, this, [=](const QMap<quint8, QVector<quint16>>& data){
        // 将map1的内容添加到map2
        for (auto iterator = data.constBegin(); iterator != data.constEnd(); ++iterator) {
            // 这里只保留前16个通道数据
            if (iterator.key() >=1 && iterator.key() <= 16){
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
            // 这里只保留前16个通道数据
            if (iterator.key() >=1 && iterator.key() <= 16){
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
    mDetectorsIsConnected = false;
    emit detectorDisconnected();
}

void CommHelper::stateChanged(QAbstractSocket::SocketState state)
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (state == QAbstractSocket::SocketState::UnconnectedState){
        mDetectorsIsConnected = false;
        emit detectorDisconnected();
    }
}


void CommHelper::socketConnected()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    mDetectorsIsConnected = true;
    emit detectorConnected();
}


bool CommHelper::connectDetectors()
{
    GlobalSettings settings(CONFIG_FILENAME);     
    QString ip = settings.value("Detector/ip").toString();
    qint32 port = settings.value("Detector/port").toInt();

    if (mSocketDetector->isOpen() && mSocketDetector->state() == QAbstractSocket::ConnectedState){
        if (mSocketDetector->peerAddress().toString() != ip || mSocketDetector->peerPort() != port)
            mSocketDetector->close();
        else
            return true;
    }

    mSocketDetector->connectToHost(ip, port);
    qDebug().noquote() << QObject::tr("连接探测器%1:%2").arg(ip).arg(port);
    return true;
}


void CommHelper::disconnectDetectors()
{
    mSocketDetector->abort();
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
    if (nullptr == mSocketDetector || mSocketDetector->state() != QAbstractSocket::ConnectedState)
        return;

    // CH2 CH1
    askCurrentCmd = QByteArray::fromHex(QString("12 34 00 0f fe 11 00 00 00 00 ab cd").toUtf8());
    askCurrentCmd[6] = value[0] >> 8;
    askCurrentCmd[7] = value[0] & 0x000FF;
    askCurrentCmd[8] = value[1] >> 8;
    askCurrentCmd[9] = value[1] & 0x000FF;
    mSocketDetector->write(askCurrentCmd);
    qDebug().noquote()<<"Send HEX: "<<askCurrentCmd.toHex(' ');

    // CH4 CH3
    askCurrentCmd = QByteArray::fromHex(QString("12 34 00 0f fe 12 00 00 00 00 ab cd").toUtf8());
    askCurrentCmd[6] = value[2] >> 8;
    askCurrentCmd[7] = value[2] & 0x000FF;
    askCurrentCmd[8] = value[3] >> 8;
    askCurrentCmd[9] = value[3] & 0x000FF;
    mSocketDetector->write(askCurrentCmd);
    qDebug().noquote()<<"Send HEX: "<<askCurrentCmd.toHex(' ');
}

/*
 波形触发模式
*/
void CommHelper::sendWaveTriggerModeCmd()
{
    GlobalSettings settings(CONFIG_FILENAME);
    quint8 triggerMode = settings.value("Fpga/TriggerMode").toUInt();
    if (nullptr == mSocketDetector || mSocketDetector->state() != QAbstractSocket::ConnectedState)
        return;

    askCurrentCmd = QByteArray::fromHex(QString("12 34 00 0f fe 14 00 00 00 00 ab cd").toUtf8());
    askCurrentCmd[9] = triggerMode;
    mSocketDetector->write(askCurrentCmd);
    qDebug().noquote()<<"Send HEX: "<<askCurrentCmd.toHex(' ');
}

/*
 波形长度
*/
void CommHelper::sendWaveLengthCmd()
{
    GlobalSettings settings(CONFIG_FILENAME);
    quint8 waveLength = settings.value("Fpga/WaveLength").toUInt();
    if (nullptr == mSocketDetector || mSocketDetector->state() != QAbstractSocket::ConnectedState)
        return;

    askCurrentCmd = QByteArray::fromHex(QString("12 34 00 0f fe 15 00 00 00 00 ab cd").toUtf8());
    askCurrentCmd[9] = waveLength;
    mSocketDetector->write(askCurrentCmd);
    qDebug().noquote()<<"Send HEX: "<<askCurrentCmd.toHex(' ');
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
    if (nullptr == mSocketDetector || mSocketDetector->state() != QAbstractSocket::ConnectedState)
        return;

    // CH2 CH1
    askCurrentCmd = QByteArray::fromHex(QString("12 34 00 0f fb 11 00 00 00 00 ab cd").toUtf8());
    askCurrentCmd[6] = value[3];
    askCurrentCmd[7] = value[2];
    askCurrentCmd[8] = value[1];
    askCurrentCmd[9] = value[0];
    mSocketDetector->write(askCurrentCmd);
    qDebug().noquote()<<"Send HEX: "<<askCurrentCmd.toHex(' ');
}

/*
 开始测量
*/
void CommHelper::sendMeasureCmd(quint8 mode)
{
    if (nullptr == mSocketDetector || mSocketDetector->state() != QAbstractSocket::ConnectedState)
        return;

    askCurrentCmd = QByteArray::fromHex(QString("12 34 00 0f ff 10 11 11 00 00 ab cd").toUtf8());
    askCurrentCmd[9] = mode;
    mSocketDetector->write(askCurrentCmd);
    qDebug().noquote()<<"Send HEX: "<<askCurrentCmd.toHex(' ');
    qDebug().noquote()<< QString::fromUtf8(">> 发送指令：传输模式-波形测量");
}

/////////////////////////////////////////////////////////////////////////////////
/*
 连接网络
*/
void CommHelper::connectNet()
{
    if (nullptr == mSocketDetector || mSocketDetector->state() == QAbstractSocket::ConnectedState)
        return;

    GlobalSettings settings(CONFIG_FILENAME);
    QString ip = settings.value("Detector/ip").toString();
    qint32 port = settings.value("Detector/port").toInt();

    if (mSocketDetector->isOpen() && mSocketDetector->state() == QAbstractSocket::ConnectedState){
        if (mSocketDetector->peerAddress().toString() != ip || mSocketDetector->peerPort() != port)
            mSocketDetector->close();
        else
            return ;
    }

    mSocketDetector->connectToHost(ip, port);
    qDebug().noquote() << QObject::tr("连接探测器%1:%2").arg(ip).arg(port);
}

/*
 断开网络
*/
void CommHelper::disconnectNet()
{
    if (nullptr == mSocketDetector || mSocketDetector->state() != QAbstractSocket::ConnectedState)
        return;

    mSocketDetector->abort();
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
void CommHelper::startMeasure(quint8 triggerMode)
{
    /*清空波形数据*/
    mWaveAllData.clear();

    mDetectorDataProcessor->startMeasureWave(triggerMode, true);
}

/*
 停止测量
*/
void CommHelper::stopMeasure()
{
    this->sendMeasureCmd(TriggerMode::tmStop);
}


/*解析历史文件*/
bool CommHelper::openHistoryWaveFile(const QString &filePath)
{
    QFile file(filePath);
    if (file.open(QIODevice::ReadWrite)){
        mWaveAllData.clear();

        QVector<quint16> rawWaveData;
        QMap<quint8, QVector<quint16>> realCurve;// 4路通道实测曲线数据
        if (filePath.endsWith(".dat")){                        
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
            }
        }
        else{
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

/* 设置矩阵响应文件 */
void CommHelper::setResMatrixFileName(const QString &fileName)
{
    this->mResMatrixFileName = fileName;
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
    }
#else
    if(unfoldData != nullptr ){
        delete unfoldData;
        unfoldData = nullptr;
    }

    unfoldData = new UnfoldSpec();
    unfoldData->setResFileName(this->mResMatrixFileName);
    result = unfoldData->pulseSum(mWaveAllData);
#endif //ENABLE_MATLAB

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
                for (int i = 0; i < result.size(); i++)
                {
                    stream << result[i].first << "," << result[i].second << "\n";
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
