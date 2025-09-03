#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "QGoodWindow"
#include "QGoodCentralWidget"
#include "commhelper.h"

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
class MainWindow;
class CentralWidget : public QMainWindow
{
    Q_OBJECT

public:
    enum StartupUIMode {
        STDUI_MODE = 0,
        MINIUI_MODE,
    };

    CentralWidget(bool isDark = true,
                  QWidget *parent = nullptr);
    ~CentralWidget();

    /*
    初始化
    */
    void initUi();
    void initCustomPlot(QCustomPlot* customPlot, QString axisXLabel, QString axisYLabel);

    void checkCloseEvent(QCloseEvent *event);
    void checkStatusTipEvent(QStatusTipEvent *event);

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
    Ui::CentralWidget *ui;
    bool isDarkTheme = true;
    class MainWindow *mainWindow = nullptr;

#ifdef MATLAB
    mwArray m_mwT;
    mwArray m_mwSeq;
    mwArray m_mwResponce_matrix;
    mwArray m_mwRom;
#endif

    CommHelper *commHelper;
};

class MainWindow : public QGoodWindow
{
    Q_OBJECT
public:
    explicit MainWindow(bool isDark = true,
                        QWidget *parent = nullptr);
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

    CentralWidget* centralWidget(){
        return m_central_widget;
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
