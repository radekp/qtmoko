#ifndef QOFONODBUSABSTRACTINTERFACE_H
#define QOFONODBUSABSTRACTINTERFACE_H

#include <QtDBus/QtDBus>
#include <qofonodbuspendingcall.h>
#include <qofonodbuspendingreply.h>

#if defined(QOFONO_LIBRARY)
    #define QOFONO_EXPORT Q_DECL_EXPORT
#else
    #define QOFONO_EXPORT Q_DECL_IMPORT
#endif

/*
 * Base class for oFono dbus interfaces, useful for logging and similar
 * customizations.
 */
class QOFONO_EXPORT QOFonoDbusAbstractInterface: public QDBusAbstractInterface
{
    Q_OBJECT

public:
    QOFonoDbusAbstractInterface(const QString &service, const QString &path, const char *interface,
                              const QDBusConnection &connection, QObject *parent);

    ~QOFonoDbusAbstractInterface();

    QOFonoDBusPendingCall ofonoAsyncCall(const QString &method, const QList<QVariant> &args);

};

#endif // QOFONODBUSABSTRACTINTERFACE_H
