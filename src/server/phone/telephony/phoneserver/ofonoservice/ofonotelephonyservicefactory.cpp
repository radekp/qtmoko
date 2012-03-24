/****************************************************************************
 * *
 ** This file is part of the Qt Extended Opensource Package.
 **
 ** Copyright (C) 2011 Radek Polak
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

#include "ofonotelephonyservicefactory.h"
#include <qvaluespace.h>
#include <qtimer.h>
#include <QSignalSourceProvider>

#include <qofonoutil.h>
#include <qofonomanager.h>

#include "phoneserver.h"
#include "ofonotelephonyservice.h"

OFonoTelephonyServiceFactory::OFonoTelephonyServiceFactory(QObject * parent)
{
    Q_UNUSED(parent);
}

QTelephonyService *OFonoTelephonyServiceFactory::service()
{
    OrgOfonoManagerInterface oManager("org.ofono", "/",
                                      QDBusConnection::systemBus(), this);
    QOFonoDBusPendingReply < QOFonoObjectList > reply = oManager.GetModems();
    if (!checkReply(reply)) {
        return NULL;
    }
    QOFonoObjectList modems = reply.value();
    if (modems.count() == 0) {
        return NULL;
    }
    return new OFonoTelephonyService("modem", modems.at(0).object.path(), NULL);
}

QByteArray OFonoTelephonyServiceFactory::serviceName() const
{
    // Synchronize with phoneserver.cpp
    return QByteArray("oFono");
}

QTOPIA_TASK(OFonoTelephonyServiceFactory, OFonoTelephonyServiceFactory);
QTOPIA_TASK_PROVIDES(OFonoTelephonyServiceFactory, TelephonyServiceFactory);
