#include "dataprocessor.h"
#include <QDebug>
#include <QTimer>

DataProcessor::DataProcessor(quint8 index, QTcpSocket* socket, QObject *parent)
    : QObject{parent}
    , mdetectorIndex(index)
    , mSocket(socket)
{
    mdataProcessThread = new QLiteThread(this);
    mdataProcessThread->setObjectName("mdataProcessThread");
    mdataProcessThread->setWorkThreadProc([=](){
        OnDataProcessThread();
    });
    mdataProcessThread->start();
    connect(this, &DataProcessor::destroyed, [=]() {
        mdataProcessThread->exit(0);
        mdataProcessThread->wait(500);
    });

    connect(mSocket, SIGNAL(readyRead()), this, SLOT(socketReadyRead()));

    // QString strTime = QDateTime::currentDateTime().toString("yyyy-MM-dd_HHmmss");
    // QString filePath = QString("%1%2_%3.dat").arg("./cache/").arg(mdetectorIndex).arg(strTime);
    // mpfSaveNet = new QFile(filePath);
    // if (mpfSaveNet->open(QIODevice::WriteOnly)) {
    //     qDebug().noquote()<< "[" << mdetectorIndex << "] " << tr("创建网口数据缓存文件成功，文件名：%1").arg(filePath);
    // } else {
    //     qDebug().noquote()<< "[" << mdetectorIndex << "] " << tr("创建网口数据缓存文件失败，文件名：%1").arg(filePath);
    // }
}

DataProcessor::~DataProcessor()
{
    // 终止线程
    mTerminatedThead = true;
    mDataReady = true;
    mCondition.wakeAll();
    mdataProcessThread->wait();

    // if (mpfSaveNet){
    //     mpfSaveNet->close();
    //     mpfSaveNet->deleteLater();
    //     mpfSaveNet = nullptr;
    // }
}

void DataProcessor::socketReadyRead()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (socket->bytesAvailable() <= 0)
        return;

    QByteArray rawData = socket->readAll();
    // if (mpfSaveNet){
    //     mpfSaveNet->write((const char *)rawData.constData(), rawData.size());
    //     mpfSaveNet->flush();
    // }
    this->inputData(rawData);
}

void DataProcessor::inputData(const QByteArray& data)
{
    {
        QMutexLocker locker(&mDataLocker);
        mRawData.append(data);

        if (mdetectorIndex == 1)
            qDebug().noquote()<< "[" << mdetectorIndex << "] "<< "inputData: " << data.toHex(' ') << " >> " << mRawData.toHex(' ');

        mDataReady = true;
    }

    mCondition.wakeAll();
}

void DataProcessor::OnDataProcessThread()
{
    static const int BASE_CMD_LENGTH = 12;
    static const int TEMPERATURE_CMD_LENGTH = 6; //温度指令长度
    static const int VERSION_CMD_LENGTH = 10; //版本号指令长度
    static const int ONE_CHANNEL_WAVE_SIZE = 1030;// 1030=(512+3)*2 单通道数据长度

    QByteArray waveDataHead = QByteArray::fromHex(QString("AB AB FF").toUtf8());;// 数据头
    QByteArray waveDataTail = QByteArray::fromHex(QString("CD CD").toUtf8());;// 数据尾

    while (!mTerminatedThead)
    {
        {
            QMutexLocker locker(&mDataLocker);
            if (mRawData.size() == 0){
                while (!mDataReady){
                    mCondition.wait(&mDataLocker);
                }

                mCachePool.append(mRawData);
                mRawData.clear();
                mDataReady = false;
            }
            else{
                mCachePool.append(mRawData);
                mRawData.clear();
            }
        }

        if (mCachePool.size() < TEMPERATURE_CMD_LENGTH)
            continue;

        while (true){
            if (mCachePool.size() < TEMPERATURE_CMD_LENGTH)
                break;

            /*是否找到完整帧（命令帧/数据帧）*/
            bool findNaul = false;

            // 数据长度从小到大依次判断
            // 温度(6)< 程序版本号(10) < 指令(12) < 波形数据(1030)

            /*温度*/
            // 0xAABB + 16bit（温度） + 0xCCDD/0xABCD
            if (mCachePool.startsWith(QByteArray::fromHex(QString("AA BB").toUtf8()))){
                if (mCachePool.size() >= TEMPERATURE_CMD_LENGTH){
                    if (mCachePool.mid(4, 2) == QByteArray::fromHex(QString("CC DD").toUtf8()) ||
                        mCachePool.mid(4, 2) == QByteArray::fromHex(QString("AB CD").toUtf8())){
                        QByteArray data = mCachePool.mid(2, 2);
                        qint16 t = qFromBigEndian<qint16>(data.constData());
                        float temperature = t * 0.0078125;// 换算系数

                        //qDebug().noquote()<< "[" << mdetectorIndex << "] "<<"Temperature "<< mdetectorIndex << ": " << temperature;
                        emit temperatureRespond(mdetectorIndex, temperature);

                        findNaul = true;
                        mCachePool.remove(0, TEMPERATURE_CMD_LENGTH);
                        continue;
                    }
                }
                else{
                    findNaul = true;
                    break;
                    //continue;
                }
            }

            /*程序版本号*/
            // 0xACAC + 48bit（版本号） + 0xEFEF/0xABCD
            if (mCachePool.startsWith(QByteArray::fromHex(QString("AC AC").toUtf8()))){
                if (mCachePool.size() >= VERSION_CMD_LENGTH){
                    if (mCachePool.mid(8, 2) == QByteArray::fromHex(QString("EF EF").toUtf8()) ||
                        mCachePool.mid(8, 2) == QByteArray::fromHex(QString("AB CD").toUtf8())){
                        QByteArray year = mCachePool.mid(2, 2);
                        QByteArray month = mCachePool.mid(4, 1);
                        QByteArray day = mCachePool.mid(5, 1);
                        QByteArray serialNumber = mCachePool.mid(6, 2);

                        QString version = year.toHex().toUpper() +
                                          month.toHex().toUpper() +
                                          day.toHex().toUpper();

                        qDebug().noquote()<< "[" << mdetectorIndex << "] "<<"AppVersion "<< mdetectorIndex<< ": "<< version<< ", serialNumber: " << serialNumber.toHex().toUpper();
                        emit appVersionRespond(mdetectorIndex, version, QString(serialNumber.toHex().toUpper()));

                        findNaul = true;
                        mCachePool.remove(0, VERSION_CMD_LENGTH);
                        continue;
                    }
                }
                else{
                    findNaul = true;
                    break;
                    //continue;
                }
            }

            /*指令*/
            /*指令起始字节是12 34 00 0F/AB，尾字节是AB CD*/
            if (mCachePool.startsWith(QByteArray::fromHex(QString("12 34 00").toUtf8()))){
                if (mCachePool.size() >= BASE_CMD_LENGTH){
                    if (mCachePool.startsWith(QByteArray::fromHex(QString("12 34 00").toUtf8())) &&
                        mCachePool.mid(10, 2) == QByteArray::fromHex(QString("AB CD").toUtf8())){
                        /*找到指令帧了！！！*/
                        /*继续判断帧类型*/

                        // 指令返回-距离测量
                        if (mCachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F AF 12 00 00 00 00 AB CD").toUtf8())) ||   //单次
                             mCachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F AF 13 00 00 00 00 Ab CD").toUtf8())) ||  //连续-关闭
                             mCachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F AF 13 00 00 00 01 Ab CD").toUtf8()))){   //连续-打开
                            findNaul = true;
                            mCachePool.remove(0, BASE_CMD_LENGTH);
                            continue;
                        }

                        // 数据返回-测量距离
                        if (mCachePool.startsWith(QByteArray::fromHex(QString("12 34 00 AB").toUtf8())) &&
                            mCachePool.mid(10, 2) == QByteArray::fromHex(QString("AB CD").toUtf8())){
                            QByteArray data = mCachePool.mid(4, 4);
                            QString string = data.toHex();
                            string.insert(5, '.');
                            float distance = string.toFloat();

                            data = mCachePool.mid(8, 2);
                            quint16 quality = data.toHex().toShort();

                            qDebug().noquote()<< "[" << mdetectorIndex << "] "<<"Distance: "<< distance << ", quality: " << quality;
                            emit distanceRespond(distance, quality);

                            if (mSingleMeasure){
                                mDistanceMeasuring = false;
                            }

                            findNaul = true;
                            mCachePool.remove(0, BASE_CMD_LENGTH);
                            continue;
                        }


                        // 硬件触发反馈
                        if (mCachePool.startsWith(QByteArray::fromHex(QString("12 34 00 AA 00 0C 00 00 00 00 AB CD").toUtf8()))){
                            qDebug().noquote()<< "[" << mdetectorIndex << "] " << QString::fromUtf8("<< 反馈指令：硬件触发");

                            findNaul = true;
                            mCachePool.remove(0, BASE_CMD_LENGTH);
                            continue;
                        }

                        // 指令返回-探测器-传输模式-程序版本号
                        if (mCachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F FA 13 00 00 00 01 AB CD").toUtf8()))){
                            mCachePool.remove(0, BASE_CMD_LENGTH);
                            qDebug().noquote()<< "[" << mdetectorIndex << "] " << QString::fromUtf8("<< 反馈指令：传输模式-程序版本号");

                            // 再发送查询指令
                            QTimer::singleShot(0, this, [=]{
                                QByteArray askCurrentCmd = QByteArray::fromHex(QString("12 34 00 0f fc 11 00 00 00 00 ab cd").toUtf8());
                                mSocket->write(askCurrentCmd);
                                mSocket->waitForBytesWritten();
                                qDebug().noquote()<< "[" << mdetectorIndex << "] "<<"Send HEX: "<<askCurrentCmd.toHex(' ');
                                qDebug().noquote()<< "[" << mdetectorIndex << "] " << QString::fromUtf8(">> 发送指令：传输模式-查询程序版本号");
                            });

                            findNaul = true;
                            mCachePool.remove(0, BASE_CMD_LENGTH);
                            continue;
                        }

                        // 指令返回-探测器-传输模式-波形
                        if (mCachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F FA 13 00 00 00 03 AB CD").toUtf8()))){
                            qDebug().noquote()<< "[" << mdetectorIndex << "] " << QString::fromUtf8("<< 反馈指令：传输模式-波形");

                            QMetaObject::invokeMethod(this, "measureStart", Qt::QueuedConnection, Q_ARG(quint8, mdetectorIndex));

                            //最后发送开始测量-软件触发模式
                            QTimer::singleShot(0, this, [=]{
                                QByteArray askCurrentCmd = QByteArray::fromHex(QString("12 34 00 0f ff 10 11 11 00 01 ab cd").toUtf8());
                                //askCurrentCmd[9] = mTransferMode;
                                mSocket->write(askCurrentCmd);
                                mSocket->waitForBytesWritten();
                                qDebug().noquote()<< "[" << mdetectorIndex << "] "<<"Send HEX: "<<askCurrentCmd.toHex(' ');
                                qDebug().noquote()<< "[" << mdetectorIndex << "] " << QString::fromUtf8(">> 发送指令：传输模式-开始波形测量");
                            });

                            findNaul = true;
                            mCachePool.remove(0, BASE_CMD_LENGTH);
                            continue;
                        }

                        // 指令返回-探测器-传输模式-温度
                        if (mCachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F FA 13 00 00 00 05 AB CD").toUtf8()))){
                            qDebug().noquote()<< "[" << mdetectorIndex << "] " << QString::fromUtf8("<< 反馈指令：传输模式-温度");

                            findNaul = true;
                            mCachePool.remove(0, BASE_CMD_LENGTH);

                            //再发送温度查询指令
                            QTimer::singleShot(0, this, [=]{
                                QByteArray askCurrentCmd = QByteArray::fromHex(QString("12 34 00 0f fc 12 00 00 00 01 ab cd").toUtf8());
                                mSocket->write(askCurrentCmd);
                                mSocket->waitForBytesWritten();
                                qDebug().noquote()<< "[" << mdetectorIndex << "] "<<"Send HEX: "<<askCurrentCmd.toHex(' ');
                                qDebug().noquote()<< "[" << mdetectorIndex << "] " << QString::fromUtf8("<< 发送指令：探测器-温度查询");
                            });

                            continue;
                        }

                        // 指令返回-探测器-传输模式-激光测距
                        if (mCachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F FA 13 00 00 00 09 AB CD").toUtf8()))){
                            qDebug().noquote()<< "[" << mdetectorIndex << "] " << QString::fromUtf8("<< 反馈指令：传输模式-激光测距");

                            findNaul = true;
                            mCachePool.remove(0, BASE_CMD_LENGTH);
                            continue;
                        }

                        // 指令返回-探测器-程控增益
                        if (mCachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F FB 11").toUtf8())) &&
                            mCachePool.mid(10, 2) == QByteArray::fromHex(QString("AB CD").toUtf8())){
                            qDebug().noquote()<< "[" << mdetectorIndex << "] " << QString::fromUtf8("<< 反馈指令：程控增益");

                            findNaul = true;
                            mCachePool.remove(0, BASE_CMD_LENGTH);
                            continue;
                        }

                        // 指令返回-探测器-程序版本查询
                        if (mCachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F FC 11 00 00 00 00 AB CD").toUtf8()))){
                            qDebug().noquote()<< "[" << mdetectorIndex << "] " << QString::fromUtf8("<< 反馈指令：程序版本查询");

                            findNaul = true;
                            mCachePool.remove(0, BASE_CMD_LENGTH);
                            continue;
                        }

                        // 指令返回-探测器-温度查询-开始
                        if (mCachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F FC 12 00 00 00 01 AB CD").toUtf8()))){
                            qDebug().noquote()<< "[" << mdetectorIndex << "] " << QString::fromUtf8("<< 反馈指令：温度查询-开始");

                            findNaul = true;
                            mCachePool.remove(0, BASE_CMD_LENGTH);
                            continue;
                        }

                        // 指令返回-探测器-温度查询-停止
                        if (mCachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F FC 12 00 00 00 00 AB CD").toUtf8()))){
                            qDebug().noquote()<< "[" << mdetectorIndex << "] " << QString::fromUtf8("<< 反馈指令：温度查询-停止");

                            findNaul = true;
                            mCachePool.remove(0, BASE_CMD_LENGTH);
                            continue;
                        }

                        // 指令返回-探测器-触发阈值
                        if ((mCachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F FE 11").toUtf8())) ||
                             mCachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F FE 12").toUtf8()))) &&
                            mCachePool.mid(10, 2) == QByteArray::fromHex(QString("AB CD").toUtf8())){
                            qDebug().noquote()<< "[" << mdetectorIndex << "] " << QString::fromUtf8("<< 反馈指令：触发阈值");

                            findNaul = true;
                            mCachePool.remove(0, BASE_CMD_LENGTH);
                            continue;
                        }

                        // 指令返回-探测器-波形触发模式
                        if (mCachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F FE 14 00 00 00 00 AB CD").toUtf8())) ||   //normal-默认值
                            mCachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F FE 14 00 00 00 01 AB CD").toUtf8()))){    //auto-定时触发
                            qDebug().noquote()<< "[" << mdetectorIndex << "] " << QString::fromUtf8("<< 反馈指令：波形触发模式");

                            findNaul = true;
                            mCachePool.remove(0, BASE_CMD_LENGTH);
                            continue;
                        }

                        // 指令返回-探测器-波形长度
                        if (mCachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F FE 15").toUtf8())) &&
                            mCachePool.mid(10, 2) == QByteArray::fromHex(QString("AB CD").toUtf8())){
                            qDebug().noquote()<< "[" << mdetectorIndex << "] " << QString::fromUtf8("<< 反馈指令：波形长度");

                            //再发送传输模式-波形
                            QTimer::singleShot(0, this, [=]{
                                QByteArray askCurrentCmd = QByteArray::fromHex(QString("12 34 00 0f fa 13 00 00 00 03 ab cd").toUtf8());
                                mSocket->write(askCurrentCmd);
                                mSocket->waitForBytesWritten();
                                qDebug().noquote()<< "[" << mdetectorIndex << "] "<<"Send HEX: "<<askCurrentCmd.toHex(' ');
                                qDebug().noquote()<< "[" << mdetectorIndex << "] " << QString::fromUtf8("<< 发送指令：传输模式-波形");
                            });

                            findNaul = true;
                            mCachePool.remove(0, BASE_CMD_LENGTH);
                            continue;
                        }

                        // 指令返回-探测器-触发模式-停止
                        if (mCachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F FF 10 11 11 00 00 AB CD").toUtf8()))){
                            qDebug().noquote()<< "[" << mdetectorIndex << "] " << QString::fromUtf8("<< 反馈指令：波形测量-停止");

                            /*通知UI，波形数据收集完毕*/
                            QMetaObject::invokeMethod(this, "measureEnd", Qt::QueuedConnection, Q_ARG(quint8, mdetectorIndex));

                            findNaul = true;
                            mCachePool.remove(0, BASE_CMD_LENGTH);
                            continue;
                        }

                        // 指令返回-探测器-触发模式-软件触发
                        if (mCachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F FF 10 11 11 00 01 AB CD").toUtf8()))){
                            qDebug().noquote()<< "[" << mdetectorIndex << "] " << QString::fromUtf8("<< 反馈指令：波形测量-开始测量-软件触发");

                            mWaveMeasuring = true;
                            findNaul = true;
                            mCachePool.remove(0, BASE_CMD_LENGTH);
                            continue;
                        }

                        // 指令返回-探测器-触发模式-硬件触发
                        if (mCachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F FF 10 11 11 00 02 AB CD").toUtf8()))){
                            qDebug().noquote()<< "[" << mdetectorIndex << "] " << QString::fromUtf8("<< 反馈指令：波形测量-开始测量-硬件触发");

                            findNaul = true;
                            mCachePool.remove(0, BASE_CMD_LENGTH);
                            continue;
                        }

                        // 指令返回-测距模块-电源闭合
                        if (mCachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F AF 10 00 00 00 01 AB CD").toUtf8()))){
                            qDebug().noquote()<< "[" << mdetectorIndex << "] " << QString::fromUtf8("<< 反馈指令：测距模块-电源闭合");

                            findNaul = true;
                            mCachePool.remove(0, BASE_CMD_LENGTH);
                            continue;
                        }

                        // 指令返回-测距模块-电源断开
                        if (mCachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F AF 10 00 00 00 00 AB CD").toUtf8()))){
                            qDebug().noquote()<< "[" << mdetectorIndex << "] " << QString::fromUtf8("<< 反馈指令：测距模块-电源断开");

                            findNaul = true;
                            mCachePool.remove(0, BASE_CMD_LENGTH);
                            continue;
                        }

                        // 指令返回-测距模块-激光打开
                        if (mCachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F AF 11 00 00 00 01 AB CD").toUtf8()))){
                            qDebug().noquote()<< "[" << mdetectorIndex << "] " << QString::fromUtf8("<< 反馈指令：测距模块-激光打开");

                            findNaul = true;
                            mCachePool.remove(0, BASE_CMD_LENGTH);
                            continue;
                        }

                        // 指令返回-测距模块-激光关闭
                        if (mCachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F AF 11 00 00 00 00 AB CD").toUtf8()))){
                            qDebug().noquote()<< "[" << mdetectorIndex << "] " << QString::fromUtf8("<< 反馈指令：测距模块-激光关闭");

                            findNaul = true;
                            mCachePool.remove(0, BASE_CMD_LENGTH);
                            continue;
                        }

                        // 指令返回-测距模块-开始单次测量
                        if (mCachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F AF 12 00 00 00 00 AB CD").toUtf8()))){
                            qDebug().noquote()<< "[" << mdetectorIndex << "] " << QString::fromUtf8("<< 反馈指令：测距模块-单次测量开始");

                            findNaul = true;
                            mCachePool.remove(0, BASE_CMD_LENGTH);
                            continue;
                        }

                        // 指令返回-测距模块-连续测量-开始
                        if (mCachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F AF 13 00 00 00 01 AB CD").toUtf8()))){
                            qDebug().noquote()<< "[" << mdetectorIndex << "] " << QString::fromUtf8("<< 反馈指令：测距模块-连续测量-开始");

                            findNaul = true;
                            mCachePool.remove(0, BASE_CMD_LENGTH);
                            continue;
                        }

                        // 指令返回-测距模块-连续测量-停止
                        if (mCachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F AF 13 00 00 00 00 AB CD").toUtf8()))){
                            qDebug().noquote()<< "[" << mdetectorIndex << "] " << QString::fromUtf8("<< 反馈指令：测距模块-连续测量-停止");

                            findNaul = true;
                            mCachePool.remove(0, BASE_CMD_LENGTH);
                            continue;
                        }

                        /*其它指令，未解析出来*/
                        {
                            qDebug().noquote()<< "[" << mdetectorIndex << "] "<<"(1) Unknown cmd: "<<mCachePool.left(BASE_CMD_LENGTH).toHex(' ');

                            findNaul = true;
                            mCachePool.remove(0, BASE_CMD_LENGTH);
                            continue;
                        }
                    }
                }
                else{
                    findNaul = true;
                    break;
                }
            }

            /*波形数据帧*/
            /*先判断包头*/
            if (mCachePool.startsWith(QByteArray::fromHex(QString("AB AB FF").toUtf8()))){
                //单个波形：0xABAB + 0xFFXY+ 波形长度*16bit +0xCDCD

                /*包头对了，再判断包长度是否满足一帧数据包长度*/
                if (mCachePool.size() >= ONE_CHANNEL_WAVE_SIZE){
                    /*包长度也够了，继续检查包尾*/
                    QByteArray chunk = mCachePool.left(ONE_CHANNEL_WAVE_SIZE);
                    if (chunk.endsWith(QByteArray::fromHex(QString("CD CD").toUtf8())) ||
                        chunk.endsWith(QByteArray::fromHex(QString("AB CD").toUtf8()))){//临时添加，后期要FPGA程序改动
                        //单个波形：0xABAB + 0xFFXY+ 波形长度*16bit +0xCDCD
                        //X:数采板序号 Y:通道号
                        quint8 no = (chunk[3] & 0xF0) >> 4;
                        no--; // 数采板序号
                        quint8 ch = chunk[3] & 0x0F;                        
                        QVector<quint16> data;

                        for (quint32 i = 0; i < this->mWaveLength * 2; i += 2) {
                            quint16 value = static_cast<quint8>(chunk[i + 4]) << 8 | static_cast<quint8>(chunk[i + 5]);
                            data.append((quint16)value);
                        }

                        mChWaveDataValidTag |= (0x01 << (ch - 1));
                        if (no==2)// 第3块采集卡，从第2通道开始计数
                        {
                            mRealCurve[no * 4 + ch] = data;
                            if (ch >= 2)
                                mRealCurve[no * 4 + ch - 1] = data;
                        }
                        else{
                            mRealCurve[no * 4 + ch] = data;
                        }
                        if (mChWaveDataValidTag == 0x0F){
                            /*4通道数据到齐了！！！*/
                            /*波形数据收集完毕，可以发送停止测量指令了*/
                            mWaveMeasuring = false;

                            /*发送停止测量指令*/
                            QTimer::singleShot(0, this, [=]{
                                if (mSocket && mSocket->state() == QAbstractSocket::ConnectedState){
                                    QByteArray askCurrentCmd = QByteArray::fromHex(QString("12 34 00 0f ff 10 11 11 00 00 ab cd").toUtf8());
                                    mSocket->write(askCurrentCmd);
                                    mSocket->waitForBytesWritten();
                                    qDebug().noquote()<< "[" << mdetectorIndex << "] "<<"Send HEX: "<<askCurrentCmd.toHex(' ');
                                }
                            });

                            // 实测曲线
                            QMetaObject::invokeMethod(this, [=]() {
                                emit showRealCurve(mRealCurve);
                            }, Qt::QueuedConnection);
                        }

                        findNaul = true;
                        mCachePool.remove(0, ONE_CHANNEL_WAVE_SIZE);
                    }
                    // else {
                    //     /*包头对了，但是包尾不正确，继续寻找包头,删除包头继续寻找*/
                    //     mCachePool.remove(0, 3);
                    // }
                }
                else {
                    findNaul = true;
                    break;
                    //continue;
                }
            }

            if (!findNaul){
                // 循环一圈下来都没发现完整数据帧，删除包头前1个字节

                /*万一只有头，尾还没收到了，这里先不着急删除，还需要进一步判断！！！！*/
                mCachePool.remove(0, 1);

                /*继续寻找下一轮*/
                continue;
            }

            /*缓存池数据都处理完了*/
            if (mCachePool.size() == 0){
                break;
            }
        }
    }
}
