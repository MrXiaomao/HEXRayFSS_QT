#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "commhelper.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class QCustomPlot;
class QCPItemText;
class QCPItemLine;
class QCPItemRect;
class QCPGraph;
class QCPAbstractPlottable;
class QCPItemCurve;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    enum StartupUIMode {
        STDUI_MODE = 0,
        MINIUI_MODE,
    };

    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    /*
    初始化
    */
    void initUi();
    void initCustomPlot(QCustomPlot* customPlot, QString axisXLabel, QString axisYLabel, int graphCount = 1);

protected:
    virtual void closeEvent(QCloseEvent *event) override;
    virtual bool event(QEvent * event) override;
    virtual bool eventFilter(QObject *watched, QEvent *event) override;

public slots:
    void slotWriteLog(const QString &msg, QtMsgType msgType = QtDebugMsg);//操作日志
    void showRealCurve(const QMap<quint8, QVector<quint16>>& data);//实测曲线
    void showEnerygySpectrumCurve(const QVector<QPair<float, float>>& data);//反解能谱

signals:
    void sigUpdateBootInfo(const QString &msg);
    void sigWriteLog(const QString &msg, QtMsgType msgType = QtDebugMsg);

private slots:
    void on_action_netCfg_triggered();

    void on_action_cfgParam_triggered();

    void on_action_exit_triggered();

    void on_action_open_triggered();

    void on_action_connect_triggered();

    void on_action_disconnect_triggered();

    void on_action_startMeasure_triggered();

    void on_action_stopMeasure_triggered();

    void on_action_powerOn_triggered();

    void on_action_powerOff_triggered();

    void on_pushButton_stopMeasureDistance_clicked();

    void on_pushButton_startMeasureDistance_clicked();

    void on_action_connectRelay_triggered();

    void on_action_disconnectRelay_triggered();

    void on_action_about_triggered();

    void on_pushButton_export_clicked();

    void on_pushButton_clicked();

    void on_action_exportImg_triggered();

private:
    Ui::MainWindow *ui;
    bool mRelayPowerOn = false;

#ifdef MATLAB
    mwArray m_mwT;
    mwArray m_mwSeq;
    mwArray m_mwResponce_matrix;
    mwArray m_mwRom;
#endif

    CommHelper *commHelper = nullptr;
};

#endif // MAINWINDOW_H
