#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "qcustomplot.h"
#include "netsetting.h"
#include "paramsetting.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    emit sigUpdateBootInfo(tr("系统启动"));

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

    ui->action_powerOn->setEnabled(false);
    ui->action_powerOff->setEnabled(false);
    ui->action_connect->setEnabled(true);
    ui->action_disconnect->setEnabled(false);
    ui->action_startMeasure->setEnabled(false);
    ui->action_stopMeasure->setEnabled(false);

    // 继电器
    connect(commHelper, &CommHelper::relayConnected, this, [=](){
        ui->action_powerOn->setEnabled(true);
        ui->action_powerOff->setEnabled(true);

        ui->action_connect->setEnabled(false);
        ui->action_disconnect->setEnabled(true);

        //查询继电器电源闭合状态
        commHelper->queryRelayStatus();
    });
    connect(commHelper, &CommHelper::relayDisconnected, this, [=](){
        ui->action_powerOn->setEnabled(false);
        ui->action_powerOff->setEnabled(false);

        ui->action_connect->setEnabled(true);
        ui->action_disconnect->setEnabled(false);
    });

    connect(commHelper, &CommHelper::relayPowerOn, this, [=](){
        ui->action_powerOn->setEnabled(false);
        ui->action_powerOff->setEnabled(true);
    });
    connect(commHelper, &CommHelper::relayPowerOff, this, [=](){
        ui->action_powerOn->setEnabled(true);
        ui->action_powerOff->setEnabled(false);
    });

    // 探测器
    connect(commHelper, &CommHelper::detectorConnected, this, [=](quint8 index){
        ui->tableWidget_detector->item(0, index - 1)->setText(tr("在线"));
        ui->action_startMeasure->setEnabled(true);
        ui->action_stopMeasure->setEnabled(true);
    });
    connect(commHelper, &CommHelper::detectorDisconnected, this, [=](quint8 index){
        ui->tableWidget_detector->item(0, index - 1)->setText(tr("离线"));
        if (ui->tableWidget_detector->item(0, 0)->text() == "离线" &&
            ui->tableWidget_detector->item(0, 1)->text() == "离线" &&
            ui->tableWidget_detector->item(0, 2)->text() == "离线"){
            ui->action_startMeasure->setEnabled(false);
            ui->action_stopMeasure->setEnabled(false);
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

    QTimer::singleShot(500, this, [=](){
        this->showMaximized();
    });
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::initUi()
{
    ui->stackedWidget->hide();

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
    QPushButton* detectorStatusButton = new QPushButton();
    detectorStatusButton->setText(tr("设备信息"));
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
            ui->lineEdit_filePath->setText(cacheDir);
        }
    });

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

    connect(ui->toolButton_closeLaserDistanceWidget,&QPushButton::clicked,this,[&](){
        ui->stackedWidget->hide();
    });
    connect(ui->toolButton_closeDetectorStatusWidget,&QPushButton::clicked,this,[&](){
        ui->stackedWidget->hide();
    });

    connect(ui->pushButton_startMeasure, &QPushButton::clicked, ui->action_startMeasure, &QAction::trigger);
}

void MainWindow::initCustomPlot(QCustomPlot* customPlot, QString axisXLabel, QString axisYLabel, int graphCount/* = 1*/)
{
    //customPlot->setObjectName(objName);
    customPlot->installEventFilter(this);

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
    QColor colors[] = {Qt::red, Qt::blue, Qt::green, Qt::cyan};
    for (int i=0; i<graphCount; ++i){
        QCPGraph * graph = customPlot->addGraph(customPlot->xAxis, customPlot->yAxis);
        graph->setAntialiased(false);
        graph->setPen(QPen(colors[i]));
        graph->selectionDecorator()->setPen(QPen(colors[i]));
        graph->setLineStyle(QCPGraph::lsLine);// 隐藏线性图
        graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, 3));//显示散点图
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
    int reply = QMessageBox::information(this, tr("提示"), tr("您确定要退出吗？"),
                                                              QMessageBox::Yes|QMessageBox::No);
    if(reply == QMessageBox::Yes) {
        event->accept();
    } else {
        event->ignore();
    }
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
    // 打开历史文件...
    QString fileName = QFileDialog::getOpenFileName(this, tr("打开文件"),";",tr("测量文件 (*.dat)"));
    if (fileName.isEmpty() || !QFileInfo::exists(fileName))
        return;


}


void MainWindow::on_action_connect_triggered()
{
    // 连接网络
    commHelper->connectNet();
}


void MainWindow::on_action_disconnect_triggered()
{
    // 断开网络
    commHelper->disconnectNet();
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
}

void MainWindow::on_pushButton_stopMeasureDistance_clicked()
{
    // 停止测距
    ui->pushButton_startMeasureDistance->setEnabled(true);
    ui->pushButton_stopMeasureDistance->setEnabled(false);

    commHelper->stopMeasureDistance();
}

void MainWindow::showRealCurve(const QMap<quint8, QVector<quint16>>& data)
{
    //实测曲线
    QVector<double> keys, values;
    for (int ch=1; ch<=4; ++ch){
        QVector<quint16> chData = data[ch];
        for (int i=0; i<chData.size(); ++i){
            keys << i;
            values << chData[i];
        }
        ui->customPlot->graph(ch - 1)->setData(keys, values);
    }
    ui->customPlot->xAxis->rescale(true);
    ui->customPlot->yAxis->rescale(true);
    ui->customPlot->replot(QCustomPlot::rpQueuedReplot);

    keys.clear();
    values.clear();
    for (int ch=5; ch<=8; ++ch){
        QVector<quint16> chData = data[ch];
        for (int i=0; i<chData.size(); ++i){
            keys << i;
            values << chData[i];
        }
        ui->customPlot_2->graph(ch - 5)->setData(keys, values);
    }
    ui->customPlot_2->xAxis->rescale(true);
    ui->customPlot_2->yAxis->rescale(true);
    ui->customPlot_2->replot(QCustomPlot::rpQueuedReplot);

    keys.clear();
    values.clear();
    for (int ch=9; ch<=11; ++ch){
        QVector<quint16> chData = data[ch];
        for (int i=0; i<chData.size(); ++i){
            keys << i;
            values << chData[i];
        }
        ui->customPlot_3->graph(ch - 9)->setData(keys, values);
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
