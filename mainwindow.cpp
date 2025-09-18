#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "qcustomplot.h"
#include "netsetting.h"
#include "paramsetting.h"
#include "globalsettings.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    emit sigUpdateBootInfo(tr("加载界面..."));

    initUi();
    initCustomPlot(ui->customPlot, tr("道址"), tr("实测曲线（通道1-4）"), 4);
    initCustomPlot(ui->customPlot_2, tr("道址"), tr("实测曲线（通道5-8）"), 4);
    initCustomPlot(ui->customPlot_3, tr("道址"), tr("实测曲线（通道9-11）"), 3);
    initCustomPlot(ui->customPlot_result, tr("能量/MeV"), tr("反解能谱）"));

    connect(this, SIGNAL(sigWriteLog(const QString&,QtMsgType)), this, SLOT(slotWriteLog(const QString&,QtMsgType)));

    commHelper = CommHelper::instance();
    connect(commHelper, &CommHelper::showRealCurve, this, &MainWindow::showRealCurve);
    connect(commHelper, &CommHelper::showEnerygySpectrumCurve, this, &MainWindow::showEnerygySpectrumCurve);
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

        if (ui->tableWidget_detector->item(0, 0)->text() == "在线" &&
            ui->tableWidget_detector->item(0, 1)->text() == "在线" &&
            ui->tableWidget_detector->item(0, 2)->text() == "在线"){
            ui->action_connect->setEnabled(false);
            ui->action_disconnect->setEnabled(true);
        }

    });
    connect(commHelper, &CommHelper::detectorDisconnected, this, [=](quint8 index){
        ui->tableWidget_detector->item(0, index - 1)->setText(tr("离线"));
        ui->tableWidget_detector->item(0, index - 1)->setForeground(QBrush(QColor(Qt::red)));
        if (ui->tableWidget_detector->item(0, 0)->text() == "离线" &&
            ui->tableWidget_detector->item(0, 1)->text() == "离线" &&
            ui->tableWidget_detector->item(0, 2)->text() == "离线"){
            ui->action_startMeasure->setEnabled(false);
            ui->action_stopMeasure->setEnabled(false);

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
    });
    //测量结束
    connect(commHelper, &CommHelper::measureEnd, this, [=](quint8 index){
        ui->action_startMeasure->setEnabled(true);
        ui->action_stopMeasure->setEnabled(false);
    });
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::initUi()
{
    ui->stackedWidget->hide();

    // 任务栏信息
    QLabel *label_Idle = new QLabel(ui->statusbar);
    label_Idle->setObjectName("label_Idle");
    label_Idle->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    label_Idle->setFixedWidth(300);
    label_Idle->setText(tr("就绪"));

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
    splitter->setHandleWidth(1);
    ui->centralwidget->layout()->addWidget(splitter);
    splitter->addWidget(ui->stackedWidget);
    splitter->addWidget(ui->leftHboxWidget);
    splitter->addWidget(ui->rightHboxWidget);
    splitter->setSizes(QList<int>() << 100000 << 100000 << 400000);
    splitter->setCollapsible(0,false);
    splitter->setCollapsible(1,false);
    splitter->setCollapsible(2,false);

    QSplitter *splitterV1 = new QSplitter(Qt::Vertical,this);
    splitterV1->setHandleWidth(1);
    ui->leftHboxWidget->layout()->addWidget(splitterV1);
    splitterV1->addWidget(ui->customPlot);
    splitterV1->addWidget(ui->customPlot_2);
    splitterV1->addWidget(ui->customPlot_3);
    splitterV1->setCollapsible(0,false);
    splitterV1->setCollapsible(1,false);
    splitterV1->setCollapsible(2,false);
    splitterV1->setSizes(QList<int>() << 100000 << 100000 << 100000);

    QSplitter *splitterV2 = new QSplitter(Qt::Vertical,this);
    splitterV2->setHandleWidth(1);
    ui->rightHboxWidget->layout()->addWidget(splitterV2);
    splitterV2->addWidget(ui->customPlot_result);
    splitterV2->addWidget(ui->textEdit_log);
    splitterV2->setCollapsible(0,false);
    splitterV2->setCollapsible(1,false);
    splitterV2->setSizes(QList<int>() << 400000 << 100000);

    QPushButton* laserDistanceButton = new QPushButton();
    laserDistanceButton->setText(tr("测距模块"));
    laserDistanceButton->setFixedSize(250,26);
    laserDistanceButton->setCheckable(true);
    QPushButton* detectorStatusButton = new QPushButton();
    detectorStatusButton->setText(tr("设备信息"));
    detectorStatusButton->setFixedSize(250,26);
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

    // ui->switchButton_power->setAutoChecked(false);
    // ui->switchButton_laser->setAutoChecked(false);

    QAction *action = ui->lineEdit_filePath->addAction(QIcon(":/open.png"), QLineEdit::TrailingPosition);
    QToolButton* button = qobject_cast<QToolButton*>(action->associatedWidgets().last());
    button->setCursor(QCursor(Qt::PointingHandCursor));
    connect(button, &QToolButton::pressed, this, [=](){
        QString cacheDir = QFileDialog::getExistingDirectory(this);
        if (!cacheDir.isEmpty()){

            JsonSettings* ipSettings = GlobalSettings::instance()->mIpSettings;
            ipSettings->prepare();

            ipSettings->beginGroup("Relay");
            ipSettings->setValue("CacheDir", cacheDir);
            ipSettings->endGroup();
            ipSettings->flush();
            ipSettings->finish();

            ui->lineEdit_filePath->setText(cacheDir);
        }
    });

    {
        JsonSettings* ipSettings = GlobalSettings::instance()->mIpSettings;
        ipSettings->prepare();

        ipSettings->beginGroup("Relay");
        QString cacheDir = ipSettings->value("CacheDir").toString();
        ipSettings->endGroup();
        ipSettings->finish();

        if (cacheDir.isEmpty())
            cacheDir = QApplication::applicationDirPath() + "/cache/";
        ui->lineEdit_filePath->setText(cacheDir);
    }

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

    connect(ui->pushButton_startMeasure, &QPushButton::clicked, ui->action_startMeasure, &QAction::trigger);
}

void MainWindow::initCustomPlot(QCustomPlot* customPlot, QString axisXLabel, QString axisYLabel, int graphCount/* = 1*/)
{
    //customPlot->setObjectName(objName);
    customPlot->installEventFilter(this);

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
    customPlot->xAxis->setRange(0, 1024);
    customPlot->yAxis->setRange(-10, 10);
    customPlot->yAxis->ticker()->setTickCount(5);
    customPlot->xAxis->ticker()->setTickCount(10);

    customPlot->yAxis2->ticker()->setTickCount(5);
    customPlot->xAxis2->ticker()->setTickCount(10);

    //设置轴标签名称
    customPlot->xAxis->setLabel(axisXLabel);
    customPlot->yAxis->setLabel(axisYLabel);

    // 添加散点图
    QColor colors[] = {Qt::red, Qt::blue, Qt::green, Qt::cyan};
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

void MainWindow::closeEvent(QCloseEvent *event) {
    if (mRelayPowerOn){
        int reply = QMessageBox::question(this, tr("系统退出提示"), tr("继电器电源处理闭合状态，是否断开？"),
                                             QMessageBox::Yes|QMessageBox::No, QMessageBox::Yes);
        if(reply == QMessageBox::Yes) {
            commHelper->closePower();
        }
    }

    event->accept();
}

bool MainWindow::event(QEvent * event) {
    if(event->type() == QEvent::StatusTip) {
        QStatusTipEvent* statusTipEvent = static_cast<QStatusTipEvent *>(event);
        if (!statusTipEvent->tip().isEmpty()) {
            ui->statusbar->showMessage(statusTipEvent->tip(), 2000);
        }

        return true;
    }

    return QMainWindow::event(event);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event){
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

void MainWindow::slotWriteLog(const QString &msg, QtMsgType msgType)
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

void MainWindow::on_action_netCfg_triggered()
{
    NetSetting w;
    w.exec();
}


void MainWindow::on_action_cfgParam_triggered()
{
    ParamSetting w;
    w.exec();
}


void MainWindow::on_action_exit_triggered()
{
    this->close();
}

void MainWindow::on_action_open_triggered()
{
    // 打开历史测量数据文件...
    QString filter = "二进制文件 (*.dat);;文本文件 (*.csv);;所有文件 (*.dat *.csv)";
    QString filePath = QFileDialog::getOpenFileName(this, tr("打开测量数据文件"),QDir::homePath(),filter);

    if (filePath.isEmpty() || !QFileInfo::exists(filePath))
        return;

    commHelper->openHistoryWaveFile(filePath);
}

void MainWindow::on_action_connectRelay_triggered()
{
    // 连接网络
    commHelper->connectNet();
}


void MainWindow::on_action_disconnectRelay_triggered()
{
    // 断开网络
    commHelper->disconnectNet();
}


void MainWindow::on_action_connect_triggered()
{
    // 打开探测器
    commHelper->connectDetectors();
}


void MainWindow::on_action_disconnect_triggered()
{
    // 关闭探测器
    commHelper->disconnectDetectors();
}


void MainWindow::on_action_startMeasure_triggered()
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
    QString shotNum = ui->lineEdit_shotNum->text();
    commHelper->setShotInformation(shotDir, shotNum);

    // 再发开始测量指令
    commHelper->startMeasure(CommHelper::TriggerMode::tmSoft);
}


void MainWindow::on_action_stopMeasure_triggered()
{
    // 停止波形测量
    commHelper->stopMeasure();
}


void MainWindow::on_action_powerOn_triggered()
{
    // 打开电源
    commHelper->openPower();
}


void MainWindow::on_action_powerOff_triggered()
{
    // 关闭电源
    commHelper->closePower();
}

void MainWindow::on_pushButton_startMeasureDistance_clicked()
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

void MainWindow::on_pushButton_stopMeasureDistance_clicked()
{
    // 停止测距
    ui->pushButton_startMeasureDistance->setEnabled(true);
    ui->pushButton_stopMeasureDistance->setEnabled(false);

    commHelper->stopMeasureDistance();

    // 测距完成之后，激光会自动关闭，所以这里把按钮状态同步更新一下
    ui->switchButton_laser->setChecked(false);
}

void MainWindow::showRealCurve(const QMap<quint8, QVector<quint16>>& data)
{
    //实测曲线
    QVector<double> keys, values;
    for (int ch=1; ch<=4; ++ch){
        keys.clear();
        values.clear();
        QVector<quint16> chData = data[ch];
        if (chData.size() > 0){
            for (int i=0; i<chData.size(); ++i){
                keys << i;
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
                keys << i;
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
                keys << i;
                values << chData[i];
            }
            ui->customPlot_3->graph(ch - 9)->setData(keys, values);
        }
    }
    ui->customPlot_3->xAxis->rescale(true);
    ui->customPlot_3->yAxis->rescale(true);
    ui->customPlot_3->replot(QCustomPlot::rpQueuedReplot);
}

void MainWindow::showEnerygySpectrumCurve(const QVector<QPair<float, float>>& data)
{
    //反解能谱
    QVector<double> keys, values;
    for (auto iter = data.begin(); iter != data.end(); ++iter){
        keys << iter->first;
        values << iter->second;
    }
    ui->customPlot_result->graph(0)->setData(keys, values);
    ui->customPlot_result->replot(QCustomPlot::rpQueuedReplot);
}


#include "aboutwidget.h"
void MainWindow::on_action_about_triggered()
{
    AboutWidget *w = new AboutWidget(this);
    w->setAttribute(Qt::WA_DeleteOnClose, true);
    w->setWindowFlags(Qt::WindowCloseButtonHint|Qt::Dialog);
    w->setWindowModality(Qt::ApplicationModal);
    w->showNormal();
}


void MainWindow::on_pushButton_export_clicked()
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


void MainWindow::on_pushButton_clicked()
{
    ui->tableWidget_laser->clearContents();
    ui->tableWidget_laser->setRowCount(0);
}


void MainWindow::on_action_exportImg_triggered()
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

