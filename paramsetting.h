#ifndef PARAMSETTING_H
#define PARAMSETTING_H

#include <QDialog>

namespace Ui {
class ParamSetting;
}

class ParamSetting : public QDialog
{
    Q_OBJECT

public:
    explicit ParamSetting(QWidget *parent = nullptr);
    ~ParamSetting();

private slots:
    void on_pushButton_save_clicked();

    void on_pushButton_cancel_clicked();

private:
    Ui::ParamSetting *ui;
};

#endif // PARAMSETTING_H
