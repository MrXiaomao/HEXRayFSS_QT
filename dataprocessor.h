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
        distanceMeasuring = on;
        singleMeasure = single;
    };

    /*
     * 开始测量
     */
    void startMeasureWave(quint8 mode, bool on = true){
        mTransferMode = mode;
        waveMeasuring = on;
        // if (on){
        //     QString strTime = QDateTime::currentDateTime().toString("yyyy-MM-dd_HHmmss");
        //     QString filePath = QString("%1%2_%3.dat").arg("./cache/").arg(detectorIndex).arg(strTime);
        //     mpfSaveNet = new QFile(filePath);
        //     if (mpfSaveNet->open(QIODevice::WriteOnly)) {
        //         qDebug().noquote() << tr("创建网口数据缓存文件成功，文件名：%1").arg(filePath);
        //     } else {
        //         qDebug().noquote() << tr("创建网口数据缓存文件失败，文件名：%1").arg(filePath);
        //     }
        // }
        // else {
        //     if (mpfSaveNet){
        //         mpfSaveNet->close();
        //         mpfSaveNet->deleteLater();
        //         mpfSaveNet = nullptr;
        //     }
        // }
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

    void measureStart(); //测量开始
    void measureEnd(); //测量结束

    void showRealCurve(const QMap<quint8, QVector<quint16>>& data);//实测曲线
    void showEnerygySpectrumCurve(const QVector<QPair<float, float>>& data);//反解能谱

private:
    quint8 detectorIndex = 0x00;
    QTcpSocket* mSocket = nullptr;
    QByteArray rawData; // 存储网络原始数据
    QByteArray cachePool; // 缓存数据，数据处理之前，先转移到二级缓存池
    QByteArray rawWaveData;// 波形数据
    QMap<quint8, QVector<quint16>> mRealCurve;// 4路通道实测曲线数据
    QFile *mpfSaveNet = nullptr;

    bool mDataReady = false;// 数据长度不够，还没准备好
    bool mTerminatedThead = false;
    QMutex mDataLocker;
    QWaitCondition mCondition;
    QLiteThread* dataProcessThread = nullptr;// 处理线程
    bool waveMeasuring = false;     //波形测量中
    bool distanceMeasuring = false; //距离测量中
    bool singleMeasure = false; //是否单次测量模式
    quint8 mTransferMode = 0x00;//传输模式
    quint32 mWaveLength = 512;// 波形长度
    quint8 mChWaveDataValidTag = 0x00;//通道数据是否完整

    void OnDataProcessThread();
};

#endif // DATAPROCESSOR_H
