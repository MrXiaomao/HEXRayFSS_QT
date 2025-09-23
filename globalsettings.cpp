#include "globalsettings.h"
#include <QFileInfo>
#include <QApplication>

/*#########################################################*/
GlobalSettings::GlobalSettings(QObject *parent)
    : QSettings(QSettings::IniFormat, QSettings::UserScope,
    QApplication::applicationName(), QApplication::applicationName(), parent)
{

}

GlobalSettings::GlobalSettings(QString filePath, QObject *parent)
    : QSettings(filePath, QSettings::IniFormat, parent)
{

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
