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
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);

    JsonSettings* ipSettings = GlobalSettings::instance()->mIpSettings;
    ipSettings->prepare();

    ipSettings->beginGroup("Relay");
    ui->tableWidget->item(0, 0)->setText(ipSettings->value("ip").toString());
    ui->tableWidget->item(0, 1)->setText(QString::number(ipSettings->value("port").toInt()));
    ipSettings->endGroup();

    for (int i=1; i<=3; ++i){
        ipSettings->beginGroup(QString("Detector%1").arg(i));
        ui->tableWidget->item(i, 0)->setText(ipSettings->value("ip").toString());
        ui->tableWidget->item(i, 1)->setText(QString::number(ipSettings->value("port").toInt()));
        ipSettings->endGroup();
    }

    ipSettings->finish();
}

NetSetting::~NetSetting()
{
    delete ui;
}

void NetSetting::on_pushButton_save_clicked()
{
    JsonSettings* ipSettings = GlobalSettings::instance()->mIpSettings;
    ipSettings->prepare();

    ipSettings->beginGroup("Relay");
    ipSettings->setValue("ip", ui->tableWidget->item(0, 0)->text());
    ipSettings->setValue("port", ui->tableWidget->item(0, 1)->text());
    ipSettings->endGroup();

    for (int i=1; i<=3; ++i){
        ipSettings->beginGroup(QString("Detector%1").arg(i));
        ipSettings->setValue("ip", ui->tableWidget->item(i, 0)->text());
        ipSettings->setValue("port", ui->tableWidget->item(i, 1)->text());
        ipSettings->endGroup();
    }

    ipSettings->flush();
    ipSettings->finish();
}


void NetSetting::on_pushButton_cancel_clicked()
{
    this->close();
}

