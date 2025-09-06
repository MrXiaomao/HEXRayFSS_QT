#include "dataprocessor.h"
#include <QDebug>
#include <QTimer>

DataProcessor::DataProcessor(quint8 index, QTcpSocket* socket, QObject *parent)
    : QObject{parent}
    , detectorIndex(index)
    , mSocket(socket)
{
    dataProcessThread = new QLiteThread(this);
    dataProcessThread->setObjectName("dataProcessThread");
    dataProcessThread->setWorkThreadProc([=](){
        OnDataProcessThread();
    });
    dataProcessThread->start();
    connect(this, &DataProcessor::destroyed, [=]() {
        dataProcessThread->exit(0);
        dataProcessThread->wait(500);
    });

    connect(mSocket, SIGNAL(readyRead()), this, SLOT(socketReadyRead()));
}

DataProcessor::~DataProcessor()
{
    // 终止线程
    mTerminatedThead = true;
    mDataReady = true;
    mCondition.wakeAll();
    dataProcessThread->wait();
}

void DataProcessor::socketReadyRead()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (socket->bytesAvailable() <= 0)
        return;

    QByteArray rawData = socket->readAll();
    this->inputData(rawData);
}

void DataProcessor::inputData(const QByteArray& data)
{
    {
        QMutexLocker locker(&mDataLocker);
        rawData.append(data);
        qDebug()<< "(" << detectorIndex << ") inputData: " << data.toHex(' ') << " >> " << rawData.toHex(' ');

        mDataReady = true;
    }

    mCondition.wakeAll();
}

void DataProcessor::OnDataProcessThread()
{
    static const int BASE_CMD_LENGTH = 12;
    static const int MIN_CMD_LENGTH = 6; //温度指令长度
    static const int ONE_CHANNEL_WAVE_SIZE = 1030;// 1030=(512+3)*2 单通道数据长度

    QByteArray waveDataHead = QByteArray::fromHex(QString("AB AB FF").toUtf8());;// 数据头
    QByteArray waveDataTail = QByteArray::fromHex(QString("CD CD").toUtf8());;// 数据尾

    while (!mTerminatedThead)
    {
        {
            QMutexLocker locker(&mDataLocker);
            if (rawData.size() == 0){
                while (!mDataReady){
                    mCondition.wait(&mDataLocker);
                }

                cachePool.append(rawData);
                rawData.clear();
                mDataReady = false;
            }
            else{
                cachePool.append(rawData);
                rawData.clear();
            }
        }

        if (cachePool.size() < MIN_CMD_LENGTH)
            continue;

        while (true){
            /*是否找到完整帧（命令帧/数据帧）*/
            bool findNaul = false;

            if (waveMeasuring){
                // 波形测量已经开始了！！！

                //单个波形：0xABAB + 0xFFXY+ 波形长度*16bit +0xCDCD
                if (cachePool.size() > 0){
                    rawWaveData.append(cachePool);
                    cachePool.clear();
                }

                /*
                 处理波形数据
                */

                while (rawWaveData.size() >= ONE_CHANNEL_WAVE_SIZE){
                    // 前面可能还存在有12字节的开始测量反馈指令，所以还需要进一步检查一下

                    if (rawWaveData.startsWith(waveDataHead)){
                        // 指令包

                        //继续检查包尾
                        QByteArray chunk = rawWaveData.left(ONE_CHANNEL_WAVE_SIZE);
                        if (chunk.endsWith(waveDataTail)){
                            //单个波形：0xABAB + 0xFFXY+ 波形长度*16bit +0xCDCD
                            //X:数采板序号 Y:通道号
                            quint8 ch = chunk[3] & 0x0F;
                            QVector<quint16> data;

                            for (int i = 0; i < this->mWaveLength * 2; i += 2) {
                                quint16 value = static_cast<quint8>(chunk[i + 4]) << 8 | static_cast<quint8>(chunk[i + 5]);
                                data.append(value);
                            }

                            mChWaveDataValidTag |= (0x01 << (ch - 1));
                            mRealCurve[ch] = data;

                            rawWaveData.remove(0, ONE_CHANNEL_WAVE_SIZE);
                        }
                        else {
                            // 包尾不正确，继续寻找包头
                            rawWaveData.remove(0, 3);
                        }
                    }
                    else{
                        // 包头不正确，继续寻找包头
                        rawWaveData.remove(0, 1);
                    }
                }

                if (mChWaveDataValidTag == 0x0F){
                    // 4路通道数据采集完毕
                    // 实测曲线
                    QMetaObject::invokeMethod(this, [=]() {
                        emit showRealCurve(mRealCurve);
                    }, Qt::QueuedConnection);
                }

                break;
            }
            else{
                // 特殊指令集判断
                // 0xAABB + 16bit（温度） + 0xCCDD/0xABCD
            reTry:
                if (cachePool.startsWith(QByteArray::fromHex(QString("AA BB").toUtf8())) &&
                    (cachePool.mid(4, 2) != QByteArray::fromHex(QString("CC DD").toUtf8()) ||
                     cachePool.mid(4, 2) != QByteArray::fromHex(QString("AB CD").toUtf8()))){
                    //0xAABB + 16bit（温度） + 0xCCDD
                    QByteArray data = cachePool.mid(2, 2);
                    qint16 t = qFromBigEndian<qint16>(data.constData());
                    float temperature = t * 0.0078125;// 换算系数

                    //qDebug()<<"Temperature "<< detectorIndex << ": " << temperature;
                    emit temperatureRespond(detectorIndex, temperature);

                    cachePool.remove(0, 6);
                    continue;
                }

                // 0xACAC + 48bit（版本号） + 0xEFEF/0xABCD
                if (cachePool.startsWith(QByteArray::fromHex(QString("AC AC").toUtf8())) &&
                    (cachePool.mid(8, 2) == QByteArray::fromHex(QString("EF EF").toUtf8()) ||
                     cachePool.mid(8, 2) == QByteArray::fromHex(QString("AB CD").toUtf8()))){
                    // 0xACAC + 48bit（版本号） + 0xEFEF
                    QByteArray year = cachePool.mid(2, 2);
                    QByteArray month = cachePool.mid(4, 1);
                    QByteArray day = cachePool.mid(5, 1);
                    QByteArray serialNumber = cachePool.mid(6, 2);

                    QString version = year.toHex().toUpper() +
                                      month.toHex().toUpper() +
                                      day.toHex().toUpper();

                    //qDebug()<<"AppVersion "<< detectorIndex<< ": "<< version<< ", serialNumber: " << serialNumber.toHex().toUpper();
                    emit appVersionRespond(detectorIndex, version, QString(serialNumber.toHex().toUpper()));

                    cachePool.remove(0, 10);
                    continue;
                }

                if (cachePool.size() == 0)
                    break;

                // 指令起始字节是12 34 00 0F/AB，尾字节是AB CD                
                while (cachePool.size() >= MIN_CMD_LENGTH){
                    if (cachePool.startsWith(QByteArray::fromHex(QString("AA BB").toUtf8())) &&
                        (cachePool.mid(4, 2) == QByteArray::fromHex(QString("CC DD").toUtf8()) ||
                         cachePool.mid(4, 2) == QByteArray::fromHex(QString("AB CD").toUtf8()))){
                        findNaul = true;
                        goto reTry;
                    }

                    if (cachePool.size() >= 10){//版本指令长度
                        if (cachePool.startsWith(QByteArray::fromHex(QString("AC AC").toUtf8())) &&
                            (cachePool.mid(8, 2) == QByteArray::fromHex(QString("EF EF").toUtf8()) ||
                             cachePool.mid(8, 2) == QByteArray::fromHex(QString("AB CD").toUtf8()))){
                            findNaul = true;
                            goto reTry;
                        }
                    }

                    if (cachePool.size() >= BASE_CMD_LENGTH){
                        if (cachePool.startsWith(QByteArray::fromHex(QString("12 34 00").toUtf8())) &&
                            cachePool.mid(10, 2) == QByteArray::fromHex(QString("AB CD").toUtf8())){
                            findNaul = true;
                            break;
                        }
                        else{
                            qDebug()<< "(" << detectorIndex << ") can't find head!" << cachePool.toHex(' ');
                            cachePool.remove(0, 1);
                            continue;
                        }
                    }
                    else{
                        break;
                    }
                }

                if (cachePool.size() < BASE_CMD_LENGTH){
                    qDebug()<< "(" << detectorIndex << ") data length isn't full!" << cachePool.toHex(' ');
                    return;
                }

                if (distanceMeasuring){
                    // 指令返回-距离测量
                    if ((cachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F AF 12").toUtf8())) ||  //单次
                         cachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F AF 13").toUtf8()))) &&  //连续
                        cachePool.mid(10, 2) != QByteArray::fromHex(QString("AB CD").toUtf8())){
                        cachePool.remove(0, BASE_CMD_LENGTH);
                        continue;
                    }

                    // 数据返回-测量距离
                    if (cachePool.startsWith(QByteArray::fromHex(QString("12 34 00 AB").toUtf8())) &&
                        cachePool.mid(10, 2) != QByteArray::fromHex(QString("AB CD").toUtf8())){
                        QByteArray data = cachePool.mid(4, 4);
                        QString string = QString::fromUtf8(data);
                        string.insert(5, '.');
                        float distance = string.toFloat();

                        data = cachePool.mid(8, 2);
                        bool ok = false;
                        quint16 quality = data.toShort(&ok, 16);

                        qDebug()<<"Distance: "<< distance << ", quality: " << quality;
                        emit distanceRespond(distance, quality);

                        cachePool.remove(0, BASE_CMD_LENGTH);
                        if (singleMeasure){
                            distanceMeasuring = false;
                        }

                        continue;
                    }
                    else{
                        qDebug()<<"(1) Unknown cmd: "<<cachePool.toHex(' ');
                        cachePool.remove(0, BASE_CMD_LENGTH);
                        continue;
                    }
                }
                else {
                    // 硬件触发反馈
                    if (cachePool.startsWith(QByteArray::fromHex(QString("12 34 00 AA 00 0C 00 00 00 00 AB CD").toUtf8()))){
                        waveMeasuring = true;
                        cachePool.remove(0, BASE_CMD_LENGTH);
                        qDebug() << QString::fromLocal8Bit("<< 反馈指令：硬件触发");

                        continue;
                    }

                    // 指令返回-探测器-传输模式-程序版本号
                    if (cachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F FA 13 00 00 00 01 AB CD").toUtf8()))){
                        cachePool.remove(0, BASE_CMD_LENGTH);
                        qDebug() << QString::fromLocal8Bit("<< 反馈指令：传输模式-程序版本号");

                        // 再发送查询指令
                        QTimer::singleShot(0, this, [=]{
                            QByteArray askCurrentCmd = QByteArray::fromHex(QString("12 34 00 0f fc 11 00 00 00 00 ab cd").toUtf8());
                            mSocket->write(askCurrentCmd);
                            qDebug() << QString::fromLocal8Bit(">> 发送指令：传输模式-查询程序版本号");
                        });

                        continue;
                    }

                    // 指令返回-探测器-传输模式-波形
                    if (cachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F FA 13 00 00 00 03 AB CD").toUtf8()))){
                        cachePool.remove(0, BASE_CMD_LENGTH);
                        qDebug() << QString::fromLocal8Bit("<< 反馈指令：传输模式-波形");

                        emit measureStart();

                        //最后发送开始测量-软件触发模式
                        QTimer::singleShot(0, this, [=]{
                            QByteArray askCurrentCmd = QByteArray::fromHex(QString("12 34 00 0f ff 10 11 11 00 01 ab cd").toUtf8());
                            //askCurrentCmd[9] = mTransferMode;
                            mSocket->write(askCurrentCmd);
                            qDebug() << QString::fromLocal8Bit(">> 发送指令：传输模式-开始波形测量");
                        });

                        continue;
                    }

                    // 指令返回-探测器-传输模式-温度
                    if (cachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F FA 13 00 00 00 05 AB CD").toUtf8()))){
                        cachePool.remove(0, BASE_CMD_LENGTH);
                        qDebug() << QString::fromLocal8Bit("<< 反馈指令：传输模式-温度");

                        continue;
                    }

                    // 指令返回-探测器-传输模式-激光测距
                    if (cachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F FA 13 00 00 00 09 AB CD").toUtf8()))){
                        cachePool.remove(0, BASE_CMD_LENGTH);
                        qDebug() << QString::fromLocal8Bit("<< 反馈指令：传输模式-激光测距");

                        continue;
                    }

                    // 指令返回-探测器-程控增益
                    if (cachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F FB 11").toUtf8())) &&
                        cachePool.mid(10, 2) == QByteArray::fromHex(QString("AB CD").toUtf8())){
                        cachePool.remove(0, BASE_CMD_LENGTH);
                        qDebug() << QString::fromLocal8Bit("<< 反馈指令：程控增益");

                        continue;
                    }

                    // 指令返回-探测器-程序版本查询
                    if (cachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F FC 11 00 00 00 00 AB CD").toUtf8()))){
                        cachePool.remove(0, BASE_CMD_LENGTH);
                        qDebug() << QString::fromLocal8Bit("<< 反馈指令：程序版本查询");

                        continue;
                    }

                    // 指令返回-探测器-温度查询-开始
                    if (cachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F FC 12 00 00 00 01 AB CD").toUtf8()))){
                        cachePool.remove(0, BASE_CMD_LENGTH);
                        qDebug() << QString::fromLocal8Bit("<< 反馈指令：温度查询-开始");

                        continue;
                    }

                    // 指令返回-探测器-温度查询-停止
                    if (cachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F FC 12 00 00 00 00 AB CD").toUtf8()))){
                        cachePool.remove(0, BASE_CMD_LENGTH);
                        qDebug() << QString::fromLocal8Bit("<< 反馈指令：温度查询-停止");

                        continue;
                    }

                    // 指令返回-探测器-触发阈值
                    if ((cachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F FE 11").toUtf8())) ||
                         cachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F FE 12").toUtf8()))) &&
                        cachePool.mid(10, 2) == QByteArray::fromHex(QString("AB CD").toUtf8())){
                        cachePool.remove(0, BASE_CMD_LENGTH);
                        qDebug() << QString::fromLocal8Bit("<< 反馈指令：触发阈值");

                        continue;
                    }

                    // 指令返回-探测器-波形触发模式
                    if (cachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F FE 14").toUtf8())) &&
                        cachePool.mid(10, 2) == QByteArray::fromHex(QString("AB CD").toUtf8())){
                        cachePool.remove(0, BASE_CMD_LENGTH);
                        qDebug() << QString::fromLocal8Bit("<< 反馈指令：波形触发模式");

                        continue;
                    }

                    // 指令返回-探测器-波形长度
                    if (cachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F FE 15").toUtf8())) &&
                        cachePool.mid(10, 2) == QByteArray::fromHex(QString("AB CD").toUtf8())){
                        cachePool.remove(0, BASE_CMD_LENGTH);
                        qDebug() << QString::fromLocal8Bit("<< 反馈指令：波形长度");

                        //再发送传输模式-波形
                        QTimer::singleShot(0, this, [=]{
                            QByteArray askCurrentCmd = QByteArray::fromHex(QString("12 34 00 0f fa 13 00 00 00 03 ab cd").toUtf8());
                            mSocket->write(askCurrentCmd);
                            qDebug() << QString::fromLocal8Bit("<< 发送指令：传输模式-波形");
                        });

                        continue;
                    }

                    // 指令返回-探测器-触发模式-停止
                    if (cachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F FF 10 11 11 00 00 AB CD").toUtf8()))){
                        cachePool.remove(0, BASE_CMD_LENGTH);
                        qDebug() << QString::fromLocal8Bit("<< 反馈指令：波形测量-停止");

                        continue;
                    }

                    // 指令返回-探测器-触发模式-软件触发
                    if (cachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F FF 10 11 11 00 01 AB CD").toUtf8()))){
                        cachePool.remove(0, BASE_CMD_LENGTH);
                        qDebug() << QString::fromLocal8Bit("<< 反馈指令：波形测量-开始测量-软件触发");

                        waveMeasuring = true;
                        continue;
                    }

                    // 指令返回-探测器-触发模式-硬件触发
                    if (cachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F FF 10 11 11 00 02 AB CD").toUtf8()))){
                        cachePool.remove(0, BASE_CMD_LENGTH);
                        qDebug() << QString::fromLocal8Bit("<< 反馈指令：波形测量-开始测量-硬件触发");

                        continue;
                    }

                    // 指令返回-测距模块-电源闭合
                    if (cachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F AF 10 00 00 00 01 AB CD").toUtf8()))){
                        cachePool.remove(0, BASE_CMD_LENGTH);
                        qDebug() << QString::fromLocal8Bit("<< 反馈指令：测距模块-电源闭合");

                        continue;
                    }

                    // 指令返回-测距模块-电源断开
                    if (cachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F AF 10 00 00 00 00 AB CD").toUtf8()))){
                        cachePool.remove(0, BASE_CMD_LENGTH);
                        qDebug() << QString::fromLocal8Bit("<< 反馈指令：测距模块-电源断开");

                        continue;
                    }

                    // 指令返回-测距模块-激光打开
                    if (cachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F AF 11 00 00 00 01 AB CD").toUtf8()))){
                        cachePool.remove(0, BASE_CMD_LENGTH);
                        qDebug() << QString::fromLocal8Bit("<< 反馈指令：测距模块-激光打开");

                        continue;
                    }

                    // 指令返回-测距模块-激光关闭
                    if (cachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F AF 11 00 00 00 00 AB CD").toUtf8()))){
                        cachePool.remove(0, BASE_CMD_LENGTH);
                        qDebug() << QString::fromLocal8Bit("<< 反馈指令：测距模块-激光关闭");

                        continue;
                    }

                    // 指令返回-测距模块-开始单次测量
                    if (cachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F AF 12 00 00 00 00 AB CD").toUtf8()))){
                        cachePool.remove(0, BASE_CMD_LENGTH);
                        qDebug() << QString::fromLocal8Bit("<< 反馈指令：测距模块-单次测量开始");

                        continue;
                    }

                    // 指令返回-测距模块-连续测量-开始
                    if (cachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F AF 13 00 00 00 01 AB CD").toUtf8()))){
                        cachePool.remove(0, BASE_CMD_LENGTH);
                        qDebug() << QString::fromLocal8Bit("<< 反馈指令：测距模块-连续测量-开始");

                        continue;
                    }

                    // 指令返回-测距模块-连续测量-停止
                    if (cachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F AF 13 00 00 00 00 AB CD").toUtf8()))){
                        cachePool.remove(0, BASE_CMD_LENGTH);
                        qDebug() << QString::fromLocal8Bit("<< 反馈指令：测距模块-连续测量-停止");

                        continue;
                    }

                    if (cachePool.size() != 0)
                    {
                        qDebug()<<"(2) Unknown cmd: "<<cachePool.toHex(' ');
                        cachePool.remove(0, BASE_CMD_LENGTH);
                        continue;
                    }
                    else{
                        break;
                    }
                }
            }
        }
    }
}
