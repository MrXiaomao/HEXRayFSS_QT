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
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    /*
    初始化
    */
    void initCustomPlot(QCustomPlot* customPlot, QString axisXLabel, QString axisYLabel);

public slots:
    void slotWriteLog(const QString &msg, QtMsgType msgType);

signals:
    void sigUpdateBootInfo(const QString &msg);
    void sigWriteLog(const QString &msg, QtMsgType msgType = QtDebugMsg);

private slots:
    void on_action_netCfg_triggered();

    void on_action_cfgParam_triggered();

    void on_action_exit_triggered();

private:
    Ui::MainWindow *ui;

#ifdef MATLAB
    mwArray m_mwT;
    mwArray m_mwSeq;
    mwArray m_mwResponce_matrix;
    mwArray m_mwRom;
#endif

    CommHelper *commHelper;
};
#endif // MAINWINDOW_H
