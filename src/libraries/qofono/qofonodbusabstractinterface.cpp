#include "qofonodbusabstractinterface.h"
#include "qofonoutil.h"

QOFonoDbusAbstractInterface::QOFonoDbusAbstractInterface(const QString &service, const QString &path, const char *interface,
                                                     const QDBusConnection &connection, QObject *parent)
    : QDBusAbstractInterface(service, path, interface, connection, parent)
{
}

QOFonoDbusAbstractInterface::~QOFonoDbusAbstractInterface()
{
}

QOFonoDBusPendingCall QOFonoDbusAbstractInterface::ofonoAsyncCall(const QString &method, const QList<QVariant> &args)
{
    QString methodStr(path() + "->" + method + "(");
    for(int i = 0; i < args.count(); i++)
    {
        QVariant arg = args.at(i);
        QString argStr;
        if(i > 0)
            methodStr += ", ";
        if(arg.canConvert<QDBusVariant>())
            argStr = arg.value<QDBusVariant>().variant().toString();
        else
            argStr = args.at(i).toString();

        int lf;
        if((lf = argStr.indexOf('\n')) > 0)
            argStr = argStr.left(lf) + "...";
        methodStr += argStr;
    }
    methodStr += ")";

    // Wait for pending calls to finish, otherwise we have problems e.g.
    // two calls of SendTextMessage() hang up the ofono daemon.
    QOFonoUtil::waitForFinished();
    qDebug() << "calling " << methodStr;

    QDBusPendingCall call = asyncCallWithArgumentList(method, args);
    QOFonoDBusPendingCall res(call);
    res.methodCall = methodStr;
    res.method = method;
    return res;
}
