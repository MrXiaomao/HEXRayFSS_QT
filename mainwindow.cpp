#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "qcustomplot.h"
#include "netsetting.h"
#include "paramsetting.h"
#include "globalsettings.h"

CentralWidget::CentralWidget(bool isDarkTheme, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::CentralWidget)
    , mIsDarkTheme(isDarkTheme)
    , mainWindow(static_cast<MainWindow *>(parent))
{
    ui->setupUi(this);    
    emit sigUpdateBootInfo(tr("加载界面..."));

    //ui->toolBar->setVisible(true);
    setWindowTitle(QApplication::applicationName()+" - "+APP_VERSION);

    initUi();
    initCustomPlot(ui->customPlot, tr("时间/ns"), tr("实测曲线（通道1-4）"), 4);
    initCustomPlot(ui->customPlot_2, tr("时间/ns"), tr("实测曲线（通道5-8）"), 4);
    initCustomPlot(ui->customPlot_3, tr("时间/ns"), tr("实测曲线（通道9-11）"), 3);
    initCustomPlot(ui->customPlot_result, tr("能量/keV"), tr("反解能谱"));
    restoreSettings();
    applyColorTheme();

    connect(this, SIGNAL(sigWriteLog(const QString&,QtMsgType)), this, SLOT(slotWriteLog(const QString&,QtMsgType)));

    commHelper = CommHelper::instance();
    connect(commHelper, &CommHelper::showRealCurve, this, &CentralWidget::showRealCurve);
    connect(commHelper, &CommHelper::showEnerygySpectrumCurve, this, &CentralWidget::showEnerygySpectrumCurve);
    connect(commHelper, &CommHelper::exportEnergyPlot, this, [=](const QString fileDir){
        QString filePath = QString("%1/测量数据/1-4.png").arg(fileDir);
        ui->customPlot->savePng(filePath, 1920, 1080);

        filePath = QString("%1/测量数据/4-8.png").arg(fileDir);
        ui->customPlot_2->savePng(filePath, 1920, 1080);

        filePath = QString("%1/测量数据/9-11.png").arg(fileDir);
        ui->customPlot_3->savePng(filePath, 1920, 1080);

        filePath = QString("%1/处理数据/反解能谱.png").arg(fileDir);
        ui->customPlot_result->savePng(filePath, 1920, 1080);
    });
    connect(commHelper, &CommHelper::appVersionRespond, this, [=](quint8 index, QString version, QString serialNumber){
        ui->tableWidget_detectorVersion->item(index - 1, 0)->setText(version + serialNumber);

        // 测量结束，可以开始温度查询了
        commHelper->queryTemperature(index);
    });

    ui->action_connectRelay->setEnabled(true);
    ui->action_disconnectRelay->setEnabled(false);
    ui->action_powerOn->setEnabled(false);
    ui->action_powerOff->setEnabled(false);
    ui->action_connect->setEnabled(false);
    ui->action_disconnect->setEnabled(false);
    ui->action_startMeasure->setEnabled(false);
    ui->action_stopMeasure->setEnabled(false);
    ui->pushButton_startMeasure->setEnabled(false);
    ui->pushButton_stopMeasure->setEnabled(false);

    // 继电器
    connect(commHelper, &CommHelper::relayConnected, this, [=](){
        QLabel* label_Connected = this->findChild<QLabel*>("label_Connected");
        label_Connected->setStyleSheet("color:green;");
        label_Connected->setText(tr("继电器网络状态：已连接"));

        ui->action_connectRelay->setEnabled(false);
        ui->action_disconnectRelay->setEnabled(true);

        ui->action_powerOn->setEnabled(true);
        ui->action_powerOff->setEnabled(true);

        ui->action_connect->setEnabled(false);
        ui->action_disconnect->setEnabled(false);

        ui->switchButton_power->setEnabled(true);
        ui->switchButton_laser->setEnabled(true);

        //查询继电器电源闭合状态
        commHelper->queryRelayStatus();
    });
    connect(commHelper, &CommHelper::relayDisconnected, this, [=](){
        QLabel* label_Connected = this->findChild<QLabel*>("label_Connected");
        label_Connected->setStyleSheet("color:red;");
        label_Connected->setText(tr("继电器网络状态：断网"));

        ui->action_connectRelay->setEnabled(true);
        ui->action_disconnectRelay->setEnabled(false);

        ui->action_powerOn->setEnabled(false);
        ui->action_powerOff->setEnabled(false);

        ui->action_connect->setEnabled(false);
        ui->action_disconnect->setEnabled(false);

        ui->switchButton_power->setEnabled(false);
        ui->switchButton_laser->setEnabled(false);
    });

    connect(commHelper, &CommHelper::relayPowerOn, this, [=](){
        ui->action_powerOn->setEnabled(false);
        ui->action_powerOff->setEnabled(true);

        ui->action_connect->setEnabled(true);
        ui->action_disconnect->setEnabled(false);

        mRelayPowerOn = true;
    });
    connect(commHelper, &CommHelper::relayPowerOff, this, [=](){
        ui->action_powerOn->setEnabled(true);
        ui->action_powerOff->setEnabled(false);

        ui->action_connect->setEnabled(false);
        ui->action_disconnect->setEnabled(false);

        mRelayPowerOn = false;
    });

    // 探测器
    connect(commHelper, &CommHelper::detectorConnected, this, [=](quint8 index){
        ui->tableWidget_detector->item(0, index - 1)->setText(tr("在线"));
        ui->tableWidget_detector->item(0, index - 1)->setForeground(QBrush(QColor(Qt::green)));
        ui->action_startMeasure->setEnabled(true);
        ui->action_stopMeasure->setEnabled(true);
        ui->pushButton_startMeasure->setEnabled(true);
        ui->pushButton_stopMeasure->setEnabled(true);

        if (ui->tableWidget_detector->item(0, 0)->text() == tr("在线") &&
            ui->tableWidget_detector->item(0, 1)->text() == tr("在线") &&
            ui->tableWidget_detector->item(0, 2)->text() == tr("在线")){
            ui->action_connect->setEnabled(false);
            ui->action_disconnect->setEnabled(true);
        }

    });
    connect(commHelper, &CommHelper::detectorDisconnected, this, [=](quint8 index){
        ui->tableWidget_detector->item(0, index - 1)->setText(tr("离线"));
        ui->tableWidget_detector->item(0, index - 1)->setForeground(QBrush(QColor(Qt::red)));
        if (ui->tableWidget_detector->item(0, 0)->text() == tr("离线") &&
            ui->tableWidget_detector->item(0, 1)->text() == tr("离线") &&
            ui->tableWidget_detector->item(0, 2)->text() == tr("离线")){
            ui->action_startMeasure->setEnabled(false);
            ui->action_stopMeasure->setEnabled(false);
            ui->pushButton_startMeasure->setEnabled(false);
            ui->pushButton_stopMeasure->setEnabled(false);

            ui->action_connect->setEnabled(true);
            ui->action_disconnect->setEnabled(false);
        }
    });
    connect(commHelper, &CommHelper::temperatureRespond, this, [=](quint8 index, float temperature){
        ui->tableWidget_detector->item(1, index - 1)->setText(QString::number(temperature, 'f', 2));
    });

    // 测距模块距离和质量    
    connect(commHelper, &CommHelper::distanceRespond, this, [=](float distance, quint16 quality){
        int row = ui->tableWidget_laser->rowCount();
        ui->tableWidget_laser->insertRow(row);

        QTableWidgetItem *item1 = new QTableWidgetItem(QString::number(distance));
        item1->setTextAlignment(Qt::AlignCenter);
        ui->tableWidget_laser->setItem(row, 0, item1);

        QTableWidgetItem *item2 = new QTableWidgetItem(QString::number(quality));
        item2->setTextAlignment(Qt::AlignCenter);
        ui->tableWidget_laser->setItem(row, 1, item2);

        ui->tableWidget_laser->setCurrentItem(item2);
    });


    //测量开始
    connect(commHelper, &CommHelper::measureStart, this, [=](quint8 index){
        ui->action_startMeasure->setEnabled(false);
        ui->action_stopMeasure->setEnabled(true);

        ui->pushButton_startMeasure->setEnabled(false);
        ui->pushButton_stopMeasure->setEnabled(true);
    });
    //测量结束
    connect(commHelper, &CommHelper::measureEnd, this, [=](quint8 index){
        ui->action_startMeasure->setEnabled(true);
        ui->action_stopMeasure->setEnabled(false);

        ui->pushButton_startMeasure->setEnabled(true);
        ui->pushButton_stopMeasure->setEnabled(false);
    });

    connect(ui->statusbar,&QStatusBar::messageChanged,this,[&](const QString &message){
        if(message.isEmpty()) {
            ui->statusbar->showMessage(tr("准备就绪"));
        } else {
            ui->statusbar->showMessage(message);
        }
    });
    QTimer::singleShot(0, this, [&](){
        qGoodStateHolder->setCurrentThemeDark(mIsDarkTheme);
        QGoodWindow::setAppCustomTheme(mIsDarkTheme,this->themeColor); // Must be >96
    });

    QTimer::singleShot(0, this, [&](){
        if(mainWindow) {
            mainWindow->fixMenuBarWidth();
        }
    });
}

CentralWidget::~CentralWidget()
{
    delete ui;
}

void CentralWidget::initUi()
{
    ui->stackedWidget->hide();

    QActionGroup *themeActionGroup = new QActionGroup(this);
    ui->action_lightTheme->setActionGroup(themeActionGroup);
    ui->action_darkTheme->setActionGroup(themeActionGroup);
    ui->action_lightTheme->setChecked(!mIsDarkTheme);
    ui->action_darkTheme->setChecked(mIsDarkTheme);

    // 任务栏信息
    QLabel *label_Idle = new QLabel(ui->statusbar);
    label_Idle->setObjectName("label_Idle");
    label_Idle->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    label_Idle->setFixedWidth(300);
    label_Idle->setText(tr("准备就绪"));
    connect(ui->statusbar,&QStatusBar::messageChanged,this,[&](const QString &message){
        label_Idle->setText(message);
    });

    QLabel *label_Connected = new QLabel(ui->statusbar);
    label_Connected->setObjectName("label_Connected");
    label_Connected->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    label_Connected->setFixedWidth(300);
    label_Connected->setText(tr("继电器网络状态：未连接"));

    /*设置任务栏信息*/
    QLabel *label_systemtime = new QLabel(ui->statusbar);
    label_systemtime->setObjectName("label_systemtime");
    label_systemtime->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    ui->statusbar->setContentsMargins(5, 0, 5, 0);
    ui->statusbar->addWidget(label_Idle);
    ui->statusbar->addWidget(label_Connected);
    ui->statusbar->addWidget(new QLabel(ui->statusbar), 1);
    ui->statusbar->addWidget(nullptr, 1);
    ui->statusbar->addPermanentWidget(label_systemtime);

    QTimer* systemClockTimer = new QTimer(this);
    systemClockTimer->setObjectName("systemClockTimer");
    connect(systemClockTimer, &QTimer::timeout, this, [=](){
        // 获取当前时间
        QDateTime currentDateTime = QDateTime::currentDateTime();

        // 获取星期几的数字（1代表星期日，7代表星期日）
        int dayOfWeekNumber = currentDateTime.date().dayOfWeek();

        // 星期几的中文名称列表
        QStringList dayNames = {
            tr("星期日"), QObject::tr("星期一"), QObject::tr("星期二"), QObject::tr("星期三"), QObject::tr("星期四"), QObject::tr("星期五"), QObject::tr("星期六"), QObject::tr("星期日")
        };

        // 根据数字获取中文名称
        QString dayOfWeekString = dayNames.at(dayOfWeekNumber);
        this->findChild<QLabel*>("label_systemtime")->setText(QString(QObject::tr("系统时间：")) + currentDateTime.toString("yyyy/MM/dd hh:mm:ss ") + dayOfWeekString);
    });
    systemClockTimer->start(900);

    QSplitter *splitter = new QSplitter(Qt::Horizontal,this);
    splitter->setHandleWidth(5);
    ui->centralwidget->layout()->addWidget(splitter);
    splitter->addWidget(ui->stackedWidget);
    splitter->addWidget(ui->leftHboxWidget);
    splitter->addWidget(ui->rightHboxWidget);
    splitter->setSizes(QList<int>() << 100000 << 100000 << 400000);
    splitter->setCollapsible(0,false);
    splitter->setCollapsible(1,false);
    splitter->setCollapsible(2,false);

    QSplitter *splitterV1 = new QSplitter(Qt::Vertical,this);
    splitterV1->setHandleWidth(5);
    ui->leftHboxWidget->layout()->addWidget(splitterV1);
    splitterV1->addWidget(ui->customPlot);
    splitterV1->addWidget(ui->customPlot_2);
    splitterV1->addWidget(ui->customPlot_3);
    splitterV1->setCollapsible(0,false);
    splitterV1->setCollapsible(1,false);
    splitterV1->setCollapsible(2,false);
    splitterV1->setSizes(QList<int>() << 100000 << 100000 << 100000);

    QSplitter *splitterH1 = new QSplitter(Qt::Horizontal,this);
    splitterH1->setHandleWidth(5);
    ui->rightHboxWidget->layout()->addWidget(splitterH1);
    splitterH1->addWidget(ui->customPlot_result);
    splitterH1->addWidget(ui->widget_5);
    splitterH1->setCollapsible(0,false);
    splitterH1->setCollapsible(1,false);
    splitterH1->setSizes(QList<int>() << 400000 << 100000);

    QSplitter *splitterV2 = new QSplitter(Qt::Vertical,this);
    splitterV2->setHandleWidth(5);
    //ui->rightHboxWidget->layout()->addWidget(splitterH1);
    ui->rightHboxWidget->layout()->addWidget(splitterV2);
    // splitterV2->addWidget(ui->customPlot_result);
    // splitterV2->addWidget(ui->widget_5);
    // splitterV2->addWidget(ui->textEdit_log);
    splitterV2->setCollapsible(0,false);
    splitterV2->setCollapsible(1,false);
    splitterV2->setSizes(QList<int>() << 400000 << 100000);

    splitterV2->addWidget(splitterH1);
    splitterV2->addWidget(ui->textEdit_log);
    splitterV2->setCollapsible(0,false);

    QPushButton* laserDistanceButton = new QPushButton();
    laserDistanceButton->setText(tr("测距模块"));
    laserDistanceButton->setFixedSize(250,29);
    laserDistanceButton->setCheckable(true);
    QPushButton* detectorStatusButton = new QPushButton();
    detectorStatusButton->setText(tr("设备信息"));
    detectorStatusButton->setFixedSize(250,29);
    detectorStatusButton->setCheckable(true);

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

    ui->tableWidget_laser->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->tableWidget_detector->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->tableWidget_detectorVersion->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->tableWidget_detector->horizontalHeader()->setFixedHeight(25);
    ui->tableWidget_detector->setRowHeight(0, 30);
    ui->tableWidget_detector->setRowHeight(1, 30);
    ui->tableWidget_detector->setFixedHeight(87);

    ui->tableWidget_detectorVersion->horizontalHeader()->setFixedHeight(25);
    ui->tableWidget_detectorVersion->setRowHeight(0, 30);
    ui->tableWidget_detectorVersion->setRowHeight(1, 30);
    ui->tableWidget_detectorVersion->setRowHeight(2, 30);
    ui->tableWidget_detectorVersion->setFixedHeight(117);

    QAction *action = ui->lineEdit_filePath->addAction(QIcon(":/open.png"), QLineEdit::TrailingPosition);
    QToolButton* button = qobject_cast<QToolButton*>(action->associatedWidgets().last());
    button->setCursor(QCursor(Qt::PointingHandCursor));
    connect(button, &QToolButton::pressed, this, [=](){
        QString cacheDir = QFileDialog::getExistingDirectory(this);
        if (!cacheDir.isEmpty()){
            GlobalSettings settings(CONFIG_FILENAME);
            settings.setValue("Global/CacheDir", cacheDir);
            ui->lineEdit_filePath->setText(cacheDir);
        }
    });

    // 数据保存路径
    {
        GlobalSettings settings(CONFIG_FILENAME);
        QString cacheDir = settings.value("Global/CacheDir").toString();
        if (cacheDir.isEmpty())
            cacheDir = QApplication::applicationDirPath() + "/cache/";
        ui->lineEdit_filePath->setText(cacheDir);

        // 发次
        ui->spinBox_shotNum->setValue(settings.value("Global/ShotNum", 100).toUInt());
    }

    QAction *action2 = ui->ReMatric_Edit->addAction(QIcon(":/open.png"), QLineEdit::TrailingPosition);
    QToolButton* button2 = qobject_cast<QToolButton*>(action2->associatedWidgets().last());
    button2->setCursor(QCursor(Qt::PointingHandCursor));
    connect(button2, &QToolButton::pressed, this, [=](){
        QString filter = "二进制文件 (*.dat);;文本文件 (*.csv);;所有文件 (*.dat *.csv)";
        QString fileName = QFileDialog::getOpenFileName(this, tr("选择响应矩阵文件"),";",filter);
        if (fileName.isEmpty() || !QFileInfo::exists(fileName))
            return;

        ui->ReMatric_Edit->setText(fileName);
    });

    // 反解能谱响应文件
    {
        GlobalSettings settings(CONFIG_FILENAME);
        QString fileName = settings.value("Global/RespondMatrix").toString();
        ui->ReMatric_Edit->setText(fileName);

        // 辐照距离
        ui->lineEdit_irradiationDistance->setText(settings.value("Global/IrradiationDistance", 12.22).toString());
        // 能量区间
        ui->doubleSpinBox_energyLeft->setValue(settings.value("Global/EnergyLeft", 0.20).toFloat());
        ui->doubleSpinBox_energyRight->setValue(settings.value("Global/EnergyRight", 102.10).toFloat());
    }

    QAction *action3 = ui->lineEdit_SaveAsPath->addAction(QIcon(":/open.png"), QLineEdit::TrailingPosition);
    QToolButton* button3 = qobject_cast<QToolButton*>(action3->associatedWidgets().last());
    button3->setCursor(QCursor(Qt::PointingHandCursor));
    connect(button3, &QToolButton::pressed, this, [=](){
        QString saveAsDir = QFileDialog::getExistingDirectory(this);
        if (!saveAsDir.isEmpty()){

            GlobalSettings settings(CONFIG_FILENAME);
            settings.setValue("Global/SaveAsPath", saveAsDir);

            ui->lineEdit_SaveAsPath->setText(saveAsDir);
        }
    });

    // 分析结果
    {
        GlobalSettings settings(CONFIG_FILENAME);
        QString saveAsDir = settings.value("Global/SaveAsPath").toString();
        if (saveAsDir.isEmpty())
            saveAsDir = QApplication::applicationDirPath() + "/cache/";
        ui->lineEdit_SaveAsPath->setText(saveAsDir);

        ui->lineEdit_reverseValue->setText(settings.value("Global/ReverseValue", "1.0%").toString());
        ui->lineEdit_dadiationDose->setText(settings.value("Global/DadiationDose", 1011.0).toString());
        ui->lineEdit_dadiationDoseRate->setText(settings.value("Global/DadiationDoseRate", 30.0).toString());
        ui->lineEdit_SaveAsFileName->setText(settings.value("Global/SaveAsFileName", "test1").toString());
    }

    //ui->switchButton_power->setAutoChecked(false);
    //ui->switchButton_laser->setAutoChecked(false);
    ui->switchButton_power->setEnabled(false);
    ui->switchButton_laser->setEnabled(false);
    ui->pushButton_startMeasureDistance->setEnabled(false);
    ui->pushButton_stopMeasureDistance->setEnabled(false);
    connect(ui->switchButton_power, &SwitchButton::toggled, this, [=](bool checked){
        if (checked){
            // 测距模块电源
            commHelper->openDistanceModulePower();
        }
        else{
            commHelper->closeDistanceModulePower();
        }
    });
    connect(ui->switchButton_laser, &SwitchButton::toggled, this, [=](bool checked){
        if (checked){
            // 测距模块激光
            commHelper->openDistanceModuleLaser();
        }
        else{
            commHelper->closeDistanceModuleLaser();
        }
    });

    connect(laserDistanceButton,&QPushButton::clicked,this,[=](){
        if(ui->stackedWidget->isHidden()) {
            ui->stackedWidget->setCurrentWidget(ui->laserDistanceWidget);
            ui->stackedWidget->show();

            laserDistanceButton->setChecked(true);
            detectorStatusButton->setChecked(false);
        } else {
            if(ui->stackedWidget->currentWidget() == ui->laserDistanceWidget) {
                ui->stackedWidget->hide();

                laserDistanceButton->setChecked(false);
                detectorStatusButton->setChecked(false);
            } else {
                ui->stackedWidget->setCurrentWidget(ui->laserDistanceWidget);

                laserDistanceButton->setChecked(true);
                detectorStatusButton->setChecked(false);
            }
        }                
    });
    connect(detectorStatusButton,&QPushButton::clicked,this,[=](){
        if(ui->stackedWidget->isHidden()) {
            ui->stackedWidget->setCurrentWidget(ui->detectorStatusWidget);
            ui->stackedWidget->show();

            laserDistanceButton->setChecked(false);
            detectorStatusButton->setChecked(true);
        } else {
            if(ui->stackedWidget->currentWidget() == ui->detectorStatusWidget) {
                ui->stackedWidget->hide();
                laserDistanceButton->setChecked(false);
                detectorStatusButton->setChecked(false);
            } else {
                ui->stackedWidget->setCurrentWidget(ui->detectorStatusWidget);
                laserDistanceButton->setChecked(false);
                detectorStatusButton->setChecked(true);
            }
        }        
    });

    connect(ui->toolButton_closeLaserDistanceWidget,&QPushButton::clicked,this,[=](){
        ui->stackedWidget->hide();
        laserDistanceButton->setChecked(false);
        detectorStatusButton->setChecked(false);
    });
    connect(ui->toolButton_closeDetectorStatusWidget,&QPushButton::clicked,this,[=](){
        ui->stackedWidget->hide();
        laserDistanceButton->setChecked(false);
        detectorStatusButton->setChecked(false);
    });

    detectorStatusButton->clicked();
    connect(ui->pushButton_startMeasure, &QPushButton::clicked, ui->action_startMeasure, &QAction::trigger);
    connect(ui->pushButton_stopMeasure, &QPushButton::clicked, ui->action_stopMeasure, &QAction::trigger);
}

void CentralWidget::initCustomPlot(QCustomPlot* customPlot, QString axisXLabel, QString axisYLabel, int graphCount/* = 1*/)
{
    //customPlot->setObjectName(objName);
    customPlot->installEventFilter(this);

    // 设置背景网格线是否显示
    //customPlot->xAxis->grid()->setVisible(true);
    //customPlot->yAxis->grid()->setVisible(true);
    // 设置背景网格线条颜色
    //customPlot->xAxis->grid()->setPen(QPen(palette.color(QPalette::WindowText),1,Qt::PenStyle::SolidLine));  // 垂直网格线条属性
    //customPlot->yAxis->grid()->setPen(QPen(palette.color(QPalette::WindowText),1,Qt::PenStyle::SolidLine)); // 水平网格线条属性
    //customPlot->xAxis->grid()->setSubGridPen(QPen(palette.color(QPalette::WindowText),1,Qt::PenStyle::DotLine));
    //customPlot->yAxis->grid()->setSubGridPen(QPen(palette.color(QPalette::WindowText),1,Qt::PenStyle::SolidLine));

    // 设置全局抗锯齿
    customPlot->setAntialiasedElements(QCP::aeAll);
    // 图例名称隐藏
    customPlot->legend->setVisible(false);
    customPlot->legend->setFillOrder(QCPLayoutGrid::foColumnsFirst);//设置图例在一行中显示
    // 图例名称显示位置
    customPlot->axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignTop|Qt::AlignRight);
    // 设置边界
    //customPlot->setContentsMargins(0, 0, 0, 0);
    // 设置标签倾斜角度，避免显示不下
    customPlot->xAxis->setTickLabelRotation(-45);
    // 允许拖拽，缩放
    customPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables);
    // 允许轴自适应大小
    customPlot->xAxis->rescale(true);
    customPlot->yAxis->rescale(true);
    // 设置刻度范围
    customPlot->xAxis->setRange(0, 200);
    customPlot->yAxis->setRange(0.0, 10.0);
    customPlot->yAxis->ticker()->setTickCount(5);
    customPlot->xAxis->ticker()->setTickCount(10);

    customPlot->yAxis2->ticker()->setTickCount(5);
    customPlot->xAxis2->ticker()->setTickCount(10);

    //设置轴标签名称
    customPlot->xAxis->setLabel(axisXLabel);
    customPlot->yAxis->setLabel(axisYLabel);

    // 添加散点图
    QColor colors[] = {Qt::green, Qt::blue, Qt::red, Qt::cyan};
    for (int i=0; i<graphCount; ++i){
        QCPGraph * graph = customPlot->addGraph(customPlot->xAxis, customPlot->yAxis);
        graph->setAntialiased(false);
        graph->setPen(QPen(colors[i]));
        graph->selectionDecorator()->setPen(QPen(colors[i]));
        graph->setLineStyle(QCPGraph::lsLine);// 隐藏线性图
        graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, 3));//显示散点图
        graph->setSmooth(true);
    }

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

void CentralWidget::closeEvent(QCloseEvent *event) {
    if (mRelayPowerOn){
        int reply = QMessageBox::question(this, tr("系统退出提示"), tr("继电器电源处理闭合状态，是否断开？"),
                                             QMessageBox::Yes|QMessageBox::No, QMessageBox::Yes);
        if(reply == QMessageBox::Yes) {
            commHelper->closePower();
        }
    }

    event->accept();
}

bool CentralWidget::checkStatusTipEvent(QEvent * event) {
    if(event->type() == QEvent::StatusTip) {
        QStatusTipEvent* statusTipEvent = static_cast<QStatusTipEvent *>(event);
        if (!statusTipEvent->tip().isEmpty()) {
            ui->statusbar->showMessage(statusTipEvent->tip(), 2000);
        }

        return true;
    }

    return false;
}

bool CentralWidget::eventFilter(QObject *watched, QEvent *event){
    if (watched != this){
        if (event->type() == QEvent::MouseButtonPress){
            QMouseEvent *e = reinterpret_cast<QMouseEvent*>(event);
            if (watched->inherits("QCustomPlot")){
                QCustomPlot* customPlot = qobject_cast<QCustomPlot*>(watched);

                if (e->button() == Qt::RightButton) {// 右键恢复
                    QMenu contextMenu(customPlot);
                    int chFrom = 1, chCount = 4;
                    if (customPlot == ui->customPlot)
                        chFrom = 1;
                    else if (customPlot == ui->customPlot_2)
                        chFrom = 5;
                    else if (customPlot == ui->customPlot_3){
                        chFrom = 9;
                        chCount = 3;
                    }
                    else if (customPlot == ui->customPlot_result){
                        chCount = 0;
                    }

                    for (int i=chFrom; i<chFrom + chCount; ++i){
                        QAction *action = contextMenu.addAction(tr("通道#%1").arg(i), this, [=]{
                            QAction* action = qobject_cast<QAction*>(sender());
                            int index = action->data().toUInt();
                            customPlot->graph(index)->setVisible(!customPlot->graph(index)->visible());
                            customPlot->replot(QCustomPlot::rpQueuedReplot);
                        });
                        action->setData(i-chFrom);
                        action->setCheckable(true);
                        if (customPlot->graph(i-chFrom)->visible())
                            action->setChecked(true);
                    }

                    if (chCount != 0)
                        contextMenu.addSeparator();

                    contextMenu.addAction(tr("恢复视图"), this, [=]{
                        customPlot->xAxis->rescale(true);
                        customPlot->yAxis->rescale(true);
                        customPlot->replot(QCustomPlot::rpQueuedReplot);
                    });
                    contextMenu.addAction(tr("导出图像..."), this, [=]{
                        QString filePath = QFileDialog::getSaveFileName(this);
                        if (!filePath.isEmpty()){
                            if (!filePath.endsWith(".png"))
                                filePath += ".png";
                            if (!customPlot->savePng(filePath, 1920, 1080))
                                QMessageBox::information(this, tr("提示"), tr("导出失败！"));
                        }
                    });
                    contextMenu.exec(QCursor::pos());

                    //释放内存
                    QList<QAction*> list = contextMenu.actions();
                    foreach (QAction* action, list) delete action;
                }
            }
        }
    }

    return QMainWindow::eventFilter(watched, event);
}

void CentralWidget::slotWriteLog(const QString &msg, QtMsgType msgType)
{
#if 0
    // 创建一个 QTextCursor
    QTextCursor cursor = ui->textEdit_log->textCursor();
    // 将光标移动到文本末尾
    cursor.movePosition(QTextCursor::End);

    // 先插入时间
    QString color = "black";
    if (mIsDarkTheme)
        color = "white";
    cursor.insertHtml(QString("<span style='color:%1;'>%2</span>").arg(color, QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz >> ")));
    // 再插入文本
    if (msgType == QtDebugMsg || msgType == QtInfoMsg)
        cursor.insertHtml(QString("<span style='color:%1;'>%2</span>").arg(color, msg));
    else if (msgType == QtCriticalMsg || msgType == QtFatalMsg)
        cursor.insertHtml(QString("<span style='color:red;'>%1</span>").arg(msg));
    else
        cursor.insertHtml(QString("<span style='color:green;'>%1</span>").arg(msg));

    // 最后插入换行符
    cursor.insertHtml("<br>");

    // 确保 QTextEdit 显示了光标的新位置
    ui->textEdit_log->setTextCursor(cursor);
#else
    ui->textEdit_log->append(QString("%1 %2").arg(QDateTime::currentDateTime().toString("[yyyy-MM-dd hh:mm:ss.zzz]"), msg));
#endif

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
    //qApp->quit();
    mainWindow->close();
}

void CentralWidget::on_action_open_triggered()
{
    // 打开历史测量数据文件...
    GlobalSettings settings;
    QString lastPath = settings.value("Global/LastFilePath", QDir::homePath()).toString();
    QString filter = "二进制文件 (*.dat);;文本文件 (*.csv);;所有文件 (*.dat *.csv)";
    QString filePath = QFileDialog::getOpenFileName(this, tr("打开测量数据文件"), QDir::homePath(), filter);

    if (filePath.isEmpty() || !QFileInfo::exists(filePath))
        return;

    settings.setValue("Global/LastFilePath", filePath);
    if (!commHelper->openHistoryWaveFile(filePath))
    {
        QMessageBox::information(this, tr("提示"), tr("文件格式错误，加载失败！"));
    }
}

void CentralWidget::on_action_readXRD_triggered()
{
    // 打开XRD数据文件...
    // 读取上次使用的路径，如果没有则使用文档路径
    GlobalSettings settings;
    QString lastPath = settings.value("Global/LastFilePath", QDir::homePath()).toString();
    QString filter = "XRD数据文件 (*.csv);;所有文件 (*.csv)";
    QString filePath = QFileDialog::getOpenFileName(this, tr("打开XRD数据文件"), lastPath, filter);

    if (filePath.isEmpty() || !QFileInfo::exists(filePath))
        return;

    settings.setValue("Global/LastFilePath", filePath);
    QVector<QPair<double, double>> data;
    if (openXRDFile(filePath, data))
    {
        showEnerygySpectrumCurve(data);
    }
    else
    {
        QMessageBox::information(this, tr("提示"), tr("文件格式错误，加载失败！"));
    }
}

void CentralWidget::on_action_exportImg_triggered()
{
    // 导出图像
    QString filePath = QFileDialog::getSaveFileName(this);
    if (!filePath.isEmpty()){
        if (!filePath.endsWith(".png"))
            filePath += ".png";
        if (!ui->customPlot_result->savePng(filePath, 1920, 1080))
            QMessageBox::information(this, tr("提示"), tr("导出失败！"));
    }
}

void CentralWidget::on_action_connectRelay_triggered()
{
    // 连接网络
    commHelper->connectNet();
}


void CentralWidget::on_action_disconnectRelay_triggered()
{
    // 断开网络
    commHelper->disconnectNet();
}


void CentralWidget::on_action_connect_triggered()
{
    // 打开探测器
    commHelper->connectDetectors();
}


void CentralWidget::on_action_disconnect_triggered()
{
    // 关闭探测器
    commHelper->disconnectDetectors();
}


void CentralWidget::on_action_startMeasure_triggered()
{
    QVector<double> keys, values;
    for (int i=0; i<=3; ++i){
        ui->customPlot->graph(i)->data()->clear();
        ui->customPlot_2->graph(i)->data()->clear();
        if (i==3)
            break;
        ui->customPlot_3->graph(i)->data()->clear();
    }

    ui->customPlot->replot();
    ui->customPlot_2->replot();
    ui->customPlot_3->replot();

    // 先发温度停止指令
    commHelper->queryTemperature(1, false);
    commHelper->queryTemperature(2, false);
    commHelper->queryTemperature(3, false);

    /*设置发次信息*/
    QString shotDir = ui->lineEdit_filePath->text();
    quint32 shotNum = ui->spinBox_shotNum->value();

    // 保存测量数据
    QString savePath = QString(tr("%1/%2/测量数据")).arg(shotDir).arg(shotNum);
    QDir dir(QString(tr("%1/%2/测量数据")).arg(shotDir).arg(shotNum));
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    dir.setPath(QString(tr("%1/%2/处理数据")).arg(shotDir).arg(shotNum));
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    {
        GlobalSettings settings(QString("%1/Settings.ini").arg(savePath));
        settings.setValue("Global/ShotNum", ui->spinBox_shotNum->value());
        settings.setValue("Global/ResponceMatrix", ui->ReMatric_Edit->text());
        settings.setValue("Global/IrradiationDistance", ui->lineEdit_irradiationDistance->text());
        settings.setValue("Global/EnergyLeft", ui->doubleSpinBox_energyLeft->text());
        settings.setValue("Global/EnergyRight", ui->doubleSpinBox_energyRight->text());
    }

    // 保存界面参数
    if (ui->checkBox_autoIncrease->isChecked()){
        ui->spinBox_shotNum->setValue(ui->spinBox_shotNum->value() + 1);
    }
    {
        GlobalSettings settings(CONFIG_FILENAME);
        settings.setValue("Global/ShotNum", ui->spinBox_shotNum->value());
        settings.setValue("Global/CacheDir", ui->lineEdit_filePath->text());
        settings.setValue("Global/ResponceMatrix", ui->ReMatric_Edit->text());
        settings.setValue("Global/IrradiationDistance", ui->lineEdit_irradiationDistance->text());
        settings.setValue("Global/EnergyLeft", ui->doubleSpinBox_energyLeft->text());
        settings.setValue("Global/EnergyRight", ui->doubleSpinBox_energyRight->text());                                              
    }

    commHelper->setShotInformation(shotDir, shotNum);
    commHelper->setResultInformation(ui->lineEdit_reverseValue->text(),
                                   ui->lineEdit_dadiationDose->text(),
                                   ui->lineEdit_dadiationDoseRate->text());

    // 再发开始测量指令
    commHelper->startMeasure(CommHelper::TriggerMode::tmSoft);
}


void CentralWidget::on_action_stopMeasure_triggered()
{
    // 停止波形测量
    commHelper->stopMeasure();
}


void CentralWidget::on_action_powerOn_triggered()
{
    // 打开电源
    commHelper->openPower();
}


void CentralWidget::on_action_powerOff_triggered()
{
    // 关闭电源
    commHelper->closePower();
}

void CentralWidget::on_pushButton_startMeasureDistance_clicked()
{
    // 开始测距
    if (ui->checkBox_continueMeasureDistance->isChecked()){
        ui->pushButton_startMeasureDistance->setEnabled(false);
        ui->pushButton_stopMeasureDistance->setEnabled(true);
    }

    commHelper->startMeasureDistance(ui->checkBox_continueMeasureDistance->isChecked());

    // 非连续测量，测量完成之后，激光会自动关闭，所以这里把按钮状态同步更新一下
    if (!ui->checkBox_continueMeasureDistance->isChecked())
        ui->switchButton_laser->setChecked(false);
}

void CentralWidget::on_pushButton_stopMeasureDistance_clicked()
{
    // 停止测距
    ui->pushButton_startMeasureDistance->setEnabled(true);
    ui->pushButton_stopMeasureDistance->setEnabled(false);

    commHelper->stopMeasureDistance();

    // 测距完成之后，激光会自动关闭，所以这里把按钮状态同步更新一下
    ui->switchButton_laser->setChecked(false);
}

#define SAMPLE_TIME 10
void CentralWidget::showRealCurve(const QMap<quint8, QVector<quint16>>& data)
{
    //实测曲线
    QVector<double> keys, values;
    for (int ch=1; ch<=4; ++ch){
        keys.clear();
        values.clear();
        QVector<quint16> chData = data[ch];
        if (chData.size() > 0){
            for (int i=0; i<chData.size(); ++i){
                keys << i * SAMPLE_TIME;
                values << chData[i];
            }
            ui->customPlot->graph(ch - 1)->setData(keys, values);
        }
    }
    ui->customPlot->xAxis->rescale(true);
    ui->customPlot->yAxis->rescale(true);
    ui->customPlot->replot(QCustomPlot::rpQueuedReplot);

    keys.clear();
    values.clear();
    for (int ch=5; ch<=8; ++ch){
        keys.clear();
        values.clear();
        QVector<quint16> chData = data[ch];
        if (chData.size() > 0){
            for (int i=0; i<chData.size(); ++i){
                keys << i * SAMPLE_TIME;
                values << chData[i];
            }
            ui->customPlot_2->graph(ch - 5)->setData(keys, values);
        }
    }
    ui->customPlot_2->xAxis->rescale(true);
    ui->customPlot_2->yAxis->rescale(true);
    ui->customPlot_2->replot(QCustomPlot::rpQueuedReplot);

    keys.clear();
    values.clear();
    for (int ch=9; ch<=11; ++ch){
        keys.clear();
        values.clear();
        QVector<quint16> chData = data[ch];
        if (chData.size() > 0){
            for (int i=0; i<chData.size(); ++i){
                keys << i * SAMPLE_TIME;
                values << chData[i];
            }
            ui->customPlot_3->graph(ch - 9)->setData(keys, values);
        }
    }
    ui->customPlot_3->xAxis->rescale(true);
    ui->customPlot_3->yAxis->rescale(true);
    ui->customPlot_3->replot(QCustomPlot::rpQueuedReplot);
}

void CentralWidget::showEnerygySpectrumCurve(const QVector<QPair<double, double>>& data)
{
    //反解能谱
    QVector<double> keys, values;
    for (auto iter = data.begin(); iter != data.end(); ++iter){
        keys << iter->first * 1000;
        values << iter->second;
    }
    ui->customPlot_result->graph(0)->setData(keys, values);
    ui->customPlot_result->xAxis->setRange(1.0, 150.0);
    ui->customPlot_result->yAxis->rescale(true);
    ui->customPlot_result->replot(QCustomPlot::rpQueuedReplot);
}

void CentralWidget::on_action_about_triggered()
{
    QMessageBox::about(this, tr("关于"),
                       QString("<p>") +
                           tr("版本") +
                           QString("</p><span style='color:blue;'>%1</span><p>").arg("LowXRayFSS").arg(APP_VERSION) +
                           tr("提交") +
                           QString("</p><span style='color:blue;'>%1: %2</span><p>").arg(GIT_BRANCH).arg(GIT_HASH) +
                           tr("日期") +
                           QString("</p><span style='color:blue;'>%1</span><p>").arg(GIT_DATE) +
                           tr("开发者") +
                           QString("</p><span style='color:blue;'>MaoXiaoqing</span><p>") +
                           "</p><p>四川大学物理学院 版权所有 (C) 2025</p>"
                       );
}

void CentralWidget::on_action_aboutQt_triggered()
{
    QMessageBox::aboutQt(this);
}

void CentralWidget::on_pushButton_export_clicked()
{
    QString filePath = QFileDialog::getSaveFileName(this);
    if (!filePath.isEmpty()){
        if (!filePath.endsWith(".csv"))
            filePath += ".csv";
        QFile file(filePath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)){
            QTextStream stream(&file);
            stream << ui->tableWidget_laser->horizontalHeaderItem(0)->text() << "," << ui->tableWidget_laser->horizontalHeaderItem(1)->text() << "\n";
            for (int i=0; i<ui->tableWidget_laser->rowCount(); ++i){
                stream << ui->tableWidget_laser->item(i, 0)->text() << "," << ui->tableWidget_laser->item(i, 1)->text() << "\n";
            }

            file.close();
        }
    }
}


void CentralWidget::on_pushButton_clicked()
{
    ui->tableWidget_laser->clearContents();
    ui->tableWidget_laser->setRowCount(0);
}


void CentralWidget::on_action_lightTheme_triggered()
{
    if(!mIsDarkTheme) return;
    mIsDarkTheme = false;
    qGoodStateHolder->setCurrentThemeDark(mIsDarkTheme);
    if(themeColorEnable) QGoodWindow::setAppCustomTheme(mIsDarkTheme,themeColor);
    GlobalSettings settings;
    settings.setValue("Global/Startup/darkTheme","false");
    applyColorTheme();
}


void CentralWidget::on_action_darkTheme_triggered()
{
    if(mIsDarkTheme) return;
    mIsDarkTheme = true;
    qGoodStateHolder->setCurrentThemeDark(mIsDarkTheme);
    if(themeColorEnable) QGoodWindow::setAppCustomTheme(mIsDarkTheme,themeColor);
    GlobalSettings settings;
    settings.setValue("Global/Startup/darkTheme","true");
    applyColorTheme();
}


void CentralWidget::on_action_colorTheme_triggered()
{
    GlobalSettings settings;
    QColor color = QColorDialog::getColor(themeColor, this, tr("选择颜色"));
    if (color.isValid()) {
        themeColor = color;
        themeColorEnable = true;
        qGoodStateHolder->setCurrentThemeDark(mIsDarkTheme);
        QGoodWindow::setAppCustomTheme(mIsDarkTheme,themeColor);
        settings.setValue("Global/Startup/themeColor",themeColor);
    } else {
        themeColorEnable = false;
        qGoodStateHolder->setCurrentThemeDark(mIsDarkTheme);
    }
    settings.setValue("Global/Startup/themeColorEnable",themeColorEnable);
    applyColorTheme();
}

void CentralWidget::applyColorTheme()
{
    QList<QCustomPlot*> customPlots = this->findChildren<QCustomPlot*>();
    for (auto customPlot : customPlots){
        QPalette palette = customPlot->palette();
        if (mIsDarkTheme)
        {
            if (this->themeColorEnable)
            {
                CustomColorDarkStyle darkStyle(themeColorEnable);
                darkStyle.polish(palette);
            }
            else
            {
                DarkStyle darkStyle;
                darkStyle.polish(palette);
            }
        }
        else
        {
            if (this->themeColorEnable)
            {
                CustomColorLightStyle lightStyle(themeColorEnable);
                lightStyle.polish(palette);
            }
            else
            {
                LightStyle lightStyle;
                lightStyle.polish(palette);
            }
        }
        // 窗体背景色
        customPlot->setBackground(QBrush(palette.color(QPalette::Window)));
        // 四边安装轴并显示
        customPlot->axisRect()->setupFullAxesBox();
        customPlot->axisRect()->setBackground(QBrush(palette.color(QPalette::Window)));
        // 坐标轴线颜色
        customPlot->xAxis->setBasePen(QPen(palette.color(QPalette::WindowText)));
        customPlot->xAxis2->setBasePen(QPen(palette.color(QPalette::WindowText)));
        customPlot->yAxis->setBasePen(QPen(palette.color(QPalette::WindowText)));
        customPlot->yAxis2->setBasePen(QPen(palette.color(QPalette::WindowText)));
        // 刻度线颜色
        customPlot->xAxis->setTickPen(QPen(palette.color(QPalette::WindowText)));
        customPlot->xAxis2->setTickPen(QPen(palette.color(QPalette::WindowText)));
        customPlot->yAxis->setTickPen(QPen(palette.color(QPalette::WindowText)));
        customPlot->yAxis2->setTickPen(QPen(palette.color(QPalette::WindowText)));
        // 子刻度线颜色
        customPlot->xAxis->setSubTickPen(QPen(palette.color(QPalette::WindowText)));
        customPlot->xAxis2->setSubTickPen(QPen(palette.color(QPalette::WindowText)));
        customPlot->yAxis->setSubTickPen(QPen(palette.color(QPalette::WindowText)));
        customPlot->yAxis2->setSubTickPen(QPen(palette.color(QPalette::WindowText)));
        // 坐标轴文本标签颜色
        customPlot->xAxis->setLabelColor(palette.color(QPalette::WindowText));
        customPlot->xAxis2->setLabelColor(palette.color(QPalette::WindowText));
        customPlot->yAxis->setLabelColor(palette.color(QPalette::WindowText));
        customPlot->yAxis2->setLabelColor(palette.color(QPalette::WindowText));
        // 坐标轴刻度文本标签颜色
        customPlot->xAxis->setTickLabelColor(palette.color(QPalette::WindowText));
        customPlot->xAxis2->setTickLabelColor(palette.color(QPalette::WindowText));
        customPlot->yAxis->setTickLabelColor(palette.color(QPalette::WindowText));
        customPlot->yAxis2->setTickLabelColor(palette.color(QPalette::WindowText));
        // 坐标系背景色
        customPlot->axisRect()->setBackground(palette.color(QPalette::Window));

        customPlot->replot();
    }
}

void CentralWidget::restoreSettings()
{
    GlobalSettings settings;
    if(mainWindow) {
        mainWindow->restoreGeometry(settings.value("MainWindow/Geometry").toByteArray());
        mainWindow->restoreState(settings.value("MainWindow/State").toByteArray());
    } else {
        restoreGeometry(settings.value("MainWindow/Geometry").toByteArray());
        restoreState(settings.value("MainWindow/State").toByteArray());
    }
    themeColor = settings.value("Global/Startup/themeColor",QColor(30,30,30)).value<QColor>();
    themeColorEnable = settings.value("Global/Startup/themeColorEnable",true).toBool();
    if(themeColorEnable) {
        QTimer::singleShot(0, this, [&](){
            qGoodStateHolder->setCurrentThemeDark(mIsDarkTheme);
            QGoodWindow::setAppCustomTheme(mIsDarkTheme,this->themeColor); // Must be >96
        });
    }
}

#include <QFile>
#include <QTextStream>
#include <QVector>
bool CentralWidget::openXRDFile(const QString &filename, QVector<QPair<double, double>>& data){
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "无法打开文件:" << filename;
        return false;
    }

    QTextStream in(&file);
    // 可选：设置编码
    in.setCodec("UTF-8");

    int lineNumber = 0;
    while (!in.atEnd()) {
        QString line = in.readLine();
        lineNumber++;

        // 跳过空行和注释行
        if (line.trimmed().isEmpty() || line.startsWith('#')) {
            continue;
        }

        // 分割CSV行（支持逗号或分号分隔）
        QStringList parts = line.split(',', Qt::SkipEmptyParts);
        if (parts.size() < 2) {
            parts = line.split(';', Qt::SkipEmptyParts);
        }

        if (parts.size() < 2) {
            qDebug() << "第" << lineNumber << "行数据列数不足，跳过";
            continue;
        }

        // 转换为double
        bool ok1, ok2;
        double value1 = 1000.0*parts[0].trimmed().toDouble(&ok1); //MeV转化为keV
        double value2 = parts[1].trimmed().toDouble(&ok2);

        if (ok1 && ok2) {
            data.append(QPair<double, double>(value1, value2));
        } else {
            qDebug() << "第" << lineNumber << "行数据转换失败:" << line;
        }
    }

    file.close();
    return true;
}

void CentralWidget::on_pushButton_saveAs_clicked()
{
    QString strSavePath = QString("%1/%2").arg(ui->lineEdit_SaveAsPath->text(), ui->lineEdit_SaveAsFileName->text());
    if (commHelper->saveAs(strSavePath))
    {
        GlobalSettings settings(CONFIG_FILENAME);
        settings.setValue("Global/ReverseValue", ui->lineEdit_reverseValue->text());
        settings.setValue("Global/DadiationDose", ui->lineEdit_dadiationDose->text());
        settings.setValue("Global/DadiationDoseRate", ui->lineEdit_dadiationDoseRate->text());
        settings.setValue("Global/SaveAsFileName", ui->lineEdit_SaveAsFileName->text());
        settings.setValue("Global/SaveAsPath", ui->lineEdit_SaveAsPath->text());
        QMessageBox::information(this, tr("提示"), tr("保存成功！"));
    }
    else
    {
        QMessageBox::information(this, tr("提示"), tr("保存失败！"));
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////
MainWindow::MainWindow(bool isDarkTheme, QWidget *parent)
    : QGoodWindow(parent) {
    mCentralWidget = new CentralWidget(isDarkTheme, this);
    mCentralWidget->setWindowFlags(Qt::Widget);
    mGoodCentraWidget = new QGoodCentralWidget(this);

#ifdef Q_OS_MAC
    //macOS uses global menu bar
    if(QApplication::testAttribute(Qt::AA_DontUseNativeMenuBar)) {
#else
    if(true) {
#endif
        mMenuBar = mCentralWidget->menuBar();

        //Set font of menu bar
        QFont font = mMenuBar->font();
#ifdef Q_OS_WIN
        font.setFamily("Segoe UI");
#else
        font.setFamily(qApp->font().family());
#endif
        mMenuBar->setFont(font);

        QTimer::singleShot(0, this, [&]{
            const int title_bar_height = mGoodCentraWidget->titleBarHeight();
            mMenuBar->setStyleSheet(QString("QMenuBar {height: %0px;}").arg(title_bar_height));
        });

        connect(mGoodCentraWidget,&QGoodCentralWidget::windowActiveChanged,this, [&](bool active){
            mMenuBar->setEnabled(active);
        });

        mGoodCentraWidget->setLeftTitleBarWidget(mMenuBar);
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
    qGoodStateHolder->setCurrentThemeDark(isDarkTheme);

    mGoodCentraWidget->setCentralWidget(mCentralWidget);
    setCentralWidget(mGoodCentraWidget);

    setWindowIcon(QIcon(":/logo.png"));
    setWindowTitle(mCentralWidget->windowTitle());

    mGoodCentraWidget->setTitleAlignment(Qt::AlignCenter);
}

MainWindow::~MainWindow() {
    delete mCentralWidget;
}

void MainWindow::closeEvent(QCloseEvent *event) {
    mCentralWidget->closeEvent(event);
}

bool MainWindow::event(QEvent * event) {
    if(event->type() == QEvent::StatusTip) {
        mCentralWidget->checkStatusTipEvent(static_cast<QStatusTipEvent *>(event));
        return true;
    }

    return QGoodWindow::event(event);
}

