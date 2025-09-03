#include "globalsettings.h"
#include <QFileInfo>

/*#########################################################*/
GlobalSettings::GlobalSettings() {
    mFpgaSettings = new JsonSettings("./config/fpga.json");
    mIpSettings = new JsonSettings("./config/ip.json");

    if (mWatchThisFile){
        // 监视文件内容变化，一旦发现变化重新读取配置文件内容，保持配置信息同步
        mConfigurationFileWatch = new QFileSystemWatcher();
        QFileInfo mConfigurationFile;
        mConfigurationFile.setFile(mFpgaSettings->fileName());
        if (!mConfigurationFileWatch->files().contains(mConfigurationFile.absoluteFilePath()))
            mConfigurationFileWatch->addPath(mConfigurationFile.absoluteFilePath());

        mConfigurationFile.setFile(mIpSettings->fileName());
        if (!mConfigurationFileWatch->files().contains(mConfigurationFile.absoluteFilePath()))
            mConfigurationFileWatch->addPath(mConfigurationFile.absoluteFilePath());

        connect(mConfigurationFileWatch, &QFileSystemWatcher::fileChanged, this, [=](const QString &fileName){
            if (fileName == mFpgaSettings->fileName())
                mFpgaSettings->load();
            else if (fileName == mIpSettings->fileName())
                mIpSettings->load();
        });

        //mConfigurationFileWatch->addPath(mConfigurationFile.absolutePath());//只需要监视某个文件即可，这里不需要监视整个目录
        // connect(mConfigurationFileWatch, &QFileSystemWatcher::directoryChanged, [=](const QString &path){
        // });

        // 链接信号槽，软件自身对配置文件所做的修改，就不用重新读取配置文件了
        std::function<void(const QString &)> onPrepare = [=](const QString &fileName) {
            mConfigurationFileWatch->removePath(fileName);
        };
        std::function<void(const QString &)> onFinish = [=](const QString &fileName) {
            mConfigurationFileWatch->addPath(fileName);
        };

        connect(mFpgaSettings, &JsonSettings::sigPrepare, this, onPrepare);
        connect(mFpgaSettings, &JsonSettings::sigFinish, this, onFinish);
        connect(mIpSettings, &JsonSettings::sigPrepare, this, onPrepare);
        connect(mIpSettings, &JsonSettings::sigFinish, this, onFinish);
    }
}

GlobalSettings::~GlobalSettings() {
    delete mFpgaSettings;
    delete mIpSettings;

    delete mConfigurationFileWatch;
    mConfigurationFileWatch = nullptr;
}
