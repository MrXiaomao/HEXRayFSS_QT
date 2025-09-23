#ifndef GLOBALSETTINGS_H
#define GLOBALSETTINGS_H

#include <QObject>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QMutex>
#include <QReadWriteLock>
#include <QFileInfo>
#include <QFileSystemWatcher>
class JsonSettings : public QObject{
    Q_OBJECT
public:
    JsonSettings(const QString &fileName) {
        QFileInfo mConfigurationFile;
        mConfigurationFile.setFile(fileName);
        mFileName = mConfigurationFile.absoluteFilePath();
        mOpened = this->load();

        if (mWatchThisFile){
            // 监视文件内容变化，一旦发现变化重新读取配置文件内容，保持配置信息同步
            mConfigurationFileWatch = new QFileSystemWatcher();
            QFileInfo mConfigurationFile;
            mConfigurationFile.setFile(mFileName);
            if (!mConfigurationFileWatch->files().contains(mConfigurationFile.absoluteFilePath()))
                mConfigurationFileWatch->addPath(mConfigurationFile.absoluteFilePath());

            connect(mConfigurationFileWatch, &QFileSystemWatcher::fileChanged, this, [=](const QString &fileName){
                this->load();
            });

            //mConfigurationFileWatch->addPath(mConfigurationFile.absolutePath());//只需要监视某个文件即可，这里不需要监视整个目录
            // connect(mConfigurationFileWatch, &QFileSystemWatcher::directoryChanged, [=](const QString &path){
            // });
        }
    };
    ~JsonSettings(){
        if (mConfigurationFileWatch)
        {
            delete mConfigurationFileWatch;
            mConfigurationFileWatch = nullptr;
        }
    };

    bool isOpen() {
        return this->mOpened;
    }

    QString fileName() const{
        return mFileName;
    }

    bool load(){
        //QReadLocker locker(&mRWLock);
        mJsonRoot = QJsonObject();
        mJsonGroup = QJsonObject();
        mPrefix.clear();

        QFile file(mFileName);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QByteArray jsonData = file.readAll();
            file.close();

            QJsonParseError error;
            QJsonDocument mJsonDoc = QJsonDocument::fromJson(jsonData, &error);
            if (error.error == QJsonParseError::NoError) {
                if (mJsonDoc.isObject()) {
                    mJsonRoot = mJsonDoc.object();
                    return true;
                } else {
                    qDebug() << "文件[" << mFileName << "]解析失败！";
                    return false;
                }
            } else{
                qDebug() << "文件[" << mFileName << "]解析失败！" << error.errorString().toUtf8().constData();
                return false;
            }
        } else {
            qDebug() << "文件[" << mFileName << "]打开失败！";

            //是否需要创建一个新的文件
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)){
                file.close();
                return true;
            } else {
                return false;
            }
        }
    };

    bool flush(){
        return save(mFileName);
    };

    /*
        {
            "键key": "值value",
        }
    */
    void setRootValue(const QString &key, const QVariant &value){
        QWriteLocker locker(&mRWLock);
        mJsonRoot[key] = value.toJsonValue();

        if (realtime)
            flush();
    };

    /*
        {
            "groupName":{
                "键key": "值value",
            }
        }
    */
    void setGroupValue(const QString &groupName, const QString &key, const QVariant &value){
        QWriteLocker locker(&mRWLock);
        auto iterator = mJsonRoot.find(groupName);
        if (iterator != mJsonRoot.end()) {
            QJsonValueRef valueGroupRef = iterator.value();
            QJsonObject objGroup = valueGroupRef.toObject();
            objGroup[key] = value.toJsonValue();
            valueGroupRef = objGroup;
        }
        else {
            QJsonObject objGroup;
            objGroup[key] = value.toJsonValue();
            mJsonRoot.insert(groupName, QJsonValue(objGroup));
        }

        if (realtime)
            flush();
    };

    /*
        {
            "groupName":{
                "group2Name":{
                    "键key": "值value",
                }
            }
        }
    */
    void setGroupValue(const QString &groupName, const QString &group2Name, const QString &key, const QVariant &value){
        QWriteLocker locker(&mRWLock);
        auto iterator = mJsonRoot.find(groupName);
        if (iterator != mJsonRoot.end())
        {
            QJsonValueRef valueGroupRef = iterator.value();
            if (valueGroupRef.isObject())
            {
                QJsonObject objGroup = valueGroupRef.toObject();
                auto iterator2 = objGroup.find(group2Name);
                if (iterator2 != objGroup.end())
                {
                    QJsonValueRef valueGroupRef2 = iterator2.value();
                    QJsonObject objGroup2 = valueGroupRef2.toObject();
                    objGroup2[key] = value.toJsonValue();
                    valueGroupRef2 = objGroup2;
                }
                else
                {
                    QJsonObject objGroup2;
                    objGroup2[key] = value.toJsonValue();
                    objGroup.insert(groupName, QJsonValue(objGroup2));
                }

                valueGroupRef = objGroup;

                if (realtime)
                    flush();
            }
        }
        else
        {
            QJsonObject objGroup2;
            objGroup2[key] = value.toJsonValue();

            QJsonObject objGroup;
            objGroup.insert(group2Name, QJsonValue(objGroup2));

            mJsonRoot.insert(groupName, QJsonValue(objGroup));

            if (realtime)
                flush();
        }
    };

    /*
        {
            "groupName":{
                "group2Name":{
                    "group3Name":{
                        "键key": "值value",
                    }
                }
            }
        }
    */
    void setGroupValue(const QString &groupName, const QString &group2Name, const QString &group3Name, const QString &key, const QVariant &value){
        QWriteLocker locker(&mRWLock);
        auto iterator = mJsonRoot.find(groupName);
        if (iterator != mJsonRoot.end())
        {
            QJsonValueRef valueGroupRef = iterator.value();
            if (valueGroupRef.isObject())
            {
                QJsonObject objGroup = valueGroupRef.toObject();
                auto iterator2 = objGroup.find(group2Name);
                if (iterator2 != objGroup.end())
                {
                    QJsonValueRef valueGroup2Ref = iterator2.value();
                    if (valueGroup2Ref.isObject())
                    {
                        QJsonObject objGroup2 = valueGroup2Ref.toObject();
                        auto iterator3 = objGroup2.find(group3Name);
                        if (iterator3 != objGroup2.end())
                        {
                            QJsonValueRef valueGroupRef3 = iterator3.value();
                            QJsonObject objGroup3 = valueGroupRef3.toObject();
                            objGroup3[key] = value.toJsonValue();

                            objGroup2.insert(group3Name, objGroup3);//valueGroupRef3 = objGroup2;
                        }
                        else
                        {
                            QJsonObject objGroup3;
                            objGroup3[key] = value.toJsonValue();

                            objGroup2.insert(group3Name, objGroup3);
                        }

                        valueGroup2Ref = objGroup2;

                        if (realtime)
                            flush();
                    }
                }
                else
                {
                    QJsonObject objGroup3;
                    objGroup3[key] = value.toJsonValue();

                    QJsonObject objGroup2;
                    objGroup2.insert(group3Name, QJsonValue(objGroup3));

                    objGroup.insert(group2Name, QJsonValue(objGroup2));
                }

                valueGroupRef = objGroup;

                if (realtime)
                    flush();
            }
        }
        else
        {
            QJsonObject objGroup3;
            objGroup3[key] = value.toJsonValue();

            QJsonObject objGroup2;
            objGroup2.insert(group3Name, QJsonValue(objGroup3));

            QJsonObject objGroup;
            objGroup.insert(group2Name, QJsonValue(objGroup2));

            mJsonRoot.insert(groupName, QJsonValue(objGroup));

            if (realtime)
                flush();
        }
    };

    /*
    {
        "arrayName":[
            "键key1": "值value1", //arrayIndex===0
            "键key2": "值value2", //arrayIndex===1
        ]
    }
    */
    void appendArrayValue(const QString &arrayName, const QVariant &value)
    {
        QWriteLocker locker(&mRWLock);
        auto iterator = mJsonRoot.find(arrayName);
        if (iterator != mJsonRoot.end())
        {
            QJsonValueRef valueArrayRef = iterator.value();
            if (valueArrayRef.isArray())
            {
                QJsonArray arrayGroup = valueArrayRef.toArray();
                arrayGroup.append(value.toJsonValue());
                valueArrayRef = arrayGroup;

                if (realtime)
                    flush();
            }
        }
        else
        {
            QJsonArray arrayGroup;
            arrayGroup.append(value.toJsonValue());

            mJsonRoot.insert(arrayName, QJsonValue(arrayGroup));

            if (realtime)
                flush();
        }
    };

    /*
    {
        "arrayName":[
            "键key1": "值value1", //arrayIndex===0
            "键key2": "值value2", //arrayIndex===1
        ]
    }
    */
    void setArrayValue(const QString &arrayName, const quint8 &arrayIndex, const QVariant &value){
        QWriteLocker locker(&mRWLock);
        auto iterator = mJsonRoot.find(arrayName);
        if (iterator != mJsonRoot.end())
        {
            QJsonValueRef valueArrayRef = iterator.value();
            if (valueArrayRef.isArray())
            {
                QJsonArray arrayGroup = valueArrayRef.toArray();
                if (arrayIndex < arrayGroup.size())
                {
                    arrayGroup.replace(arrayIndex, value.toJsonValue());
                    valueArrayRef = arrayGroup;

                    if (realtime)
                        flush();
                }
            }
        }
    };

    /*
    {
        "groupName":{
            "arrayName":[
                "键key1": "值value1", //arrayIndex===0
                "键key2": "值value2", //arrayIndex===2
            ]
        }
    }
    */

    /*
    {
        "groupName":{
            "arrayName":[
                {
                    "键key1": "值value1", //arrayIndex===0
                    "键key2": "值value2", //arrayIndex===2
                }
            ]
        }
    }
    */
    void setArrayValue(const QString &groupName, const QString &arrayName, const quint8 &arrayIndex, const QString &key, const QVariant &value){
        QWriteLocker locker(&mRWLock);
        auto iterator = mJsonRoot.find(groupName);
        if (iterator != mJsonRoot.end())
        {
            QJsonValueRef valueGroupRef = iterator.value();
            if (valueGroupRef.isObject())
            {
                QJsonObject objGroup = valueGroupRef.toObject();
                auto iterator2 = objGroup.find(arrayName);
                if (iterator2 != objGroup.end())
                {
                    QJsonValueRef valueArrayRef = iterator2.value();
                    if (valueArrayRef.isArray())
                    {
                        QJsonArray arrayGroup = valueArrayRef.toArray();
                        if (arrayIndex < arrayGroup.size()){
                            QJsonValueRef valueGroupRef = arrayGroup[arrayIndex];
                            if (valueGroupRef.isObject())
                            {
                                QJsonObject objArray = valueGroupRef.toObject();
                                objArray[key] = value.toJsonValue();

                                valueGroupRef = objArray;
                            }
                            else
                            {
                                // 找到字段，但是类型不对
                                return;
                            }
                        }
                        else
                        {
                            // 数组越界
                            QJsonObject objArray;
                            objArray[key] = value.toJsonValue();

                            arrayGroup.append(objArray);
                        }

                        valueArrayRef = arrayGroup;
                    }
                    else
                    {
                        // 找到字段，但是类型不对
                        return;
                    }
                }
                else
                {
                    QJsonObject objArray;
                    objArray[key] = value.toJsonValue();

                    QJsonArray arrayGroup;
                    arrayGroup.append(objArray);

                    objGroup.insert(arrayName, QJsonValue(arrayGroup));
                }

                valueGroupRef = objGroup;
            }
            else
            {
                // 找到字段，但是类型不对
                return;
            }
        }
        else
        {
            QJsonObject objArray;
            objArray[key] = value.toJsonValue();

            QJsonArray arrayGroup;
            arrayGroup.append(objArray);

            QJsonObject objGroup;
            objGroup.insert(arrayName, QJsonValue(arrayGroup));

            mJsonRoot.insert(groupName, QJsonValue(objGroup));
        }

        if (realtime)
            flush();
    };

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////
private:
    bool save(const QString &fileName = ""){
        //QWriteLocker locker(&mRWLock);
        QFile file(fileName);
        if (fileName.isEmpty())
            file.setFileName(mFileName);

        if (file.open(QIODevice::ReadWrite | QIODevice::Text)) {
            QJsonDocument jsonDoc(mJsonRoot);
            file.write(jsonDoc.toJson());
            file.close();
            return true;
        } else {
            qDebug() << "文件[" << mFileName << "]信息保存失败！";
            return false;
        }
    };

protected:
    QString mFileName;
    QString mPrefix;
    QJsonObject mJsonRoot;
    QJsonObject mJsonGroup;
    QReadWriteLock mRWLock;
    QMutex mAccessMutex;//访问锁
    bool mOpened = false;//文档打开成功标识
    bool realtime = false;
    bool mWatchThisFile = false;
    QFileSystemWatcher *mConfigurationFileWatch;
};

#include <QSettings>
#include <QApplication>
class GlobalSettings: public QSettings
{
    Q_OBJECT
public:
    explicit GlobalSettings(QObject *parent = nullptr);
    explicit GlobalSettings(QString fileName, QObject *parent = nullptr);
    ~GlobalSettings();

#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    void setValue(QAnyStringView key, const QVariant &value);
#else
    void setValue(const QString &key, const QVariant &value);
#endif
    void setRealtimeSave(bool realtime);
    bool isRealtimeSave() const { return realtime;}

private:
    bool realtime = false;
};

#endif // GLOBALSETTINGS_H
