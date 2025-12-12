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

    connect(detectorDataProcessor, &DataProcessor::initSuccess, this, &CommHelper::initSuccess);
    connect(detectorDataProcessor, &DataProcessor::waitTriggerSignal, this, &CommHelper::waitTriggerSignal);

    connect(detectorDataProcessor, &DataProcessor::measureStart, this, &CommHelper::measureStart);

    connect(detectorDataProcessor, &DataProcessor::measureEnd, this, &CommHelper::measureEnd);

    connect(detectorDataProcessor, &DataProcessor::onRawWaveData, this, &CommHelper::onRawWaveData);

    connect(this, &CommHelper::showHistoryCurve, this, [=](const QMap<quint8, QVector<quint16>>& data){
        // 将map1的内容添加到map2
        for (auto iterator = data.constBegin(); iterator != data.constEnd(); ++iterator) {
            // 这里只保留前16个通道数据
            if (iterator.key() >=1 && iterator.key() <= 16){
                mWaveAllData[iterator.key()] = iterator.value();
            }
        }

        // 计算反解能谱
        calEnerygySpectrumCurve(false);
    });
}

void CommHelper::errorOccurred(QAbstractSocket::SocketError)
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (mDetectorsIsConnected){
        mDetectorsIsConnected = false;
        emit netDisconnected();
    }
}

void CommHelper::stateChanged(QAbstractSocket::SocketState state)
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (mDetectorsIsConnected && state == QAbstractSocket::SocketState::UnconnectedState){
        mDetectorsIsConnected = false;
        emit netDisconnected();
    }
}


void CommHelper::socketConnected()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    mDetectorsIsConnected = true;
    emit netConnected();
}


bool CommHelper::openDetector()
{
    if (nullptr == mSocketDetector || mSocketDetector->state() != QAbstractSocket::ConnectedState)
        return false;

    // 初始化指令
    mDetectorDataProcessor->sendInitCmd();
    return true;
}


void CommHelper::closeDetector()
{
    if (nullptr == mSocketDetector || mSocketDetector->state() != QAbstractSocket::ConnectedState)
        return;

    // 停止测量指令
    mDetectorDataProcessor->sendStopCmd();
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

void CommHelper::setResultInformation(const QString reverseValue/*, const QString dadiationDose, const QString dadiationDoseRate*/)
{
    this->mReverseValue = reverseValue;
    // this->mDadiationDose = dadiationDose;
    // this->mDadiationDoseRate = dadiationDoseRate;
}

/*
 开始测量
*/
void CommHelper::startMeasure(quint8 triggerMode, quint8 triggerType)
{
    if (nullptr == mSocketDetector || mSocketDetector->state() != QAbstractSocket::ConnectedState)
        return;

    mTriggerTimers = 0;
    mTriggerMode = triggerMode;
    mTriggerType = triggerType;

    /*设置波形模式*/
    mDetectorDataProcessor->startMeasure(mTriggerMode, mTriggerType);
}

/*
 停止测量
*/
void CommHelper::stopMeasure()
{
    if (nullptr == mSocketDetector || mSocketDetector->state() != QAbstractSocket::ConnectedState)
        return;

    /*清空波形数据*/
    mWaveAllData.clear();
    mDetectorDataProcessor->stopMeasure();
}


/*解析历史文件*/
bool CommHelper::openHistoryWaveFile(const QString &filePath)
{
    QFile file(filePath);
    if (file.open(QIODevice::ReadWrite)){
        mWaveAllData.clear();

        QByteArray rawWaveData;
        if (filePath.endsWith(".dat")){
            rawWaveData = file.readAll();
            onRawWaveData(rawWaveData, false);
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

    // 先矫正数据
    UnfoldSpec unfoldSpec;
    if (0/*m_nTriggerMode == DataProcessor::TriggerMode::tmTestTrigger*/) {
        unfoldSpec.setWaveData((unsigned char*)rawWaveData.constData());
    }
    else{
        unfoldSpec.func_waveCorrect((unsigned char*)rawWaveData.constData());
        mWaveAllData = unfoldSpec.getCorrWaveData();
    }

    //反解能谱
    unfoldSpec.setResFileName(this->mResMatrixFileName.toStdString());
    unfoldSpec.unfold();
    result = unfoldSpec.getUnfoldWaveData();

    emit showEnerygySpectrumCurve(result);

    /*保存能谱数据*/
    if (needSave)
    {
        /*csv*/
        {
            QString filePath = QString("%1/%2/%3_En.csv").arg(mShotDir).arg(mShotNum).arg(triggerTime);
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
            QString filePath = QString("%1/%2/%3_Result.ini").arg(mShotDir).arg(mShotNum).arg(triggerTime);
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

void CommHelper::onRawWaveData(const QByteArray rawWaveData, bool needSave)
{
    QVector<QPair<double, double>> unfoldData;//反解能谱

    UnfoldSpec unfoldSpec;
    if (mTriggerMode == DataProcessor::TriggerMode::tmTestTrigger) {
        //读取波形后，不矫正
        unfoldSpec.setWaveData((unsigned char*)rawWaveData.constData());
        mWaveAllData = unfoldSpec.getCorrWaveData();
    }
    else{
        // 读取波形后，先矫正波形
        unfoldSpec.func_waveCorrect((unsigned char*)rawWaveData.constData());
        mWaveAllData = unfoldSpec.getCorrWaveData();
    }

    //反解能谱
    unfoldSpec.setResFileName(this->mResMatrixFileName.toStdString());
    unfoldSpec.unfold();
    unfoldData = unfoldSpec.getUnfoldWaveData();

    // 绘制实测曲线
    {
        emit showRealCurve(mWaveAllData);
    }

    // 绘制反解能谱
    {
        emit showEnerygySpectrumCurve(unfoldData);
    }

    // 保存原始网口数据、界面上的波形数据、反解能谱数据等信息
    QString triggerTime = QDateTime::currentDateTime().toString("yyyy-MM-dd_HHmmss");
    if (needSave)
    {
        mTriggerTimers++;
        QMetaObject::invokeMethod(this, "refreshTriggerTimers", Qt::QueuedConnection, Q_ARG(quint8, mTriggerTimers));

        // {
        //     QString oldFilePath = QString("%1/%2/Settings.ini").arg(mShotDir).arg(mShotNum);
        //     QString newFilePath = QString("%1/%2/%3_Settings.ini").arg(mShotDir).arg(mShotNum).arg(triggerTime);
        //     QFile::rename(oldFilePath, newFilePath);
        // }

        /*保存波形数据*/
        /*(1)二进制*/
        {
            QString filePath = QString("%1/%2/%3_net.dat").arg(mShotDir).arg(mShotNum).arg(triggerTime);
            QFile file(filePath);
            if (file.open(QIODevice::WriteOnly)){
                file.write((const char *)rawWaveData.constData(), rawWaveData.size());
                file.close();
            }
        }
        /*(2)csv*/
        {
            QString filePath = QString("%1/%2/%3_Wave.csv").arg(mShotDir).arg(mShotNum).arg(triggerTime);
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

        /*波形校正数据csv*/
        /*{
            QString filePath = QString("%1/%2/%3_corr.csv").arg(mShotDir).arg(mShotNum).arg(triggerTime);
            QFile file(filePath);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)){
                QTextStream stream(&file);
                for (int i=1; i<=mWaveAllData.size(); ++i){
                    QVector<quint16> waveData = mWaveAllData[i];
                    stream << i << ",";//通道号
                    for (int j = 0; j < waveData.size(); ++j){
                        stream << waveData.at(j);
                        if (j < waveData.size() - 1)
                            stream << ",";
                    }
                    stream << "\n";
                }

                file.close();
            }
        }*/

        /*反解能谱csv*/
        {
            QString filePath = QString("%1/%2/%3_unfold.csv").arg(mShotDir).arg(mShotNum).arg(triggerTime);
            QFile file(filePath);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)){
                QTextStream stream(&file);
                for (int i = 0; i < unfoldData.size(); i++)
                {
                    stream << unfoldData[i].first << "," << unfoldData[i].second << "\n";
                }

                file.close();
            }
        }

        {
            QString filePath = QString("%1/%2/%3_Result.ini").arg(mShotDir).arg(mShotNum).arg(triggerTime);
            GlobalSettings settings(filePath);
            settings.setValue("Result/ReverseValue", mReverseValue);//反解能谱不确定值
            // settings.setValue("Result/DadiationDose", mDadiationDose);//辐照剂量(μGy)
            // settings.setValue("Result/DadiationDoseRate", mDadiationDoseRate);//辐照剂量率(μGy*h-1)
        }

        QString fileDir = QString("%1/%2").arg(mShotDir).arg(mShotNum);
        emit exportEnergyPlot(fileDir, triggerTime);
    }
}
