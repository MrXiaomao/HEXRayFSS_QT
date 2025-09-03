#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "qcustomplot.h"
#include "netsetting.h"
#include "paramsetting.h"

CentralWidget::CentralWidget(bool isDark, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::CentralWidget)
    , isDarkTheme(isDark)
    , mainWindow(static_cast<MainWindow *>(parent))
{
    ui->setupUi(this);

    initUi();
    initCustomPlot(ui->customPlot, tr("道址"), tr("实测曲线（通道1-4）"));
    initCustomPlot(ui->customPlot_2, tr("道址"), tr("实测曲线（通道5-8）"));
    initCustomPlot(ui->customPlot_3, tr("道址"), tr("实测曲线（通道9-11）"));
    initCustomPlot(ui->customPlot_result, tr("能量/MeV"), tr("反解能谱）"));

    commHelper = CommHelper::instance();
    connect(this, SIGNAL(sigWriteLog(const QString&,QtMsgType)), this, SLOT(slotWriteLog(const QString&,QtMsgType)));
}

CentralWidget::~CentralWidget()
{
    delete ui;
}

void CentralWidget::initUi()
{
    ui->stackedWidget->hide();
    QPushButton* laserDistanceButton = new QPushButton();
    laserDistanceButton->setText(tr("测距模块"));
    laserDistanceButton->setFixedSize(250,26);
    QPushButton* detectorStatusButton = new QPushButton();
    detectorStatusButton->setText(tr("设备状态"));
    detectorStatusButton->setFixedSize(250,26);

    QHBoxLayout* sideHboxLayout = new QHBoxLayout();
    sideHboxLayout->setContentsMargins(0,0,0,0);
    sideHboxLayout->setSpacing(2);

    QWidget* sideProxyWidget = new QWidget();
    sideProxyWidget->setLayout(sideHboxLayout);
    sideHboxLayout->addWidget(laserDistanceButton);
    sideHboxLayout->addWidget(detectorStatusButton);

    QGraphicsScene *scene = new QGraphicsScene(this);
    QGraphicsProxyWidget *w = scene->addWidget(sideProxyWidget);
    w->setPos(0,0);
    w->setRotation(-90);
    ui->graphicsView->setScene(scene);
    ui->graphicsView->setFrameStyle(0);
    ui->graphicsView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->graphicsView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->graphicsView->setFixedSize(30, 502);
    ui->sidewidget->setFixedWidth(30);

    // QHBoxLayout* centralHboxLayout = new QHBoxLayout();
    // centralHboxLayout->setContentsMargins(0,0,0,0);
    // centralHboxLayout->setSpacing(2);
    // centralHboxLayout->addWidget(ui->sidewidget);
    // centralHboxLayout->addWidget(ui->stackedWidget);
    // centralHboxLayout->addLayout(ui->leftVboxLayout);
    // centralHboxLayout->addLayout(ui->rightVboxLayout);
    // ui->centralwidget->setLayout(centralHboxLayout);
    // centralHboxLayout->setSizes(QList<int>() << 1 << 100000 << 100000 << 100000 << 1);

    ui->tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->switchButton_power->setAutoChecked(false);
    ui->switchButton_laser->setAutoChecked(false);
    connect(ui->switchButton_power, &SwitchButton::clicked, this, [=](bool checked){
        if (!checked){
            // 测距模块电源
        }
    });
    connect(ui->switchButton_laser, &SwitchButton::clicked, this, [=](bool checked){
        if (!checked){
            // 测距模块激光
        }
    });

    connect(laserDistanceButton,&QPushButton::clicked,this,[&](){
        if(ui->stackedWidget->isHidden()) {
            ui->stackedWidget->setCurrentWidget(ui->laserDistanceWidget);
            ui->stackedWidget->show();
        } else {
            if(ui->stackedWidget->currentWidget() == ui->laserDistanceWidget) {
                ui->stackedWidget->hide();
            } else {
                ui->stackedWidget->setCurrentWidget(ui->laserDistanceWidget);
            }
        }
    });
    connect(detectorStatusButton,&QPushButton::clicked,this,[&](){
        if(ui->stackedWidget->isHidden()) {
            ui->stackedWidget->setCurrentWidget(ui->detectorStatusWidget);
            ui->stackedWidget->show();
        } else {
            if(ui->stackedWidget->currentWidget() == ui->detectorStatusWidget) {
                ui->stackedWidget->hide();
            } else {
                ui->stackedWidget->setCurrentWidget(ui->detectorStatusWidget);
            }
        }
    });
}

void CentralWidget::initCustomPlot(QCustomPlot* customPlot, QString axisXLabel, QString axisYLabel)
{
    //customPlot->setObjectName(objName);

    // 设置全局抗锯齿
    customPlot->setAntialiasedElements(QCP::aeAll);
    //customPlot->setNotAntialiasedElements(QCP::aeAll);
    // 图例名称隐藏
    customPlot->legend->setVisible(false);
    customPlot->legend->setFillOrder(QCPLayoutGrid::foColumnsFirst);//设置图例在一行中显示
    // 图例名称显示位置
    customPlot->axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignTop|Qt::AlignRight);
    // 设置边界
    //customPlot->setContentsMargins(0, 0, 0, 0);
    // 设置标签倾斜角度，避免显示不下
    customPlot->xAxis->setTickLabelRotation(-45);
    // 背景色
    //customPlot->setBackground(QBrush(Qt::white));
    // 图像画布边界
    //customPlot->axisRect()->setMinimumMargins(QMargins(0, 0, 0, 0));
    // 坐标背景色
    //customPlot->axisRect()->setBackground(Qt::white);
    // 允许拖拽，缩放
    customPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables);
    // 允许轴自适应大小
    customPlot->xAxis->rescale(true);
    customPlot->yAxis->rescale(true);
    // 设置刻度范围
    customPlot->xAxis->setRange(0, 1024);
    customPlot->yAxis->setRange(-10, 10);
    customPlot->yAxis->ticker()->setTickCount(5);
    customPlot->xAxis->ticker()->setTickCount(10);

    customPlot->yAxis2->ticker()->setTickCount(5);
    customPlot->xAxis2->ticker()->setTickCount(10);
    // 设置刻度可见
    // customPlot->xAxis->setTicks(true);
    // customPlot->xAxis2->setTicks(true);
    // customPlot->yAxis->setTicks(true);
    // customPlot->yAxis2->setTicks(true);
    // 设置刻度高度
    // customPlot->xAxis->setTickLength(13);
    // customPlot->yAxis->setTickLength(13);
    // customPlot->xAxis->setSubTickLength(4);
    // customPlot->yAxis->setSubTickLength(4);

    // customPlot->xAxis2->setTickLength(13);
    // customPlot->yAxis2->setTickLength(13);
    // customPlot->xAxis2->setSubTickLength(4);
    // customPlot->yAxis2->setSubTickLength(4);
    // 设置轴线可见
    // customPlot->xAxis->setVisible(true);
    // customPlot->xAxis2->setVisible(true);
    // customPlot->yAxis->setVisible(true);
    // customPlot->yAxis2->setVisible(true);
    //customPlot->axisRect()->setupFullAxesBox();//四边安装轴并显示
    // 设置刻度标签可见
    // customPlot->xAxis->setTickLabels(true);
    // customPlot->xAxis2->setTickLabels(false);
    // customPlot->yAxis->setTickLabels(true);
    // customPlot->yAxis2->setTickLabels(false);
    // 设置子刻度可见
    // customPlot->xAxis->setSubTicks(false);
    // customPlot->xAxis2->setSubTicks(false);
    // customPlot->yAxis->setSubTicks(false);
    // customPlot->yAxis2->setSubTicks(false);
    //设置轴标签名称
    customPlot->xAxis->setLabel(axisXLabel);
    customPlot->yAxis->setLabel(axisYLabel);
    // 设置网格线颜色
    // customPlot->xAxis->grid()->setPen(QPen(QColor(114, 114, 114, 255), 1, Qt::PenStyle::DashLine));
    // customPlot->yAxis->grid()->setPen(QPen(QColor(114, 114, 114, 255), 1, Qt::PenStyle::DashLine));
    // customPlot->xAxis->grid()->setSubGridPen(QPen(QColor(50, 50, 50, 128), 1, Qt::DotLine));
    // customPlot->yAxis->grid()->setSubGridPen(QPen(QColor(50, 50, 50, 128), 1, Qt::DotLine));
    // customPlot->xAxis->grid()->setZeroLinePen(QPen(QColor(50, 50, 50, 100), 1, Qt::SolidLine));
    // customPlot->yAxis->grid()->setZeroLinePen(QPen(QColor(50, 50, 50, 100), 1, Qt::SolidLine));
    // 设置网格线是否可见
    // customPlot->xAxis->grid()->setVisible(false);
    // customPlot->yAxis->grid()->setVisible(false);
    // 设置子网格线是否可见
    // customPlot->xAxis->grid()->setSubGridVisible(false);
    // customPlot->yAxis->grid()->setSubGridVisible(false);

    // 添加散点图
    // for (int i=0; i<count; ++i){
    //     QCPGraph * mainGraph = customPlot->addGraph(customPlot->xAxis, customPlot->yAxis);
    //     mainGraph->setName("mainGraph");
    //     mainGraph->setAntialiased(false);
    //     mainGraph->setPen(QPen(QColor(i == 0x00 ? clrLine : clrLine2)));
    //     mainGraph->selectionDecorator()->setPen(QPen(i == 0x00 ? clrLine : clrLine2));
    //     mainGraph->setLineStyle(QCPGraph::lsNone);// 隐藏线性图
    //     mainGraph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, 5));//显示散点图
    // }

    connect(customPlot, SIGNAL(beforeReplot()), this, SLOT(slotBeforeReplot()));
    connect(customPlot, SIGNAL(afterLayout()), this, SLOT(slotBeforeReplot()));

    connect(customPlot->xAxis, SIGNAL(rangeChanged(const QCPRange &)), customPlot->xAxis2, SLOT(setRange(const QCPRange &)));
    connect(customPlot->yAxis, SIGNAL(rangeChanged(const QCPRange &)), customPlot->yAxis2, SLOT(setRange(const QCPRange &)));


    // 是否允许X轴自适应缩放
    connect(customPlot, SIGNAL(mousePress(QMouseEvent*)), this, SLOT(slotShowTracer(QMouseEvent*)));
    connect(customPlot, SIGNAL(mouseRelease(QMouseEvent*)), this, SLOT(slotRestorePlot(QMouseEvent*)));
    //connect(customPlot, SIGNAL(mouseWheel(QWheelEvent*)), this, SLOT(slotRestorePlot(QWheelEvent*)));
    //connect(customPlot, SIGNAL(mouseMove(QMouseEvent*)), this,SLOT(slotShowTracer(QMouseEvent*)));
}

void CentralWidget::checkCloseEvent(QCloseEvent *event)
{
    QMessageBox::StandardButton reply = QMessageBox::question(this, tr("提示"), tr("您确定要退出吗？"),
                                                              QMessageBox::Yes|QMessageBox::No);
    if(reply == QMessageBox::Yes) {
        event->accept();
    } else {
        event->ignore();
    }
}

void CentralWidget::checkStatusTipEvent(QStatusTipEvent *event)
{
    if (!event->tip().isEmpty()) {
        ui->statusbar->showMessage(event->tip(), 2000);
    }
}

void CentralWidget::slotWriteLog(const QString &msg, QtMsgType msgType)
{
    // 创建一个 QTextCursor
    QTextCursor cursor = ui->textEdit_log->textCursor();
    // 将光标移动到文本末尾
    cursor.movePosition(QTextCursor::End);

    // 先插入时间
    cursor.insertHtml(QString("<span style='color:black;'>%1</span>").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz >> ")));
    // 再插入文本
    if (msgType == QtDebugMsg || msgType == QtInfoMsg)
        cursor.insertHtml(QString("<span style='color:black;'>%1</span>").arg(msg));
    else if (msgType == QtCriticalMsg || msgType == QtFatalMsg)
        cursor.insertHtml(QString("<span style='color:red;'>%1</span>").arg(msg));
    else
        cursor.insertHtml(QString("<span style='color:green;'>%1</span>").arg(msg));

    // 最后插入换行符
    cursor.insertHtml("<br>");

    // 确保 QTextEdit 显示了光标的新位置
    ui->textEdit_log->setTextCursor(cursor);

    //限制行数
    QTextDocument *document = ui->textEdit_log->document(); // 获取文档对象，想象成打开了一个TXT文件
    int rowCount = document->blockCount(); // 获取输出区的行数
    int maxRowNumber = 2000;//设定最大行
    if(rowCount > maxRowNumber){//超过最大行则开始删除
        QTextCursor cursor = QTextCursor(document); // 创建光标对象
        cursor.movePosition(QTextCursor::Start); //移动到开头，就是TXT文件开头

        for (int var = 0; var < rowCount - maxRowNumber; ++var) {
            cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor); // 向下移动并选中当前行
        }
        cursor.removeSelectedText();//删除选择的文本
    }
}

void CentralWidget::on_action_netCfg_triggered()
{
    NetSetting w;
    w.exec();
}


void CentralWidget::on_action_cfgParam_triggered()
{
    ParamSetting w;
    w.exec();
}


void CentralWidget::on_action_exit_triggered()
{
    this->close();
}

////////////////////////////////////////
MainWindow::MainWindow(bool isDark,
                       QWidget *parent)
    : QGoodWindow(parent) {
    m_central_widget = new CentralWidget(isDark,this);
    m_central_widget->setWindowFlags(Qt::Widget);

    m_good_central_widget = new QGoodCentralWidget(this);

#ifdef Q_OS_MAC
    //macOS uses global menu bar
    if(QApplication::testAttribute(Qt::AA_DontUseNativeMenuBar)) {
#else
    if(true) {
#endif
        m_menu_bar = m_central_widget->menuBar();

        //Set font of menu bar
        QFont font = m_menu_bar->font();
#ifdef Q_OS_WIN
        font.setFamily("Segoe UI");
#else
        font.setFamily(qApp->font().family());
#endif
        m_menu_bar->setFont(font);

        QTimer::singleShot(0, this, [&]{
            const int title_bar_height = m_good_central_widget->titleBarHeight();
            m_menu_bar->setStyleSheet(QString("QMenuBar {height: %0px;}").arg(title_bar_height));
        });

        connect(m_good_central_widget,&QGoodCentralWidget::windowActiveChanged,this, [&](bool active){
            m_menu_bar->setEnabled(active);
#ifdef Q_OS_MACOS
            fixWhenShowQuardCRTTabPreviewIssue();
#endif
        });

        m_good_central_widget->setLeftTitleBarWidget(m_menu_bar);
        setNativeCaptionButtonsVisibleOnMac(false);
    } else {
        setNativeCaptionButtonsVisibleOnMac(true);
    }

    connect(qGoodStateHolder, &QGoodStateHolder::currentThemeChanged, this, [](){
        if (qGoodStateHolder->isCurrentThemeDark())
            QGoodWindow::setAppDarkTheme();
        else
            QGoodWindow::setAppLightTheme();
    });
    connect(this, &QGoodWindow::systemThemeChanged, this, [&]{
        qGoodStateHolder->setCurrentThemeDark(QGoodWindow::isSystemThemeDark());
    });
    qGoodStateHolder->setCurrentThemeDark(isDark);

    m_good_central_widget->setCentralWidget(m_central_widget);
    setCentralWidget(m_good_central_widget);

    setWindowIcon(QIcon(":/logo.png"));
    setWindowTitle(m_central_widget->windowTitle());

    m_good_central_widget->setTitleAlignment(Qt::AlignCenter);
}

MainWindow::~MainWindow() {
    delete m_central_widget;
}

void MainWindow::closeEvent(QCloseEvent *event) {
    m_central_widget->checkCloseEvent(event);
}

bool MainWindow::event(QEvent * event) {
    if(event->type() == QEvent::StatusTip) {
        m_central_widget->checkStatusTipEvent(static_cast<QStatusTipEvent *>(event));
        return true;
    }
    return QGoodWindow::event(event);
}
