#ifndef GLOBALSETTINGS_H
#define GLOBALSETTINGS_H

#include <QObject>
#include <QSettings>

class BaseSettings : public QObject{
    Q_OBJECT
public:
    BaseSettings(const QString &fileName){
        mSettings = new QSettings(mFileName, QSettings::IniFormat);
    };
    ~BaseSettings(){
        delete mSettings;
        mSettings = nullptr;
    };

    QString fileName() const{
        return mFileName;
    }

    void beginGroup(const QString &prefix){
        mSettings->beginGroup(prefix);
    };
    void endGroup(){
        mSettings->endGroup();
    };

    virtual void load() = 0;
    virtual void save() = 0;

    void setValue(const QString &key, const QVariant &value){
        mSettings->setValue(key, value);
    };
    QVariant value(const QString &key, const QVariant &defaultValue/* = QVariant()*/) const
    {
        return mSettings->value(key, defaultValue);
    };

    void setArrayValue(const QString &arrayKey, const int &index, const QString &valueKey, const QVariant &value){
        mSettings->setValue(QString("%1_%2_%3").arg(arrayKey, valueKey).arg(index), value);
    };
    QVariant arrayValue(const QString &arrayKey, const int &index, const QString &valueKey, const QVariant &defaultValue = QVariant()) const
    {
        return mSettings->value(QString("%1_%2_%3").arg(arrayKey, valueKey).arg(index), defaultValue);
    };

protected:
    QString mFileName;
    QSettings *mSettings;
};

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
    };
    ~JsonSettings(){

    };

    bool isOpen() {
        return this->mOpened;
    }

    QString fileName() const{
        return mFileName;
    }

    void prepare(){
        emit sigPrepare(mFileName);
        //qDebug() << "enter lock >>>";
        mAccessMutex.lock();
    }

    bool finish()
    {
        mAccessMutex.unlock();
        //qDebug() << "leave lock <<<<<<";
        emit sigFinish(mFileName);
        return mResult;
    }

    void beginGroup(const QString &prefix = ""){
        if (prefix.isEmpty()){
            mPrefix = prefix;
            mJsonGroup = QJsonObject();
        } else {
            if (mJsonRoot.contains(prefix)){
                mJsonGroup = mJsonRoot[prefix].toObject();
                mPrefix = prefix;
            } else {
                mJsonGroup = QJsonObject();
            }
        }
    };
    void endGroup(){
        if (!mPrefix.isEmpty()){
            mJsonRoot[mPrefix] = mJsonGroup;
            mJsonGroup = QJsonObject();
            mPrefix.clear();
        }        
    };

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
    bool save(const QString &fileName = ""){
        //QWriteLocker locker(&mRWLock);
        QFile file(fileName);
        if (fileName.isEmpty())
            file.setFileName(mFileName);

        if (file.open(QIODevice::ReadWrite | QIODevice::Text)) {
            QJsonDocument jsonDoc(mJsonRoot);
            file.write(jsonDoc.toJson());
            file.close();
            mResult = true;
        } else {
            qDebug() << "文件[" << mFileName << "]信息保存失败！";
            mResult = false;
        }

        return mResult;
    };

    bool flush(){
        return save(mFileName);
    };

    /*
        适用于跟节点或一级节点赋值
        {
            "键key": "值value",
            "一级节点" :{           //需调用beginGroup进入子节点
                "键key": "值value"
            }
        }
    */
    void setValue(const QString &key, const QVariant &value){
        //QWriteLocker locker(&mRWLock);//beginGroup已经上锁了，这里就不需要了
        if (!mJsonGroup.isEmpty())
            mJsonGroup[key] = QJsonValue::fromVariant(value);
        else
            mJsonRoot[key] = QJsonValue::fromVariant(value);

        // QJsonValueRef RefPage = mJsonRoot.find(key).value();
        // RefPage = QJsonValue(value.toString());
        // flush();
    };
    QVariant value(const QString &key, const QVariant &defaultValue = QVariant())
    {
        //QReadLocker locker(&mRWLock);//beginGroup已经上锁了，这里就不需要了
        if (!mJsonGroup.isEmpty())
            return mJsonGroup[key].toVariant();
        else if (mJsonRoot.contains(key))
            return mJsonRoot[key].toVariant();
        else
            return defaultValue;
    };

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /*
        适用于二级节点下数组类型数据访问
        {
            "一级节点" :{            //需调用beginGroup进入子节点
                "二级节点键arrayKey": [
                    {               // index=0
                        "键valueKey" : "值value"
                    },
                    {               // index=1
                        "键valueKey" : "值value"
                    }
                ]
                "二级节点": [
                    {               // index=0
                        "键valueKey" : "值value"
                    }
                ]
            }
        }
    */
    void setArrayValue(const QString &arrayKey, const int &index, const QString &valueKey, const QVariant &value){
        //QWriteLocker locker(&mRWLock);//beginGroup已经上锁了，这里就不需要了
        QJsonArray jsonArray;
        if (mJsonGroup.contains(arrayKey)){
            jsonArray = mJsonGroup[arrayKey].toArray();
            QJsonObject item;
            if (index > jsonArray.size()){
                item[valueKey] = QJsonValue::fromVariant(value);
                jsonArray.append(item);
            } else {
                item = jsonArray.at(index).toObject();
                item[valueKey] = QJsonValue::fromVariant(value);
                jsonArray.replace(index, item);
            }
        } else {
            QJsonObject item;
            item[valueKey] = QJsonValue::fromVariant(value);
            jsonArray.append(item);
        }

        mJsonGroup[arrayKey] = jsonArray;
    };
    QVariant arrayValue(const QString &arrayKey, const int &index, const QString &valueKey, const QVariant &defaultValue = QVariant())
    {
        //QReadLocker locker(&mRWLock);//beginGroup已经上锁了，这里就不需要了
        if (mJsonGroup.contains(arrayKey)){
            QJsonArray jsonArray = mJsonGroup[arrayKey].toArray();
            if (index > jsonArray.size()){
                return defaultValue;
            } else {
                const QJsonObject mJsonValue = jsonArray[index].toObject();
                return mJsonValue[valueKey].toVariant();
            }
        } else {
            return defaultValue;
        }
    };

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /*
        适用于三级节点数据访问
        {
            "一级节点" :{            //需调用beginGroup进入子节点
                "二级节点键subGroup": {
                    "三级节点键childKey" {
                        "键valueKey" : "值value"
                    },
                    "三级节点键childKey" {
                        "键valueKey" : "值value"
                    }
                ]
            }
        }
    */
    void setChildValue(const QString &subGroup, const QString &childKey, const QString &valueKey, const QVariant &value){
        //QWriteLocker locker(&mRWLock);//beginGroup已经上锁了，这里就不需要了
        QJsonObject jsonSubGroup;
        if (mJsonGroup.contains(subGroup)){
            jsonSubGroup = mJsonGroup[subGroup].toObject();
            QJsonObject jsonChild;
            if (jsonSubGroup.contains(childKey)){
                jsonChild = jsonSubGroup[childKey].toObject();
            }

            jsonChild[valueKey] = QJsonValue::fromVariant(value);
            jsonSubGroup[childKey] = jsonChild;
        }
        else{
            QJsonObject jsonChild;
            jsonChild[valueKey] = QJsonValue::fromVariant(value);
            jsonSubGroup[childKey] = jsonChild;
        }

        mJsonGroup[subGroup] = jsonSubGroup;
    };
    QVariant childValue(const QString &subGroup, const QString &childKey, const QString &valueKey, const QVariant &defaultValue = QVariant())
    {
        //QReadLocker locker(&mRWLock);//beginGroup已经上锁了，这里就不需要了
        QVariant result = defaultValue;
        if (mJsonGroup.contains(subGroup)){
            QJsonObject jsonSubGroup = mJsonGroup[subGroup].toObject();
            if (jsonSubGroup.contains(childKey)){
                QJsonObject jsonChild = jsonSubGroup[childKey].toObject();
                if (jsonChild.contains(valueKey))
                    result = jsonChild[valueKey].toVariant();
                else
                    result = defaultValue;
            } else {
                result = defaultValue;
            }
        }

        return result;
    };

    Q_SIGNAL void sigPrepare(const QString &fileName);
    Q_SIGNAL void sigFinish(const QString &fileName);

protected:
    QString mFileName;
    QString mPrefix;
    QJsonObject mJsonRoot;
    QJsonObject mJsonGroup;
    QReadWriteLock mRWLock;
    QMutex mAccessMutex;//访问锁
    bool mOpened = false;//文档打开成功标识
    bool mResult = false;//保存操作结果
};

class GlobalSettings: public QObject
{
    Q_OBJECT
public:
    static GlobalSettings *instance() {
        static GlobalSettings globalSettings;
        return &globalSettings;
    }

    explicit GlobalSettings();
    ~GlobalSettings();

    JsonSettings* mFpgaSettings;
    JsonSettings* mIpSettings;

private:
    bool mWatchThisFile = true;
    QFileSystemWatcher *mConfigurationFileWatch;
};

#endif // GLOBALSETTINGS_H
