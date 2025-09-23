#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "commhelper.h"
#include "QGoodWindow"
#include "QGoodCentralWidget"

QT_BEGIN_NAMESPACE
namespace Ui {
class CentralWidget;
}
QT_END_NAMESPACE

class QCustomPlot;
class QCPItemText;
class QCPItemLine;
class QCPItemRect;
class QCPGraph;
class QCPAbstractPlottable;
class QCPItemCurve;

class CentralWidget : public QMainWindow
{
    Q_OBJECT

public:
    enum StartupUIMode {
        STDUI_MODE = 0,
        MINIUI_MODE,
    };

    CentralWidget(QWidget *parent = nullptr);
    ~CentralWidget();

    /*
    初始化
    */
    void initUi();
    void initCustomPlot(QCustomPlot* customPlot, QString axisXLabel, QString axisYLabel, int graphCount = 1);

public:
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
    Ui::CentralWidget *ui;
    bool mRelayPowerOn = false;
    qreal windowTransparency = 1.0;
    bool windowTransparencyEnabled = false;
    bool isDarkTheme = true;
    QColor themeColor = QColor(255,255,255);

#ifdef MATLAB
    mwArray m_mwT;
    mwArray m_mwSeq;
    mwArray m_mwResponce_matrix;
    mwArray m_mwRom;
#endif

    CommHelper *commHelper = nullptr;
    class MainWindow *mainWindow = nullptr;
};

class MainWindow : public QGoodWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void setLaboratoryButton(QToolButton *laboratoryButton) {
        QTimer::singleShot(0, this, [this, laboratoryButton](){
            laboratoryButton->setFixedSize(m_good_central_widget->titleBarHeight(),m_good_central_widget->titleBarHeight());
            m_good_central_widget->setRightTitleBarWidget(laboratoryButton, false);
            connect(m_good_central_widget,&QGoodCentralWidget::windowActiveChanged,this, [laboratoryButton](bool active){
                laboratoryButton->setEnabled(active);
            });
        });
    }
    void fixMenuBarWidth(void) {
        if (m_menu_bar) {
            /* FIXME: Fix the width of the menu bar
             * please optimize this code */
            int width = 0;
            int itemSpacingPx = m_menu_bar->style()->pixelMetric(QStyle::PM_MenuBarItemSpacing);
            for (int i = 0; i < m_menu_bar->actions().size(); i++) {
                QString text = m_menu_bar->actions().at(i)->text();
                QFontMetrics fm(m_menu_bar->font());
                width += fm.size(0, text).width() + itemSpacingPx*1.5;
            }
            m_good_central_widget->setLeftTitleBarWidth(width);
        }
    }

    CentralWidget* centralWidget() const
    {
        return this->m_central_widget;
    }


protected:
    void closeEvent(QCloseEvent *event) override;
    bool event(QEvent * event) override;

private:
    QGoodCentralWidget *m_good_central_widget;
    QMenuBar *m_menu_bar = nullptr;
    CentralWidget *m_central_widget;
};

#endif // MAINWINDOW_H
