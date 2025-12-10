#ifndef DATAPROCESSOR_H
#define DATAPROCESSOR_H

#include <QObject>
#include <QMutex>
#include <QWaitCondition>
#include <QtEndian>
#include <QTcpSocket>
#include <QFile>
#include <QDateTime>

#include "qlitethread.h"
class DataProcessor : public QObject
{
    Q_OBJECT
public:
    explicit DataProcessor(QTcpSocket* socket, QObject *parent = nullptr);
    ~DataProcessor();

    enum TriggerMode
    {
        tmHardTrigger = 0,
        tmSoftTrigger = 1,
        tmTestTrigger = 2
    };

    enum TriggerType
    {
        ttSingleTrigger = 0,	// 单次触发
        ttContinueTrigger = 1	// 连续触发
    };

    /*
     * 添加数据
     */
    void inputData(const QByteArray& data);

    /*
     * 开始测量
     */
    void startMeasure(quint8 triggerMode, quint8 triggerType){
        mWaveMeasuring = false;
        mTriggerMode = triggerMode;
        mTriggerType = triggerType;
        mCachePool.clear();

        this->sendWaveModeCmd();
    };

    /*
     * 停止测量
     */
    void stopMeasure(){
        this->sendStopCmd();
    };

private:
    QByteArray askCurrentCmd;// 当前发送指令

public slots:
    void socketReadyRead();

    void sendInitCmd(); //初始化指令
    void sendWaveModeCmd();//波形模式
    void sendStartCmd(); //开始测量指令
    void sendStopCmd();//停止测量

signals:
    void initSuccess(); //初始化成功
    void waitTriggerSignal();//等待触发

    void measureStart(); //测量开始
    void measureEnd(); //测量结束

    void onRawWaveData(const QByteArray data, bool needSave);//网络原生数据

private:
    QTcpSocket* mSocket = nullptr;
    QByteArray mRawData; // 存储网络原始数据
    QByteArray mCachePool; // 缓存数据，数据处理之前，先转移到二级缓存池

    bool mDataReady = false;// 数据长度不够，还没准备好
    bool mTerminatedThead = false;
    QMutex mDataLocker;
    QWaitCondition mCondition;
    QLiteThread* mdataProcessThread = nullptr;// 处理线程
    bool mWaveMeasuring = false;     //波形测量中
    quint8 mTriggerMode = 0x00;//触发模式
    quint8 mTriggerType = 0x00;//触发类型（单次/连续）

    void OnDataProcessThread();
};

#endif // DATAPROCESSOR_H
