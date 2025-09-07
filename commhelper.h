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

    void measureStart(); //测量开始
    void measureEnd(); //测量结束

    void showRealCurve(const QMap<quint8, QVector<quint16>>& data);//实测曲线
    void showEnerygySpectrumCurve(const QVector<QPair<float, float>>& data);//反解能谱

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

private:
    quint8 detectorConnectedTag = 0x00;
    bool relayIsConnected = false;
    bool detectorsIsConnected = false;
    bool waveMeasuring = false;     //波形测量中
    bool distanceMeasuring = false; //距离测量中
    bool singleMeasure = false; //是否单次测量模式
    quint16 chWaveDataValidTag = 0x00;//标记通道数据是否接收完成
    QString shotDir;// 保存路径
    quint32 shotNum;// 测量发次
    quint8 waveLength = 512;    // 波形长度
    QTcpSocket *socketRelay = nullptr;    //继电器
    QTcpSocket *socketDetector1 = nullptr; //探测器
    QTcpSocket *socketDetector2 = nullptr; //探测器
    QTcpSocket *socketDetector3 = nullptr; //探测器
    DataProcessor* detector1DataProcessor = nullptr;
    DataProcessor* detector2DataProcessor = nullptr;
    DataProcessor* detector3DataProcessor = nullptr;
    quint8 socketConectedStatus = ssNone; // 设备在线状态

    QByteArray dataHeadCmd;// 数据头
    QByteArray dataTailCmd;// 数据尾
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

    QByteArray rawWaveData[3]; //原始波形数据
    bool mDataReady = false;// 数据长度不够，还没准备好
    bool mTerminatedThead = false;
    QMutex mReceivePoolLocker;
    QWaitCondition mCondition;//
    QLiteThread* dataProcessThread = nullptr;// 处理线程
    void OnDataProcessThread();
    QByteArray rawWaveAllData;//波形总数据

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

    /*
     探测器连接/断开
    */
    bool connectDetectors();
    void disconnectDetectors();
};

#endif // COMMHELPER_H
