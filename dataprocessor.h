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
    explicit DataProcessor(quint8 index, QTcpSocket* socket, QObject *parent = nullptr);
    ~DataProcessor();

    /*
     * 添加数据
     */
    void inputData(const QByteArray& data);

    /*
     * 开始测距
     */
    void startMeasureDistance(bool on = true, bool single = true){
        mDistanceMeasuring = on;
        mSingleMeasure = single;
    };

    /*
     * 开始测量
     */
    void startMeasureWave(quint8 mode, bool on = true){
        mTransferMode = mode;
        mWaveMeasuring = on;
    };

public slots:
    void socketReadyRead();

signals:   
    void relayConnected();// 继电器
    void relayDisconnected();
    void relayPowerOn();
    void relayPowerOff();

    void detectorConnected(quint8 index);  // 探测器
    void detectorDisconnected(quint8 index);
    void temperatureRespond(quint8 index, float temperature);
    void appVersionRespond(quint8 index, QString version, QString serialNumber);
    void distanceRespond(float distance, quint16 quality);// 测距模块距离和质量

    void measureStart(quint8 index); //测量开始
    void measureEnd(quint8 index); //测量结束

    void showRealCurve(const QMap<quint8, QVector<quint16>>& data);//实测曲线
    void showEnerygySpectrumCurve(const QVector<QPair<float, float>>& data);//反解能谱

private:
    quint8 mdetectorIndex = 0x00;
    QTcpSocket* mSocket = nullptr;
    QByteArray mRawData; // 存储网络原始数据
    QByteArray mCachePool; // 缓存数据，数据处理之前，先转移到二级缓存池
    QMap<quint8, QVector<quint16>> mRealCurve;// 4路通道实测曲线数据
    QFile *mpfSaveNet = nullptr;

    bool mDataReady = false;// 数据长度不够，还没准备好
    bool mTerminatedThead = false;
    QMutex mDataLocker;
    QWaitCondition mCondition;
    QLiteThread* mdataProcessThread = nullptr;// 处理线程
    bool mWaveMeasuring = false;     //波形测量中
    bool mDistanceMeasuring = false; //距离测量中
    bool mSingleMeasure = false; //是否单次测量模式
    quint8 mTransferMode = 0x00;//传输模式
    quint32 mWaveLength = 512;// 波形长度
    quint8 mChWaveDataValidTag = 0x00;//通道数据是否完整

    void OnDataProcessThread();
};

#endif // DATAPROCESSOR_H
