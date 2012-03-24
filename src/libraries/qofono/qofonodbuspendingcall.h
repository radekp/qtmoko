#ifndef QOFONODBUSPENDINGCALL_H
#define QOFONODBUSPENDINGCALL_H

#include <QtDBus>

#if defined(QOFONO_LIBRARY)
    #define QOFONO_EXPORT Q_DECL_EXPORT
#else
    #define QOFONO_EXPORT Q_DECL_IMPORT
#endif

class QOFONO_EXPORT QOFonoDBusPendingCall : public QDBusPendingCall
{
public:
    QOFonoDBusPendingCall(const QDBusPendingCall &other);

    // Useful for debugging
    QString method;
    QString methodCall;
};

#endif // QOFONODBUSPENDINGCALL_H
