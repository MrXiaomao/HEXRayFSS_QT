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
    ui->tableWidget->setColumnWidth(1, 50);
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);


    GlobalSettings settings("./Settings.ini");
    ui->tableWidget->item(0, 0)->setText(settings.value("Relay/ip").toString());
    ui->tableWidget->item(0, 1)->setText(settings.value("Relay/port").toString());

    for (int i=1; i<=3; ++i){
        ui->tableWidget->item(i, 0)->setText(settings.value(QString("Detector/%1/ip").arg(i)).toString());
        ui->tableWidget->item(i, 1)->setText(settings.value(QString("Detector/%1/port").arg(i)).toString());
    }
}

NetSetting::~NetSetting()
{
    delete ui;
}

void NetSetting::on_pushButton_save_clicked()
{
    GlobalSettings settings("./Settings.ini");
    settings.setValue("Relay/ip", ui->tableWidget->item(0, 0)->text());
    settings.setValue("Relay/port", ui->tableWidget->item(0, 1)->text());

    for (int i=1; i<=3; ++i){
        settings.setValue(QString("Detector/%1/ip").arg(i), ui->tableWidget->item(i, 0)->text());
        settings.setValue(QString("Detector/%1/port").arg(i), ui->tableWidget->item(i, 1)->text());
    }

    this->close();
}


void NetSetting::on_pushButton_cancel_clicked()
{
    this->close();
}

