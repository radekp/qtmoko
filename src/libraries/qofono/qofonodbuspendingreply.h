#ifndef QOFONODBUSPENDINGREPLY_H
#define QOFONODBUSPENDINGREPLY_H

#include <QtDBus>
#include "qofonodbuspendingcall.h"

#if defined(QOFONO_LIBRARY)
    #define QOFONO_EXPORT Q_DECL_EXPORT
#else
    #define QOFONO_EXPORT Q_DECL_IMPORT
#endif

template<typename T1 = void, typename T2 = void, typename T3 = void, typename T4 = void,
         typename T5 = void, typename T6 = void, typename T7 = void, typename T8 = void>
class QOFONO_EXPORT QOFonoDBusPendingReply : public QOFonoDBusPendingCall
{
public:
    QDBusPendingReply<T1,T2,T3,T4,T5,T6,T7,T8> reply;

    inline QOFonoDBusPendingReply()
        : QOFonoDBusPendingCall(QDBusPendingReply<>())
    {
    }

    inline QOFonoDBusPendingReply(const QOFonoDBusPendingCall &call)
        : QOFonoDBusPendingCall(call)
    {
        reply = call;
        this->method = call.method;
        this->methodCall = call.methodCall;
    }

    inline T1 value()
    {
        return reply.value();
    }

    inline QVariant argumentAt(int index) const
    {
        return reply.argumentAt(index);
    }

    inline int count()
    {
        return reply.count();
    }
};


#endif // QOFONODBUSPENDINGREPLY_H
