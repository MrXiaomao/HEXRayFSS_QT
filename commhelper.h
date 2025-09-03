#ifndef COMMHELPER_H
#define COMMHELPER_H

#include <QObject>
#include <QTcpSocket>
#include <QMutex>
#include <QFile>
#include <QElapsedTimer>
#include <QWaitCondition>

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

public slots:
    void errorOccurred(QAbstractSocket::SocketError);
    void socketConnected();
    void socketReadyRead();

signals:    
    void relayConnected();// 继电器
    void relayDisconnected();
    void detectorConnected(quint8 index);  // 探测器
    void detectorDisconnected(quint8 index);
    void distanceRespond(float distance, quint16 quality);// 测距模块距离和质量
    void temperatureRespond(float temperature);
    void appVersopmRespond(QString version, QString serialNumber);

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
    enum TransferMode{
        tmAppVersion = 0x01,    // 程序版本号
        tmWaveMode = 0x03,      // 波形
        tmTemperature = 0x05,   // 温度
        tmLaserDistance = 0x09  // 激光测距
    };
    void sendTransferModeCmd(quint8 mode);

    /*
     开始测量
    */
    enum TriggerMode{
        tmStop = 0x00, // 停止测量
        tmSoft = 0x01, // 软件触发
        tmHard = 0x02  // 硬件触发
    };
    void sendMeasureCmd(quint8 mode);

    /*
     程序版本查询
    */
    void sendQueryAppVersionCmd();

    /*
     温度查询
    */
    void sendQueryTemperaturCmd(quint8 on = 0x01);

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
    void sendSingleMeasureCmd(quint8 on = 0x01);

    /*
     开始连续测量
    */
    void sendContinueMeasureCmd(quint8 on = 0x01);

private:
    bool measuring = false;     //波形测量中
    bool singleMeasure = false; //是否单次测量模式
    quint8 waveLength = 128;    // 波形长度
    QTcpSocket *socketRelay;    //继电器
    QTcpSocket *socketDetector1; //探测器
    QTcpSocket *socketDetector2; //探测器
    QTcpSocket *socketDetector3; //探测器
    quint8 socketConectedStatus = ssNone; // 设备在线状态

    QByteArray askCurrentCmd;
    QByteArray askRelayPowerOnCmd;// 继电器电源-闭合
    QByteArray askRelayPowerOffCmd;// 继电器电源-断开
    QByteArray askQueryRelayPowerStatusCmd;// 查询继电器电源状态
    QByteArray ackRelayPowerStatusOnCmd;// 继电器电源状态-闭合
    QByteArray ackRelayPowerStatusOffCmd;// 继电器电源状态-断开

    QByteArray ackDistanceDataCmd;// 测距模块返回数据格式
    QByteArray ackHardTriggerCmd;// 硬件触发指令

    QByteArray rawWaveData[3]; //原始波形数据
    bool mDataReady = false;// 数据长度不够，还没准备好
    QWaitCondition mCondition;//
    QLiteThread* dataProcessThread;// 处理线程
    void OnDataProcessThread();
    /*
     初始化网络
    */
    void initSocket(QTcpSocket **socket);
    bool connectRelay();
    bool connectDetectors();

    /*
     初始化指令
    */
    void initCommand();
};

#endif // COMMHELPER_H
