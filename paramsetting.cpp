#include "paramsetting.h"
#include "ui_paramsetting.h"
#include "globalsettings.h"

ParamSetting::ParamSetting(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ParamSetting)
{
    ui->setupUi(this);
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    GlobalSettings settings("./Settings.ini");
    ui->comboBox_triggerMode->setCurrentIndex(settings.value("Fpga/TriggerMode").toInt());
    ui->comboBox_waveLength->setCurrentIndex(settings.value("Fpga/WaveLength").toInt());

    {
        QString triggerThold = settings.value("Fpga/TriggerThold").toString();
        QList<QString> triggerTholds = triggerThold.split(',', Qt::SkipEmptyParts);
        while (triggerTholds.size() < 4)
            triggerTholds.push_back("200");

        ui->tableWidget->item(0, 0)->setText(triggerTholds[0]);
        ui->tableWidget->item(0, 1)->setText(triggerTholds[1]);
        ui->tableWidget->item(0, 2)->setText(triggerTholds[2]);
        ui->tableWidget->item(0, 3)->setText(triggerTholds[3]);
    }

    {
        QString gain = settings.value("Fpga/Gain").toString();
        QList<QString> gains = gain.split(',', Qt::SkipEmptyParts);
        while (gains.size() < 4)
            gains.push_back("5");

        ui->tableWidget->item(1, 0)->setText(gains[0]);
        ui->tableWidget->item(1, 1)->setText(gains[1]);
        ui->tableWidget->item(1, 2)->setText(gains[2]);
        ui->tableWidget->item(1, 3)->setText(gains[3]);
    }
}

ParamSetting::~ParamSetting()
{
    delete ui;
}

void ParamSetting::on_pushButton_save_clicked()
{
    GlobalSettings settings("./Settings.ini");
    settings.setValue("Fpga/TriggerMode", ui->comboBox_triggerMode->currentIndex());
    settings.setValue("Fpga/WaveLength", ui->comboBox_waveLength->currentIndex());

    {
        QString triggerThold = ui->tableWidget->item(0, 0)->text() + "," +
                               ui->tableWidget->item(0, 1)->text() + "," +
                               ui->tableWidget->item(0, 2)->text() + "," +
                               ui->tableWidget->item(0, 3)->text();
        settings.setValue("Fpga/TriggerThold", triggerThold);
    }

    {
        QString gain = ui->tableWidget->item(1, 0)->text() + "," +
                             ui->tableWidget->item(1, 1)->text() + "," +
                             ui->tableWidget->item(1, 2)->text() + "," +
                             ui->tableWidget->item(1, 3)->text();
        settings.setValue("Fpga/Gain", gain);
    }

    this->close();
}


void ParamSetting::on_pushButton_cancel_clicked()
{
    this->close();
}

