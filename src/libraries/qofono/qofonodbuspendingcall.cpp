#include "qofonodbuspendingcall.h"

QOFonoDBusPendingCall::QOFonoDBusPendingCall(const QDBusPendingCall &other)
    : QDBusPendingCall(other)
    , method()
    , methodCall()
{
}
