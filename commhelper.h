#ifndef COMMHELPER_H
#define COMMHELPER_H

#include <QObject>
#include <QTcpSocket>
#include <QMutex>
#include <QFile>
#include <QElapsedTimer>
#include <QWaitCondition>

#include "qlitethread.h"
#include "dataprocessor.h"

#include "unfoldSpec.h"

class CommHelper : public QObject
{
    Q_OBJECT
public:
    explicit CommHelper(QObject *parent = nullptr);
    ~CommHelper();

    static CommHelper *instance() {
        static CommHelper commHelper;
        return &commHelper;
    }

    /*
     连接网络
    */
    void connectNet();
    /*
     断开网络
    */
    void disconnectNet();

    /*
     打开探测器
    */
    bool openDetector();
    /*
     断开探测器
    */
    void closeDetector();
    /*
     探测器连接/断开
    */

    /*
     设置发次信息
    */
    void setShotInformation(const QString shotDir, const quint32 shotNum);
    void setResultInformation(const QString reverseValue/*, const QString dadiationDose, const QString dadiationDoseRate*/);

    /*
     设置波形模式
    */
    void startMeasure(quint8 triggerMode, quint8 triggerType);

    /*
     开始测量
    */
    void startMeasure();

    /*
     停止测量
    */
    void stopMeasure();

    /*解析历史文件*/
    bool openHistoryWaveFile(const QString &filePath);

    /* 设置矩阵响应文件 */
    void setResMatrixFileName(const QString &fileName);

    //////////////////////////////////////////////////////
    /* 数据另存为 */
    bool saveAs(QString dstPath);

public slots:
    void errorOccurred(QAbstractSocket::SocketError);
    void socketConnected();
    void stateChanged(QAbstractSocket::SocketState);
    void onRawWaveData(const QByteArray data, bool needSave);//网络原生数据

signals:    
    void netConnected();  // 探测器
    void netDisconnected();

    void initSuccess(); //初始化成功
    void waitTriggerSignal();//等待触发信息

    void measureStart(); //测量开始
    void measureEnd(); //测量结束

    void measureDistanceStart(); //测量开始
    void measureDistanceEnd(); //测量结束

    void showRealCurve(const QMap<quint8, QVector<quint16>>& data);//波形曲线
    void showEnerygySpectrumCurve(const QVector<QPair<double, double>>& data);//反解能谱
    void exportEnergyPlot(const QString fileDir, const QString triggerTime);

    void refreshTriggerTimers(quint8);//刷新触发次数
private:
    /*********************************************************
     探测器指令
    ***********************************************************/

    /*
     反解能谱
    */
    // void calEnerygySpectrumCurve(bool needSave = true);

private:
    bool mDetectorsIsConnected = false;
    //bool mWaveMeasuring = false;     //波形测量中
    QString mShotDir;// 保存路径
    QString mShotNum;// 测量发次
    QString mReverseValue;
    QString mDadiationDose;
    QString mDadiationDoseRate;

    QTcpSocket *mSocketDetector = nullptr; //探测器
    DataProcessor* mDetectorDataProcessor = nullptr;
    quint8 mTriggerMode; // 触发模式
    quint8 mTriggerType; // 触发类型
    quint32 mTriggerTimers;// 触发次数
    QMap<quint8, QVector<quint16>> mWaveAllData;

    UnfoldSpec* unfoldData = nullptr;

    QString mResMatrixFileName;//响应矩阵文件

    /*
     初始化网络
    */
    void initSocket(QTcpSocket **socket);

    /*
     初始化数据处理器
    */
    void initDataProcessor(DataProcessor **processor, QTcpSocket *socket);
};

#endif // COMMHELPER_H
