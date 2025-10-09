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
        ssRelay     = 0x01,     // 继电器
        ssDetector1 = 0x02,     // 探测器1
        ssDetector2 = 0x04,     // 探测器2
        ssDetector3 = 0x08,     // 探测器3
        ssAll       = 0x0F      // 所有设备都在线
    };
    enum TransferMode{
        tmAppVersion = 0x01,    // 程序版本号
        tmWaveMode = 0x03,      // 波形
        tmTemperature = 0x05,   // 温度
        tmLaserDistance = 0x09  // 激光测距
    };
    enum TriggerMode{
        tmStop = 0x00, // 停止测量
        tmSoft = 0x01, // 软件触发
        tmHard = 0x02  // 硬件触发
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
     打开电源
    */
    void openPower();
    /*
     断开电源
    */
    void closePower();

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
    void startMeasure(quint8 mode);
    /*
     停止测量
    */
    void stopMeasure();

    /*
     温度查询
    */
    void queryTemperature(quint8 index, quint8 on = true);

    /*
     继电器状态查询
    */
    void queryRelayStatus();

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

    //////////////////////////////////////////////////////
    /*
     打开测距模块电源
    */
    void openDistanceModulePower();
    /*
     断开测距模块电源
    */
    void closeDistanceModulePower();

    /*
     打开测距模块激光
    */
    void openDistanceModuleLaser();
    /*
     断开测距模块激光
    */
    void closeDistanceModuleLaser();

    bool saveAs(QString dstPath);

public slots:
    void errorOccurred(QAbstractSocket::SocketError);
    void socketConnected();
    void socketReadyRead();
    void stateChanged(QAbstractSocket::SocketState);

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

    void measureDistanceStart(); //测量开始
    void measureDistanceEnd(); //测量结束

    void showHistoryCurve(const QMap<quint8, QVector<quint16>>& data);//实测曲线
    void showRealCurve(const QMap<quint8, QVector<quint16>>& data);//实测曲线
    void showEnerygySpectrumCurve(const QVector<QPair<double, double>>& data);//反解能谱
    void exportEnergyPlot(const QString fileDir, const QString triggerTime);

private:
    /*********************************************************
     继电器指令
    ***********************************************************/
    /*
     控制单路通断
    */
    void sendRelayPowerSwitcherCmd(quint8 on = 0x01);

    /*
     查询状态
    */
    void sendQueryRelayStatusCmd();

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
     传输模式
    */
    void sendTransferModeCmd(quint8 index, quint8 mode);//TransferMode

    /*
     开始测量
    */
    void sendMeasureCmd(quint8 mode);//TriggerMode

    /*
     程序版本查询
    */
    void sendQueryAppVersionCmd(quint8 index = 0x01);

    /*
     温度查询
    */
    void sendQueryTemperaturCmd(quint8 index, quint8 on = 0x01);

    /*********************************************************
     测距模块指令
    ***********************************************************/

    /*
     模块电源打开/关闭
    */
    void sendPowerSwitcherCmd(quint8 on = 0x01);

    /*
     激光打开/关闭
    */
    void sendLaserSwitcherCmd(quint8 on = 0x01);

    /*
     开始单次测量
    */
    void sendSingleMeasureCmd();

    /*
     开始连续测量
    */
    void sendContinueMeasureCmd(quint8 on = 0x01);

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

    QTcpSocket *mSocketRelay = nullptr;    //继电器
    QTcpSocket *mSocketDetector1 = nullptr; //探测器
    QTcpSocket *mSocketDetector2 = nullptr; //探测器
    QTcpSocket *mSocketDetector3 = nullptr; //探测器
    DataProcessor* mDetector1DataProcessor = nullptr;
    DataProcessor* mDetector2DataProcessor = nullptr;
    DataProcessor* mDetector3DataProcessor = nullptr;
    quint8 mSocketConectedStatus = ssNone; // 设备在线状态

    QByteArray askCurrentCmd;// 当前发送指令
    QByteArray askRelayPowerOnCmd;// 继电器电源-闭合
    QByteArray askRelayPowerOffCmd;// 继电器电源-断开
    QByteArray askQueryRelayPowerStatusCmd;// 查询继电器电源状态
    QByteArray ackRelayPowerStatusOnCmd;// 继电器电源状态-闭合
    QByteArray ackRelayPowerStatusOffCmd;// 继电器电源状态-断开

    QByteArray ackDistanceDataCmd;// 测距模块返回数据格式
    QByteArray ackHardTriggerCmd;// 硬件触发指令

    QByteArray askAppVersionCmd;// 程序版本查询发指令
    QByteArray askTemperatureCmd;// 温度查询指令

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
#endif //ENABLE_MATLAB

    /*
     初始化网络
    */
    void initSocket(QTcpSocket **socket);

    /*
     初始化数据处理器
    */
    void initDataProcessor(DataProcessor **processor, QTcpSocket *socket, quint8 index);

    /*
     初始化指令
    */
    void initCommand();

    /*
     继电器连接/断开
    */
    bool connectRelay();
    void disconnectRelay();
};

#endif // COMMHELPER_H
