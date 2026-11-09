// Implementation of common utility functions for logging and network operations
#include <QDebug>
#include <QDir>
#include <QByteArray>
#include <QProcess>
#include <QStringList>
#include <QCoreApplication>
#include <QSettings>
#include <QStandardPaths>
#include <QFileInfo>
#include <QtNetwork>
#include <QHostInfo>
#include "commonutils.h"


QString CommonUtils::m_logPath = DEFAULT_LOG_FILE_PATH;

void outputMessage(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QString typeText;

    switch(type)
    {
    case QtDebugMsg:
        typeText = QString("Debug:");
        break;
    case QtInfoMsg:
        typeText = QString("Info:");
        break;
    case QtWarningMsg:
        typeText = QString("Warning:");
        break;
    case QtCriticalMsg:
        typeText = QString("Critical:");
        break;
    case QtFatalMsg:
        typeText = QString("Fatal:");
    }
    QString filePath(context.file);

    static QMutex mutex;

    static bool logToFile = QSettings(CommonUtils::settingsFilePath(),QSettings::IniFormat).value("Log/logToFile", true).toBool();
    if(logToFile)
    {
        mutex.lock();
        QFile file(CommonUtils::m_logPath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Append))
        {
            mutex.unlock();
            return;
        }
        QTextStream text_stream(&file);
        text_stream << QString("%1 %2 %3 %4 %5").arg(typeText).arg(QLocale("en_US").toString(QDateTime::currentDateTime(),"yyyy-MM-dd hh:mm:ss ddd")).arg(filePath.mid(filePath.lastIndexOf('\\')+1)).arg(context.line).arg(msg);
        file.close();
        mutex.unlock();
    }
}

QString getLogFilePath()
{
    QString logDir = CommonUtils::runtimeDataDirectory() + "/log";

    if(!QDir(logDir).exists())
    {
        qDebug() << logDir << " not exist!" << Qt::endl;

        if(!QDir().mkpath(logDir))
        {
            qDebug() << logDir << " creat failed!" << Qt::endl;
            return DEFAULT_LOG_FILE_PATH;
        }
        else
        {
            qDebug() << logDir << " creat success!" << Qt::endl;
        }
    }

    QDir sDir(logDir);
    QFileInfoList fileList = sDir.entryInfoList(QDir::Files, QDir::Time);

    while(fileList.size() - MAX_LOG_FILE_NUM > 0)
    {
        if(!QFile::remove(fileList.back().filePath()))
        {
            qDebug() << "remove file error:" << fileList.back().filePath() <<Qt::endl;
        }
        fileList.pop_back();
    }

   QString name = QDateTime::currentDateTime().date().toString("yyyyMMdd") + ".txt";
   if(fileList.size() > 0 && name == fileList.front().fileName())
   {
       return fileList.front().filePath();
   }
   else
   {
       if(fileList.size() >= MAX_LOG_FILE_NUM)
       {
            if(!QFile::remove(fileList.back().filePath()))
            {
               qDebug() << "remove file error:" << fileList.back().filePath() <<Qt::endl;
            }
            fileList.pop_back();
       }

       QFile file(logDir + "/" + name);
       if(!file.open(QIODevice::WriteOnly | QIODevice::Text))
       {
           qDebug() << "creat file failed: " << logDir + "/" + name << Qt::endl;
           return DEFAULT_LOG_FILE_PATH;
       }
       file.close();
   }

   return logDir + "/" + name;
}


CommonUtils::CommonUtils()
{
}

CommonUtils::~CommonUtils()
{}

void CommonUtils::installLogHandler()
{
    m_logPath = getLogFilePath();
    qInstallMessageHandler(outputMessage);
}

QString CommonUtils::settingsFilePath()
{
    const QString configured = qEnvironmentVariable("XRT_SERVICE_CONFIG_FILE");
    if (!configured.isEmpty())
        return QFileInfo(configured).absoluteFilePath();
    return QCoreApplication::applicationDirPath() + "/setting.ini";
}

QString CommonUtils::runtimeDataDirectory()
{
    const QString configured = qEnvironmentVariable("XRT_RUNTIME_DIRECTORY");
    if (!configured.isEmpty())
        return QFileInfo(configured).absoluteFilePath();
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/PICOBusinessSuitData";
}

QStringList CommonUtils::getLocalIPv4AddressList()
{
    QStringList addresses;
    for (const QNetworkInterface& interface : QNetworkInterface::allInterfaces())
    {
        const auto flags = interface.flags();
        if (!(flags & QNetworkInterface::IsUp) || !(flags & QNetworkInterface::IsRunning)
            || (flags & QNetworkInterface::IsLoopBack))
            continue;
        for (const QNetworkAddressEntry& entry : interface.addressEntries())
        {
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol && !entry.ip().isLoopback())
                addresses.append(entry.ip().toString());
        }
    }
    addresses.removeDuplicates();
    return addresses;
}

QString CommonUtils::getLocalIPv4Address()
{
    const auto addresses = getLocalIPv4AddressList();
    return addresses.isEmpty() ? QString() : addresses.first();
}

bool CommonUtils::isOnline()
{
    if(!QNetworkInformation::loadDefaultBackend())
    {
        qDebug() << "QNetworkInformation load failed." << Qt::endl;
        return false;
    }

    QNetworkInformation::Reachability status = QNetworkInformation::instance()->reachability();
    if(status == QNetworkInformation::Reachability::Local
        || status ==  QNetworkInformation::Reachability::Site
        || status == QNetworkInformation::Reachability::Online)
    {
        return true;
    }
    else
    {
        return false;
    }
}
