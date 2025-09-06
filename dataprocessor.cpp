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

    QString strTime = QDateTime::currentDateTime().toString("yyyy-MM-dd_HHmmss");
    QString filePath = QString("%1%2_%3.dat").arg("./cache/").arg(detectorIndex).arg(strTime);
    mpfSaveNet = new QFile(filePath);
    if (mpfSaveNet->open(QIODevice::WriteOnly)) {
        qDebug().noquote() << tr("创建网口数据缓存文件成功，文件名：%1").arg(filePath);
    } else {
        qDebug().noquote() << tr("创建网口数据缓存文件失败，文件名：%1").arg(filePath);
    }
}

DataProcessor::~DataProcessor()
{
    // 终止线程
    mTerminatedThead = true;
    mDataReady = true;
    mCondition.wakeAll();
    dataProcessThread->wait();

    if (mpfSaveNet){
        mpfSaveNet->close();
        mpfSaveNet->deleteLater();
        mpfSaveNet = nullptr;
    }
}

void DataProcessor::socketReadyRead()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (socket->bytesAvailable() <= 0)
        return;

    QByteArray rawData = socket->readAll();
    if (mpfSaveNet){
        mpfSaveNet->write((const char *)rawData.constData(), rawData.size());
        mpfSaveNet->flush();
    }
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
    static const int TEMPERATURE_CMD_LENGTH = 6; //温度指令长度
    static const int VERSION_CMD_LENGTH = 10; //版本号指令长度
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

        if (cachePool.size() < TEMPERATURE_CMD_LENGTH)
            continue;

        while (true){
reTry:
            if (cachePool.size() < TEMPERATURE_CMD_LENGTH)
                break;

            /*是否找到完整帧（命令帧/数据帧）*/
            bool findNaul = false;

            // 数据长度从小到大依次判断
            // 温度(6)< 程序版本号(10) < 指令(12) < 波形数据(1030)

            /*温度*/
            // 0xAABB + 16bit（温度） + 0xCCDD/0xABCD
            if (cachePool.startsWith(QByteArray::fromHex(QString("AA BB").toUtf8()))){
                if (cachePool.size() >= TEMPERATURE_CMD_LENGTH){
                    if (cachePool.mid(4, 2) == QByteArray::fromHex(QString("CC DD").toUtf8()) ||
                        cachePool.mid(4, 2) == QByteArray::fromHex(QString("AB CD").toUtf8())){
                        QByteArray data = cachePool.mid(2, 2);
                        qint16 t = qFromBigEndian<qint16>(data.constData());
                        float temperature = t * 0.0078125;// 换算系数

                        qDebug()<<"Temperature "<< detectorIndex << ": " << temperature;
                        emit temperatureRespond(detectorIndex, temperature);

                        findNaul = true;
                        cachePool.remove(0, TEMPERATURE_CMD_LENGTH);
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
            if (cachePool.startsWith(QByteArray::fromHex(QString("AC AC").toUtf8()))){
                if (cachePool.size() >= VERSION_CMD_LENGTH){
                    if (cachePool.mid(8, 2) == QByteArray::fromHex(QString("EF EF").toUtf8()) ||
                        cachePool.mid(8, 2) == QByteArray::fromHex(QString("AB CD").toUtf8())){
                        QByteArray year = cachePool.mid(2, 2);
                        QByteArray month = cachePool.mid(4, 1);
                        QByteArray day = cachePool.mid(5, 1);
                        QByteArray serialNumber = cachePool.mid(6, 2);

                        QString version = year.toHex().toUpper() +
                                          month.toHex().toUpper() +
                                          day.toHex().toUpper();

                        qDebug()<<"AppVersion "<< detectorIndex<< ": "<< version<< ", serialNumber: " << serialNumber.toHex().toUpper();
                        emit appVersionRespond(detectorIndex, version, QString(serialNumber.toHex().toUpper()));

                        findNaul = true;
                        cachePool.remove(0, VERSION_CMD_LENGTH);
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
            if (cachePool.startsWith(QByteArray::fromHex(QString("12 34 00").toUtf8()))){
                if (cachePool.size() >= BASE_CMD_LENGTH){
                    if (cachePool.startsWith(QByteArray::fromHex(QString("12 34 00").toUtf8())) &&
                        cachePool.mid(10, 2) == QByteArray::fromHex(QString("AB CD").toUtf8())){
                        /*找到指令帧了！！！*/
                        /*继续判断帧类型*/

                        // 指令返回-距离测量
                        if (cachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F AF 12 00 00 00 00 AB CD").toUtf8())) ||   //单次
                             cachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F AF 13 00 00 00 00 Ab CD").toUtf8())) ||  //连续-关闭
                             cachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F AF 13 00 00 00 01 Ab CD").toUtf8()))){   //连续-打开
                            findNaul = true;
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

                            if (singleMeasure){
                                distanceMeasuring = false;
                            }

                            findNaul = true;
                            cachePool.remove(0, BASE_CMD_LENGTH);
                            continue;
                        }


                        // 硬件触发反馈
                        if (cachePool.startsWith(QByteArray::fromHex(QString("12 34 00 AA 00 0C 00 00 00 00 AB CD").toUtf8()))){
                            qDebug() << QString::fromLocal8Bit("<< 反馈指令：硬件触发");

                            findNaul = true;
                            cachePool.remove(0, BASE_CMD_LENGTH);
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
                                qDebug()<<"Send HEX: "<<askCurrentCmd.toHex(' ');
                                qDebug() << QString::fromLocal8Bit(">> 发送指令：传输模式-查询程序版本号");
                            });

                            findNaul = true;
                            cachePool.remove(0, BASE_CMD_LENGTH);
                            continue;
                        }

                        // 指令返回-探测器-传输模式-波形
                        if (cachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F FA 13 00 00 00 03 AB CD").toUtf8()))){
                            qDebug() << QString::fromLocal8Bit("<< 反馈指令：传输模式-波形");

                            emit measureStart();

                            //最后发送开始测量-软件触发模式
                            QTimer::singleShot(0, this, [=]{
                                QByteArray askCurrentCmd = QByteArray::fromHex(QString("12 34 00 0f ff 10 11 11 00 01 ab cd").toUtf8());
                                //askCurrentCmd[9] = mTransferMode;
                                mSocket->write(askCurrentCmd);
                                qDebug()<<"Send HEX: "<<askCurrentCmd.toHex(' ');
                                qDebug() << QString::fromLocal8Bit(">> 发送指令：传输模式-开始波形测量");
                            });

                            findNaul = true;
                            cachePool.remove(0, BASE_CMD_LENGTH);
                            continue;
                        }

                        // 指令返回-探测器-传输模式-温度
                        if (cachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F FA 13 00 00 00 05 AB CD").toUtf8()))){
                            qDebug() << QString::fromLocal8Bit("<< 反馈指令：传输模式-温度");

                            findNaul = true;
                            cachePool.remove(0, BASE_CMD_LENGTH);
                            continue;
                        }

                        // 指令返回-探测器-传输模式-激光测距
                        if (cachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F FA 13 00 00 00 09 AB CD").toUtf8()))){
                            qDebug() << QString::fromLocal8Bit("<< 反馈指令：传输模式-激光测距");

                            findNaul = true;
                            cachePool.remove(0, BASE_CMD_LENGTH);
                            continue;
                        }

                        // 指令返回-探测器-程控增益
                        if (cachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F FB 11").toUtf8())) &&
                            cachePool.mid(10, 2) == QByteArray::fromHex(QString("AB CD").toUtf8())){
                            qDebug() << QString::fromLocal8Bit("<< 反馈指令：程控增益");

                            findNaul = true;
                            cachePool.remove(0, BASE_CMD_LENGTH);
                            continue;
                        }

                        // 指令返回-探测器-程序版本查询
                        if (cachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F FC 11 00 00 00 00 AB CD").toUtf8()))){
                            qDebug() << QString::fromLocal8Bit("<< 反馈指令：程序版本查询");

                            findNaul = true;
                            cachePool.remove(0, BASE_CMD_LENGTH);
                            continue;
                        }

                        // 指令返回-探测器-温度查询-开始
                        if (cachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F FC 12 00 00 00 01 AB CD").toUtf8()))){
                            qDebug() << QString::fromLocal8Bit("<< 反馈指令：温度查询-开始");

                            findNaul = true;
                            cachePool.remove(0, BASE_CMD_LENGTH);
                            continue;
                        }

                        // 指令返回-探测器-温度查询-停止
                        if (cachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F FC 12 00 00 00 00 AB CD").toUtf8()))){
                            qDebug() << QString::fromLocal8Bit("<< 反馈指令：温度查询-停止");

                            findNaul = true;
                            cachePool.remove(0, BASE_CMD_LENGTH);
                            continue;
                        }

                        // 指令返回-探测器-触发阈值
                        if ((cachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F FE 11").toUtf8())) ||
                             cachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F FE 12").toUtf8()))) &&
                            cachePool.mid(10, 2) == QByteArray::fromHex(QString("AB CD").toUtf8())){
                            qDebug() << QString::fromLocal8Bit("<< 反馈指令：触发阈值");

                            findNaul = true;
                            cachePool.remove(0, BASE_CMD_LENGTH);
                            continue;
                        }

                        // 指令返回-探测器-波形触发模式
                        if (cachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F FE 14 00 00 00 00 AB CD").toUtf8())) ||   //normal-默认值
                            cachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F FE 14 00 00 00 01 AB CD").toUtf8()))){    //auto-定时触发
                            qDebug() << QString::fromLocal8Bit("<< 反馈指令：波形触发模式");

                            findNaul = true;
                            cachePool.remove(0, BASE_CMD_LENGTH);
                            continue;
                        }

                        // 指令返回-探测器-波形长度
                        if (cachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F FE 15").toUtf8())) &&
                            cachePool.mid(10, 2) == QByteArray::fromHex(QString("AB CD").toUtf8())){
                            qDebug() << QString::fromLocal8Bit("<< 反馈指令：波形长度");

                            //再发送传输模式-波形
                            QTimer::singleShot(0, this, [=]{
                                QByteArray askCurrentCmd = QByteArray::fromHex(QString("12 34 00 0f fa 13 00 00 00 03 ab cd").toUtf8());
                                mSocket->write(askCurrentCmd);
                                qDebug()<<"Send HEX: "<<askCurrentCmd.toHex(' ');
                                qDebug() << QString::fromLocal8Bit("<< 发送指令：传输模式-波形");
                            });

                            findNaul = true;
                            cachePool.remove(0, BASE_CMD_LENGTH);
                            continue;
                        }

                        // 指令返回-探测器-触发模式-停止
                        if (cachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F FF 10 11 11 00 00 AB CD").toUtf8()))){
                            qDebug() << QString::fromLocal8Bit("<< 反馈指令：波形测量-停止");

                            findNaul = true;
                            cachePool.remove(0, BASE_CMD_LENGTH);
                            continue;
                        }

                        // 指令返回-探测器-触发模式-软件触发
                        if (cachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F FF 10 11 11 00 01 AB CD").toUtf8()))){
                            qDebug() << QString::fromLocal8Bit("<< 反馈指令：波形测量-开始测量-软件触发");

                            waveMeasuring = true;
                            findNaul = true;
                            cachePool.remove(0, BASE_CMD_LENGTH);
                            continue;
                        }

                        // 指令返回-探测器-触发模式-硬件触发
                        if (cachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F FF 10 11 11 00 02 AB CD").toUtf8()))){
                            qDebug() << QString::fromLocal8Bit("<< 反馈指令：波形测量-开始测量-硬件触发");

                            findNaul = true;
                            cachePool.remove(0, BASE_CMD_LENGTH);
                            continue;
                        }

                        // 指令返回-测距模块-电源闭合
                        if (cachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F AF 10 00 00 00 01 AB CD").toUtf8()))){
                            qDebug() << QString::fromLocal8Bit("<< 反馈指令：测距模块-电源闭合");

                            findNaul = true;
                            cachePool.remove(0, BASE_CMD_LENGTH);
                            continue;
                        }

                        // 指令返回-测距模块-电源断开
                        if (cachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F AF 10 00 00 00 00 AB CD").toUtf8()))){
                            qDebug() << QString::fromLocal8Bit("<< 反馈指令：测距模块-电源断开");

                            findNaul = true;
                            cachePool.remove(0, BASE_CMD_LENGTH);
                            continue;
                        }

                        // 指令返回-测距模块-激光打开
                        if (cachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F AF 11 00 00 00 01 AB CD").toUtf8()))){
                            qDebug() << QString::fromLocal8Bit("<< 反馈指令：测距模块-激光打开");

                            findNaul = true;
                            cachePool.remove(0, BASE_CMD_LENGTH);
                            continue;
                        }

                        // 指令返回-测距模块-激光关闭
                        if (cachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F AF 11 00 00 00 00 AB CD").toUtf8()))){
                            qDebug() << QString::fromLocal8Bit("<< 反馈指令：测距模块-激光关闭");

                            findNaul = true;
                            cachePool.remove(0, BASE_CMD_LENGTH);
                            continue;
                        }

                        // 指令返回-测距模块-开始单次测量
                        if (cachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F AF 12 00 00 00 00 AB CD").toUtf8()))){
                            qDebug() << QString::fromLocal8Bit("<< 反馈指令：测距模块-单次测量开始");

                            findNaul = true;
                            cachePool.remove(0, BASE_CMD_LENGTH);
                            continue;
                        }

                        // 指令返回-测距模块-连续测量-开始
                        if (cachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F AF 13 00 00 00 01 AB CD").toUtf8()))){
                            qDebug() << QString::fromLocal8Bit("<< 反馈指令：测距模块-连续测量-开始");

                            findNaul = true;
                            cachePool.remove(0, BASE_CMD_LENGTH);
                            continue;
                        }

                        // 指令返回-测距模块-连续测量-停止
                        if (cachePool.startsWith(QByteArray::fromHex(QString("12 34 00 0F AF 13 00 00 00 00 AB CD").toUtf8()))){
                            qDebug() << QString::fromLocal8Bit("<< 反馈指令：测距模块-连续测量-停止");

                            findNaul = true;
                            cachePool.remove(0, BASE_CMD_LENGTH);
                            continue;
                        }

                        /*其它指令，未解析出来*/
                        {
                            qDebug()<<"(1) Unknown cmd: "<<cachePool.left(BASE_CMD_LENGTH).toHex(' ');

                            findNaul = true;
                            cachePool.remove(0, BASE_CMD_LENGTH);
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
            if (cachePool.startsWith(QByteArray::fromHex(QString("AB AB FF").toUtf8()))){
                //单个波形：0xABAB + 0xFFXY+ 波形长度*16bit +0xCDCD

                /*包头对了，再判断包长度是否满足一帧数据包长度*/
                if (cachePool.size() >= ONE_CHANNEL_WAVE_SIZE){
                    /*包长度也够了，继续检查包尾*/
                    QByteArray chunk = cachePool.left(ONE_CHANNEL_WAVE_SIZE);
                    if (chunk.endsWith(QByteArray::fromHex(QString("CD CD").toUtf8()))){
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

                        if (mChWaveDataValidTag == 0x0F){
                            /*4通道数据到齐了！！！*/
                            /*波形数据收集完毕，可以发送停止测量指令了*/
                            waveMeasuring = false;

                            /*发送停止测量指令*/
                            QTimer::singleShot(0, this, [=]{
                                if (mSocket && mSocket->state() == QAbstractSocket::ConnectedState){
                                    QByteArray askCurrentCmd = QByteArray::fromHex(QString("12 34 00 0f ff 10 11 11 00 00 ab cd").toUtf8());
                                    mSocket->write(askCurrentCmd);
                                    qDebug()<<"Send HEX: "<<askCurrentCmd.toHex(' ');
                                }
                            });

                            /*通知UI，波形数据收集完毕*/
                            QMetaObject::invokeMethod(this, "measureStop", Qt::QueuedConnection);

                            // 实测曲线
                            QMetaObject::invokeMethod(this, [=]() {
                                emit showRealCurve(mRealCurve);
                            }, Qt::QueuedConnection);
                        }

                        findNaul = true;
                        cachePool.remove(0, ONE_CHANNEL_WAVE_SIZE);
                    }
                    // else {
                    //     /*包头对了，但是包尾不正确，继续寻找包头,删除包头继续寻找*/
                    //     cachePool.remove(0, 3);
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
                cachePool.remove(0, 1);

                /*继续寻找下一轮*/
                continue;
            }

            /*缓存池数据都处理完了*/
            if (cachePool.size() == 0){
                break;
            }
        }
    }
}
