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

#include "ofonocallprovider.h"
#include "ofonotelephonyservice.h"
#include "ofonophonecall.h"

OFonoCallProvider::OFonoCallProvider(OFonoTelephonyService * service)
:  QPhoneCallProvider(service->service(), service)
    , service(service)
{
    setCallTypes(QStringList() << "Voice");
}

OFonoCallProvider::~OFonoCallProvider()
{
}

QPhoneCallImpl *OFonoCallProvider::create
    (const QString & identifier, const QString & callType)
{
    qDebug() << "OFonoCallProvider::create callType=" << callType;
    return new OFonoPhoneCall(service, identifier, callType, "");
}
