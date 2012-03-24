/****************************************************************************
**
** This file is part of the Qt Extended Opensource Package.
**
** Copyright (C) 2011 Radek Polak.
**
** Contact: Qt Extended Information (info@qtextended.org)
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

#include "ofonorffunctionality.h"
#include "ofonotelephonyservice.h"
#include "ofonoutil.h"

OFonoRfFunctionality::OFonoRfFunctionality(OFonoTelephonyService * service)
:  QPhoneRfFunctionality(service->service(), service, QCommInterface::Server)
    , service(service)
    , modemPowered(false)
    , reqLevel((QPhoneRfFunctionality::Level) (-1))
{
}

OFonoRfFunctionality::~OFonoRfFunctionality()
{
}

void OFonoRfFunctionality::modemPropertyChanged(const QString & name,
                                                const QDBusVariant & value)
{
    if (name == "Powered") {
        bool oldPowered = modemPowered;
        modemPowered = value.variant().toBool();

        qDebug() << "OFonoRfFunctionality::modemPropertyChanged oldPowered=" <<
            oldPowered << ", modemPowered=" << modemPowered;

        if (oldPowered == modemPowered) {
            return;
        }
        // Set level which was requested while modem was not alive
        if (modemPowered && reqLevel >= 0) {
            setLevel(reqLevel);
            reqLevel = (QPhoneRfFunctionality::Level) (-1);
        }
        forceLevelRequest();
    }
}

void OFonoRfFunctionality::forceLevelRequest()
{
    setValue("level", qVariantFromValue(Full));
    emit levelChanged();
    return;

/*    
    qDebug() << "forceLevelRequest";

    if (!modemPowered) {
        return;
    }

    QVariantMap properties = service->modemProperties;
    bool online = properties.value("Online").toBool();
    setValue("level",
             online ? QPhoneRfFunctionality::Full : QPhoneRfFunctionality::
             DisableTransmitAndReceive);
    emit levelChanged(); */
}

void OFonoRfFunctionality::setLevel(QPhoneRfFunctionality::Level level)
{
    qDebug() << "setLevel level=" << level;

    if (!modemPowered) {
        reqLevel = level;       // just remember the request
        return;
    }

    bool online = (level == QPhoneRfFunctionality::Full);

    QOFonoDBusPendingCall call =
        service->oModem.SetProperty("Online", QDBusVariant(online));
    watchOFonoCall(call, this, SLOT(onlineFinished(QOFonoDBusPendingCall &)));
}

void OFonoRfFunctionality::onlineFinished(QOFonoDBusPendingCall & call)
{
    QOFonoDBusPendingReply <> reply = call;
    QTelephony::Result res = qTelResult(reply);
    emit setLevelResult(res);
}
