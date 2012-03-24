#ifndef QOFONOUTIL_H
#define QOFONOUTIL_H

#include <QDebug>
#include <QtDBus>
#include <QTimer>
#include "qofonodbuspendingcall.h"
#include "qofonodbuspendingreply.h"

#if defined(QOFONO_LIBRARY)
#define QOFONO_EXPORT Q_DECL_EXPORT
#else
#define QOFONO_EXPORT Q_DECL_IMPORT
#endif

// Check reply from ofono dbus call.
// Waits for reply to finish if waitForFinished is true.
// Returns value specified in ok if success.
// Returns value specified in err if error.
template <class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8>
        int QOFONO_EXPORT checkReply(QOFonoDBusPendingReply<T1, T2, T3, T4, T5, T6, T7, T8> & reply,
                                   bool waitForFinished = true,
                                   int ok = 1,
                                   int err = 0,
                                   int unfinished = 0)
{
    if(waitForFinished)
    {
        reply.waitForFinished();
    }
    if(reply.isError())
    {
        QString errorStr = reply.error().message();
        qWarning() << "Error in " << reply.methodCall << errorStr;
        return err;
    }
    if(reply.isFinished() && reply.isValid())
    {
        // Dump result
        QString resStr;
        for(int i = 0; i < reply.count(); i++)
        {
            QVariant arg = reply.argumentAt(i);
            if(arg.canConvert<QDBusArgument>())
            {
                QDBusArgument dbusArg = arg.value<QDBusArgument>();
                QString signature = dbusArg.currentSignature();
                if(signature == "a{sv}")
                {
                    QVariantMap map;
                    dbusArg >> map;
                    for(int j = 0; j < map.count(); j++)
                    {
                        QString key = map.keys().at(j);
                        resStr += " " + key + "=" + map.value(key).toString();
                    }
                }
                //else
                //{
                    //qWarning() << "checkReply: unknown signature " << signature;
                //}
            }
            else
            {
                resStr += " " + arg.toString();
            }
        }
        qDebug() << "    dbus call " + reply.methodCall + " returned" + resStr;
        return ok;
    }
    return unfinished;
}

// This class is used to implement watchCall mechanism which allows us to
// call callback method when dbus call is finished. It's similar to
// QDBusPendingCallWatcher, which for some reason sometimes does not work
// on some methods.
//
// This implementation now allows only once pending call at a time. Next
// call will use waitForFinished() to wait until previous call ends.
class QOFONO_EXPORT QOFonoUtil : public QObject {
    Q_OBJECT
public:
    QOFonoUtil(QObject *parent = 0);
    ~QOFonoUtil();

public:
    QOFonoDBusPendingCall pendingCall;            // current call, we can have just one pending call at a time
    bool pendingNotified;                       // true if we already signalled finish
    QObject * pendingReceiver;                  // object to receive finish signal
    const char * pendingFinished;               // finish method to be called
    int checkInterval;                          // we start checking with small intervals and then make them longer

    static QOFonoUtil instance;

    void watchCall(QOFonoDBusPendingCall & call,
                   QObject * receiver,
                   const char * finishedMethod);

    static void waitForFinished();              // wait for current pending call to be finished

Q_SIGNALS:
    void finished(QOFonoDBusPendingCall & call);

private slots:
    void pendingCheck();
    void emitFinished(QObject * receiver, const char * finishedMethod, QOFonoDBusPendingCall & call);
};

void QOFONO_EXPORT watchOFonoCall(QOFonoDBusPendingCall & call,
                              QObject * receiver,
                              const char * finishedMethod);

#endif // QOFONOUTIL_H
