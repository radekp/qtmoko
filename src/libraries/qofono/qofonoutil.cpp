#include "qofonoutil.h"

QOFonoUtil::QOFonoUtil(QObject *parent) : QObject(parent)
        , pendingCall(QDBusPendingReply<>())
        , pendingNotified(true)
        , pendingReceiver(NULL)
        , pendingFinished(NULL)
        , checkInterval(10)
{
}

QOFonoUtil::~QOFonoUtil()
{
}

QOFonoUtil QOFonoUtil::instance;

void QOFonoUtil::emitFinished(QObject *receiver, const char *finishedMethod, QOFonoDBusPendingCall &call)
{
    disconnect();
    QObject::connect(this, SIGNAL(finished(QOFonoDBusPendingCall &)),
                     receiver, finishedMethod);
    emit finished(call);

   //QMetaObject::invokeMethod(receiver, "getStatusFinished", Qt::DirectConnection,
   //                          Q_ARG(QOFonoDBusPendingCall &, call));
}

void QOFonoUtil::pendingCheck()
{
    if(pendingNotified)
    {
        return;
    }

    QDBusPendingReply<> reply = pendingCall;
    if(!reply.isFinished())
    {
        checkInterval += 10;
        QTimer::singleShot(checkInterval, this, SLOT(pendingCheck()));
    }
    else
    {
        pendingNotified = true;
        emitFinished(pendingReceiver, pendingFinished, pendingCall);
    }
}

void QOFonoUtil::watchCall(QOFonoDBusPendingCall & call,
                         QObject * receiver,
                         const char * finishedMethod)
{
    bool oldNotified = pendingNotified;
    QOFonoDBusPendingCall oldCall = pendingCall;
    QObject *oldReceiver = pendingReceiver;
    const char *oldFinished = pendingFinished;

    pendingNotified = false;
    pendingCall = call;
    pendingReceiver = receiver;
    pendingFinished = finishedMethod;

    checkInterval = 10;
    QTimer::singleShot(checkInterval, this, SLOT(pendingCheck()));

    if(!oldNotified)
    {
        qDebug() << "watchCall: waiting for finish of " << oldCall.methodCall;
        oldCall.waitForFinished();
        qDebug() << "watchCall: " << oldCall.methodCall << " finished";
        emitFinished(oldReceiver, oldFinished, oldCall);
    }
}

void QOFonoUtil::waitForFinished()
{
    instance.pendingCall.waitForFinished();
}

void watchOFonoCall(QOFonoDBusPendingCall & call,
                  QObject * receiver,
                  const char * finishedMethod)
{
    QOFonoUtil::instance.watchCall(call, receiver, finishedMethod);
}
