#include "aboutwidget.h"
#include "ui_aboutwidget.h"

AboutWidget::AboutWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::AboutWidget)
{
    ui->setupUi(this);

    ui->label_3->setText(tr("Commit: %1").arg(APP_VERSION));
}

AboutWidget::~AboutWidget()
{
    delete ui;
}

void AboutWidget::on_pushButton_ok_clicked()
{
    this->close();
}
