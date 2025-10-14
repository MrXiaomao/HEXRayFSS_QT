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

#ifdef ENABLE_MATLAB
#include "UnfolddingAlgorithm_Gravel.h"
#include "mclmcrrt.h"  // MATLAB 运行时头文件
#include "mclcppclass.h"  // mwArray 头文件
extern bool gMatlabInited;
#else
#include "unfoldSpec.h"
#endif //ENABLE_MATLAB

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

    enum SocketConectedStatus{
        ssNone      = 0x00,     // 都不在线
        ssDetector  = 0x01,     // 探测器1
    };
    enum TriggerMode{
        tmStop = 0x00, // 停止测量
        tmSoft = 0x01, // 软件触发
        tmHard = 0x02, // 硬件触发
        tmTest = 0x03  // 测试模式
    };

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
    bool connectDetectors();
    /*
     断开探测器
    */
    void disconnectDetectors();
    /*
     探测器连接/断开
    */

    /*
     设置发次信息
    */
    void setShotInformation(const QString shotDir, const quint32 shotNum);
    void setResultInformation(const QString reverseValue, const QString dadiationDose, const QString dadiationDoseRate);

    /*
     开始测量
    */
    void startMeasure(quint8 triggerMode);
    /*
     停止测量
    */
    void stopMeasure();

    /*
     开始测距
    */
    void startMeasureDistance(bool isContinue = false);
    /*
     停止测距
    */
    void stopMeasureDistance();

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

signals:    
    void detectorConnected();  // 探测器
    void detectorDisconnected();

    void measureStart(); //测量开始
    void measureEnd(); //测量结束

    void measureDistanceStart(); //测量开始
    void measureDistanceEnd(); //测量结束

    void showHistoryCurve(const QMap<quint8, QVector<quint16>>& data);//实测曲线
    void showRealCurve(const QMap<quint8, QVector<quint16>>& data);//实测曲线
    void showEnerygySpectrumCurve(const QVector<QPair<double, double>>& data);//反解能谱
    void exportEnergyPlot(const QString fileDir, const QString triggerTime);

private:
    /*********************************************************
     探测器指令
    ***********************************************************/

    /*
     触发阈值
    */
    void sendTriggerTholdCmd();

    /*
     波形触发模式
    */
    void sendWaveTriggerModeCmd();

    /*
     波形长度
    */
    void sendWaveLengthCmd();

    /*
     程控增益
    */
    void sendGainCmd();

    /*
     开始测量
    */
    void sendMeasureCmd(quint8 mode);//TriggerMode

    /*
     反解能谱
    */
    void calEnerygySpectrumCurve(bool needSave = true);

private:
    bool mRelayIsConnected = false;
    bool mDetectorsIsConnected = false;
    bool mWaveMeasuring = false;     //波形测量中
    bool mDistanceMeasuring = false; //距离测量中
    bool mSingleMeasure = false; //是否单次测量模式
    QString mShotDir;// 保存路径
    QString mShotNum;// 测量发次
    QString mReverseValue;
    QString mDadiationDose;
    QString mDadiationDoseRate;

    QTcpSocket *mSocketDetector = nullptr; //探测器
    DataProcessor* mDetectorDataProcessor = nullptr;
    quint8 mSocketConectedStatus = ssNone; // 设备在线状态

    QByteArray askCurrentCmd;// 当前发送指令

    QMap<quint8, QVector<quint16>> mWaveAllData;
#ifdef ENABLE_MATLAB
    mwArray m_mwT;
    mwArray m_mwSeq;
    mwArray m_mwResponce_matrix;

    bool loadSeq(double* seq);
    bool reloadResponceMatrix();
    bool loadResponceMatrix(double* responceMatrix);
    bool loadRom(double* rom);
    bool loadData(double* data);
#else
    UnfoldSpec* unfoldData = nullptr;
#endif //ENABLE_MATLAB
    QString mResMatrixFileName;

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
