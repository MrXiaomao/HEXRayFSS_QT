#include "netsetting.h"
#include "ui_netsetting.h"
#include "globalsettings.h"

NetSetting::NetSetting(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::NetSetting)
{
    ui->setupUi(this);
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->tableWidget->setColumnWidth(1, 100);
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);


    GlobalSettings settings(CONFIG_FILENAME);
    ui->tableWidget->item(0, 0)->setText(settings.value("Detector/ip").toString());
    ui->tableWidget->item(0, 1)->setText(settings.value("Detector/port").toString());
}

NetSetting::~NetSetting()
{
    delete ui;
}

void NetSetting::on_pushButton_save_clicked()
{
    GlobalSettings settings(CONFIG_FILENAME);
    settings.setValue("Detector/ip", ui->tableWidget->item(0, 0)->text());
    settings.setValue("Detector/port", ui->tableWidget->item(0, 1)->text());

    this->close();
}


void NetSetting::on_pushButton_cancel_clicked()
{
    this->close();
}

