//Main entry point for the Enterprise Assistant Client application

#include <QCoreApplication>
#include <QTextCodec>
#include <QSslSocket>
#include <QLockFile>
#include <QProcess>
#include <QElapsedTimer>
#include "commonutils.h"
#include "business.h"
#include "singleapplication.h"
#include "PXREAGRPCServer.h"


#define STR(R) #R
#define STRVALUE(R) STR(R)



int main(int argc, char *argv[])
{
    SingleApplication app(argc, argv);

    QElapsedTimer ElapsedTimer;
    ElapsedTimer.start();

#ifdef QT_NO_DEBUG
    qDebug() << "release mode";
    CommonUtils::installLogHandler();
#else
    qDebug() << "debug mode";

#endif

    if(app.isStartUp())
    {
        app.connectLocalServer();
        return 0;
    }
    else
    {
        app.initLocalServer();
    }

    QTextCodec* codec = QTextCodec::codecForName("utf-8");
    QTextCodec::setCodecForLocale(codec);

    app.setOrganizationDomain("picoxr.com");
    app.setApplicationName("RoboticsServiceProcess");

    auto sdk = new PXREAServerAPI(&app);


    DeviceManagement deviceManage;
    Business business;
    business.setDeviceManager(deviceManage.getThisPoint());
    business.init();

    sdk->init(deviceManage.getThisPoint(), true, true);
    qDebug() << "耗时: " <<ElapsedTimer.elapsed() << "毫秒" << Qt::endl;

    app.exec();
    sdk->deinit();
    app.release();


    return 0;
}
