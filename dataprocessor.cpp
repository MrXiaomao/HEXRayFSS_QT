#include "dataprocessor.h"
#include <QDebug>
#include <QTimer>

DataProcessor::DataProcessor(QTcpSocket* socket, QObject *parent)
    : QObject{parent}
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

        mDataReady = true;
    }

    mCondition.wakeAll();
}

void DataProcessor::OnDataProcessThread()
{
    static const int BASE_CMD_LENGTH = 4;
    static const int ONE_CHANNEL_WAVE_SIZE = 1030;// 1030=(512+3)*2 单通道数据长度
    static const int ONE_PACK_SIZE = 33024; //一次完整数据包大小33024=((1024+8)*2*16)
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

        if (mCachePool.size() < BASE_CMD_LENGTH)
            continue;

        while (true){
            if (mCachePool.size() < BASE_CMD_LENGTH)
                break;

            /*是否找到完整帧（命令帧/数据帧）*/
            bool findNaul = false;

            // 数据长度从小到大依次判断
            if (!mWaveMeasuring){
                // 测量未进入到波形数据接收阶段
                if (askCurrentCmd.at(1) == 0x21) {
                    // 初始化
                    if (mCachePool.at(1) == 0x31) {
                        qInfo().noquote() << "初始化指令响应成功";
                        QMetaObject::invokeMethod(this, "initSuccess", Qt::QueuedConnection);
                    }
                    else {
                        qCritical().noquote() << "初始化指令响应失败";
                    }

                    // 处理剩下数据（粘包处理）
                    mCachePool.remove(0, BASE_CMD_LENGTH);
                    continue;
                }
                else if (askCurrentCmd.at(1) == 0x22 || askCurrentCmd.at(1) == 0x28 || askCurrentCmd.at(1) == 0x29) {
                    // 启动
                    if (mCachePool.at(1) == 0x32 || mCachePool.at(1) == 0x38 || mCachePool.at(1) == 0x39) {
                        mWaveMeasuring = true;
                        qInfo().noquote() << "等待触发...";
                        QMetaObject::invokeMethod(this, "waitTriggerSignal", Qt::QueuedConnection);
                    }
                    else {
                        qCritical().noquote() << "开始采集指令响应失败";
                    }

                    // 处理剩下数据（粘包处理）
                    mCachePool.remove(0, BASE_CMD_LENGTH);
                    continue;
                }
                else if (askCurrentCmd.at(1) == 0x23) {
                    // 停止
                    if (mCachePool.at(1) == 0x33) {
                        mWaveMeasuring = false;
                        qInfo().noquote() << "停止采集";
                        QMetaObject::invokeMethod(this, "measureEnd", Qt::QueuedConnection);
                     }
                    else {
                        qCritical().noquote() << "停止采集指令响应失败";
                    }

                     // 处理剩下数据（粘包处理）
                     mCachePool.remove(0, BASE_CMD_LENGTH);
                     continue;
                }
                else if (askCurrentCmd.at(1) == 0x24) {
                    // 设置参数N
                    if (mCachePool.at(1) == 0x34) {
                        qInfo().noquote() << "设置参数指令响应成功";
                    }
                    else {
                        qCritical().noquote() << "设置参数指令响应失败";
                    }

                    // 处理剩下数据（粘包处理）
                    mCachePool.remove(0, BASE_CMD_LENGTH);
                    continue;
                }
                else if (askCurrentCmd.at(1) == 0x25) {
                    // 输出积分
                    if (mCachePool.at(1) == 0x35) {

                    }

                    return;
                }
                else if (askCurrentCmd.at(1) == 0x26) {
                    // 输出波形
                    if (mCachePool.at(1) == 0x36) {
                        qInfo().noquote() << "输出波形指令响应成功";

                        // 启动
                        sendStartCmd();
                    }
                    else {
                        qCritical().noquote() << "输出波形指令响应失败";
                    }

                    // 处理剩下数据（粘包处理）
                    mCachePool.remove(0, BASE_CMD_LENGTH);
                    continue;
                }
                else if (askCurrentCmd.at(1) == 0x27) {
                    // 校准DRS4
                    if (mCachePool.at(1) == 0x37) {
                        qInfo().noquote() << "校准DRS4指令响应成功";
                    }
                    else {
                        qCritical().noquote() << "校准DRS4指令响应失败";
                    }

                    // 处理剩下数据（粘包处理）
                    mCachePool.remove(0, BASE_CMD_LENGTH);
                    continue;
                }
            }
            else{
                if (mCollectFinished) {
                    //已经采集完成了，判断停止采集指令
                    if (askCurrentCmd.at(1) == 0x23) {
                        // 停止
                        if (mCachePool.at(1) == 0x33) {
                            mWaveMeasuring = false;
                            qInfo().noquote() << "停止采集";
                            QMetaObject::invokeMethod(this, "measureEnd", Qt::QueuedConnection);
                        }
                        else {
                            qCritical().noquote() << "停止采集确认失败";
                        }

                        // 清空缓存
                        mCachePool.clear();
                        continue;
                    }
                    else{
                        // 处理剩下数据（粘包处理）
                        mCachePool.remove(0, 1);
                        continue;
                    }
                }
                else {
                    if (mCachePool.size() >= ONE_PACK_SIZE) {
                        qInfo().noquote() << "数据采集完成，开始解析数据";

                        //发送停止指令
                        if (mTriggerType == ttSingleTrigger) {
                            mCollectFinished = true;

                            /*单触发类型，收到完整数据需要发送停止测量指令*/
                            QTimer::singleShot(0, this, [=]{
                                if (mSocket && mSocket->state() == QAbstractSocket::ConnectedState){
                                    QByteArray askCurrentCmd = QByteArray::fromHex(QString("12 34 00 0f ff 10 11 11 00 00 ab cd").toUtf8());
                                    mSocket->write(askCurrentCmd);
                                    mSocket->waitForBytesWritten();
                                    qDebug().noquote()<<"Send HEX: "<<askCurrentCmd.toHex(' ');
                                }
                            });
                        }

                        //采集数据已经足够了，通知处理数据
                        // 实测曲线
                        QMetaObject::invokeMethod(this, [=]() {
                            emit onRawWaveData(mCachePool);
                        }, Qt::QueuedConnection);

                        // 处理剩下数据（粘包处理）
                        mCachePool.remove(0, ONE_PACK_SIZE);
                    }
                }
            }

            /*缓存池数据都处理完了*/
            if (mCachePool.size() == 0){
                break;
            }
        }
    }
}

void DataProcessor::sendInitCmd()
{
    askCurrentCmd = QByteArray::fromHex(QString("01 21 00 00").toUtf8());
    mSocket->write(askCurrentCmd);
    qDebug().noquote()<<"Send HEX: "<<askCurrentCmd.toHex(' ');
    qDebug().noquote()<< QString::fromUtf8(">> 打开探测器，发送指令：初始化");
}

void DataProcessor::sendWaveMode()
{
    askCurrentCmd = QByteArray::fromHex(QString("01 26 00 00").toUtf8());
    mSocket->write(askCurrentCmd);
    qDebug().noquote()<<"Send HEX: "<<askCurrentCmd.toHex(' ');
    qDebug().noquote()<< QString::fromUtf8(">> 发送指令：波形模式");
}

void DataProcessor::sendStartCmd()
{
    askCurrentCmd = QByteArray::fromHex(QString("01 00 00 00").toUtf8());
    if (mTriggerMode == TriggerMode::tmHardTrigger)
        askCurrentCmd[1] = 0x22;
    else if (mTriggerMode == TriggerMode::tmSoftTrigger)
        askCurrentCmd[1] = 0x28;
    else if (mTriggerMode == TriggerMode::tmTest)
        askCurrentCmd[1] = 0x29;
    else
        askCurrentCmd[1] = 0x22;
    mSocket->write(askCurrentCmd);
    qDebug().noquote()<<"Send HEX: "<<askCurrentCmd.toHex(' ');
    qDebug().noquote()<< QString::fromUtf8(">> 发送指令：触发模式");
}

void DataProcessor::sendStopCmd()
{
    askCurrentCmd = QByteArray::fromHex(QString("01 23 00 00").toUtf8());
    mSocket->write(askCurrentCmd);
    qDebug().noquote()<<"Send HEX: "<<askCurrentCmd.toHex(' ');
    qDebug().noquote()<< QString::fromUtf8(">> 打开探测器，发送指令：停止测量");
}
