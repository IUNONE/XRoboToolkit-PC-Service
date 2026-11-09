//Implementation of single instance application management

#include <QStandardPaths>
#include <QDir>
#include <QCryptographicHash>
#include "commonutils.h"
#include "singleapplication.h"


static QString localServerName()
{
    if (qEnvironmentVariableIsEmpty("XRT_RUNTIME_DIRECTORY"))
        return QStringLiteral(SINGLE_APP_NAME);
    return QStringLiteral(SINGLE_APP_NAME) + "_" + QString::fromLatin1(
        QCryptographicHash::hash(CommonUtils::runtimeDataDirectory().toUtf8(), QCryptographicHash::Sha256).toHex().left(16));
}

SingleApplication::SingleApplication(int &argc, char **argv):QCoreApplication(argc,argv)
{
    m_instanceLock = nullptr;
    connect(&m_localServer, &QLocalServer::newConnection, this, &SingleApplication::onNewConnection);
}

SingleApplication::~SingleApplication()
{
    if(m_instanceLock)
    {
        m_instanceLock->unlock();
        delete m_instanceLock;
        m_instanceLock = nullptr;
    }

}

void SingleApplication::onNewConnection()
{
    emit activeWindowSiganl();
}

bool SingleApplication::isStartUp()
{
    if(m_instanceLock)
    {
        if(!m_instanceLock->tryLock(TRY_LOCK_TIMEOUT))
        {
            qDebug() << "error: " << QString::number(m_instanceLock->error()) << Qt::endl;
            qDebug() << "client has been activate!" << Qt::endl;
            return true;
        }
        else
        {
            return false;
        }
    }
    else
    {
        const QString directory = CommonUtils::runtimeDataDirectory();
        if (!QDir().mkpath(directory))
            qFatal("Cannot create service runtime directory");
        QString lockFilePath = directory + "/lockfile";
        m_instanceLock = new QLockFile(lockFilePath);

        if(!m_instanceLock->tryLock(1))
        {
            qDebug() << "error: " << QString::number(m_instanceLock->error()) << Qt::endl;
            qDebug() << "client has been activate!" << Qt::endl;
            return true;
        }
        else
        {
            return false;
        }

    }
}

void SingleApplication::initLocalServer()
{
    m_localServer.setSocketOptions(QLocalServer::WorldAccessOption);
    if(!m_localServer.listen(localServerName()))
    {
        if(m_localServer.serverError() == QAbstractSocket::AddressInUseError)
        {
            QLocalServer::removeServer(localServerName());
            if(!m_localServer.listen(localServerName()))
            {
                qDebug() << "local server listen error: " << m_localServer.errorString() << Qt::endl;
            }
        }
        else
        {
             qDebug() << "local server listen error: " << m_localServer.errorString() << Qt::endl;
        }
    }
}

bool SingleApplication::connectLocalServer()
{
    m_localSocket.connectToServer(localServerName());
    if(m_localSocket.waitForConnected(CONNECT_TIMEOUT))
    {
        qDebug() << "connnect to " << localServerName() << " ok." << Qt::endl;
        return true;
    }
    else
    {

        qDebug() << "connnect to " << localServerName() << "error: " << m_localSocket.error() << Qt::endl;
        return false;
    }
}

void SingleApplication::release()
{
    if(m_instanceLock)
    {
        m_instanceLock->unlock();
        delete m_instanceLock;
        m_instanceLock = nullptr;
    }
}
