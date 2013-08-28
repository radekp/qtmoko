/****************************************************************************
**
** This file is part of the Qt Extended Opensource Package.
**
** Copyright (C) 2011 Radek Polak.
**
** Contact: psonek2@seznam.cz
**
** This file may be used under the terms of the GNU General Public License
** version 2.0 as published by the Free Software Foundation and appearing
** in the file LICENSE.GPL included in the packaging of this file.
**
** Please review the following information to ensure GNU General Public
** Licensing requirements will be met:
**     http://www.fsf.org/licensing/licenses/info/GPLv2.html.
**
**
****************************************************************************/

#ifndef QBLUETOOTHDBUS_H
#define QBLUETOOTHDBUS_H

#include <qbluetoothglobal.h>
#include <qbluetoothnamespace.h>

#include <qobject.h>
#include <qglobal.h>
#include <qtopialog.h>
#include <stdio.h>
#include <string.h>

#include <QString>
#include <QList>
#include <QDBusInterface>
#include <QDBusReply>
#include <QStringList>
#include <QDateTime>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QSet>
#include <QDebug>
#include <QMetaObject>

typedef QMap<uint,QString> UintStringMap;
Q_DECLARE_METATYPE(UintStringMap)

/*
 * Helper class for calling bluez4 dbus interface, It can log dbus calls and
 * handle errors.
 */
class QBLUETOOTH_EXPORT QBluetoothDbusIface : public QDBusInterface
{
    Q_OBJECT

public:
    
    QBluetoothDbusIface(const QString &service, const QString &path, const char *interface,
                        const QDBusConnection &connection, QObject *parent = 0);
    
    template <class T>
            bool btcall(const QString & method,
                        QDBusReply<T> & reply = QDBusReply<T>(),
                        QList<QVariant> args = QList<QVariant>(),
                        bool async = false,
                        QObject * receiver = NULL,
                        const char * returnMethod = NULL,
                        const char * errorMethod = NULL);

    QVariant getProperty(QString name, QDBusError * error = 0);
    bool setProperty(QString name, QVariant value, QDBusError * error = 0);
    bool setPropertyAsync(QString name, 
                          QVariant value,
                          QObject * receiver,
                          const char * returnMethod,
                          const char * errorMethod);

protected:
    virtual void asyncReply(const QDBusMessage &msg);
    virtual void asyncErrorReply(const QDBusError &error, const QDBusMessage &msg);        
};

template <class T>
        bool QBluetoothDbusIface::btcall(const QString & method,
                                         QDBusReply<T> & reply,
                                         QList<QVariant> args,
                                         bool async,
                                         QObject * receiver,
                                         const char * returnMethod,
                                         const char * errorMethod)
{
    if(!isValid()) {
        qWarning() << "Dbus interface " << path() << " is not valid";
        return false;
    }

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
    qLog(Bluetooth) << "calling " << methodStr;
    
    if(async) {
        if(receiver == NULL)
            receiver = this;
        if(returnMethod == NULL)
            returnMethod = SLOT(asyncReply(QDBusMessage));
        if(errorMethod == NULL)
            errorMethod = SLOT(asyncErrorReply(QDBusError,QDBusMessage));

        return callWithCallback(method, args, receiver, returnMethod, errorMethod);
    }
    else {
        reply = callWithArgumentList(QDBus::AutoDetect, method, args);
        if(reply.isValid())
            return true;
    }

    qWarning() << "Method call " << methodStr << " failed: " << reply.error();
    return false;
}

#endif
