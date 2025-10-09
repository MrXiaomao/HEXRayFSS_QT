#include "globalsettings.h"
#include <QFileInfo>
#include <QApplication>
#include <QTextCodec>

/*#########################################################*/
GlobalSettings::GlobalSettings(QObject *parent)
    : QSettings(GLOBAL_CONFIG_FILENAME, QSettings::IniFormat, parent)
{
    this->setIniCodec(QTextCodec::codecForName("utf-8"));
}

GlobalSettings::GlobalSettings(QString filePath, QObject *parent)
    : QSettings(filePath, QSettings::IniFormat, parent)
{
    this->setIniCodec(QTextCodec::codecForName("utf-8"));
}

GlobalSettings::~GlobalSettings() {

}

#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
void GlobalSettings::setValue(QAnyStringView key, const QVariant &value)
#else
void GlobalSettings::setValue(const QString &key, const QVariant &value)
#endif
{
    QSettings::setValue(key, value);
    if(realtime)
        sync();
}

void GlobalSettings::setRealtimeSave(bool realtime)
{
    this->realtime = realtime;
}
