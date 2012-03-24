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

#include "ofonosiminfo.h"
#include "ofonotelephonyservice.h"

OFonoSimInfo::OFonoSimInfo(OFonoTelephonyService * service)
:  QSimInfo(service->service(), service, QCommInterface::Server)
    , service(service)
    , info()
{
}

OFonoSimInfo::~OFonoSimInfo()
{
}

void OFonoSimInfo::deviceStatus(QString status)
{
    /*
    qDebug() << "OFonoSimInfo::deviceStatus status=" << status;

    if (status == "alive-registered" || status == "alive-sim-ready") {
        QOFonoDBusPendingCall call = service->gsmSim.GetSimInfo();
        watchOFonoCall(call, this,
                     SLOT(getSimInfoFinished(QOFonoDBusPendingCall &)));
    } else {
        setIdentity(QString());
    }
    */
}

void OFonoSimInfo::getSimInfoFinished(QOFonoDBusPendingCall & call)
{
    QOFonoDBusPendingReply < QVariantMap > reply = call;
    if (!checkReply(reply)) {
        return;
    }
    info = reply.value();
    setIdentity(info.value("imsi").toString());
}
