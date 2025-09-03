#ifndef NETSETTING_H
#define NETSETTING_H

#include <QDialog>

namespace Ui {
class NetSetting;
}

class NetSetting : public QDialog
{
    Q_OBJECT

public:
    explicit NetSetting(QWidget *parent = nullptr);
    ~NetSetting();

private slots:
    void on_pushButton_save_clicked();

    void on_pushButton_cancel_clicked();

private:
    Ui::NetSetting *ui;
};

#endif // NETSETTING_H
