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

#include "ofonosupplementaryservices.h"
#include "ofonotelephonyservice.h"

OFonoSupplementaryServices::OFonoSupplementaryServices(OFonoTelephonyService *service)
    : QSupplementaryServices( service->service(), service, QCommInterface::Server )
    , service(service)
{
}

OFonoSupplementaryServices::~OFonoSupplementaryServices()
{
}

void OFonoSupplementaryServices::cancelUnstructuredSession()
{
    qDebug() << "OFonoSupplementaryServices::cancelUnstructuredSession";
}

void OFonoSupplementaryServices::sendUnstructuredData( const QString& data )
{
    qDebug() << "OFonoSupplementaryServices::sendUnstructuredData data=" << data;
}

void OFonoSupplementaryServices::sendSupplementaryServiceData
        ( const QString& data )
{
    qDebug() << "OFonoSupplementaryServices::sendSupplementaryServiceData data=" << data;
/*    QOFonoDBusPendingCall call = service->gsmNet.SendUssdRequest(data);
    watchOFonoCall(call, this, SLOT(sendUssdRequestFinished(QOFonoDBusPendingCall &)));
    */
}

void OFonoSupplementaryServices::sendUssdRequestFinished(QOFonoDBusPendingCall & call)
{
    QOFonoDBusPendingReply<> reply = call;
    emit supplementaryServiceResult(qTelResult(reply));
}

static QSupplementaryServices::UnstructuredAction ofonoModeToQtAction(const QString &mode)
{
    if(mode == "completed")     // if the last user-initiated request has been successfully completed
        return QSupplementaryServices::NoFurtherActionRequired;
    if(mode == "useraction")    // if this is a network-initiated request and further user action is necessary
        return QSupplementaryServices::FurtherActionRequired;
    if(mode == "terminated")    // if the network terminated the request
        return QSupplementaryServices::TerminatedByNetwork;
    if(mode == "localclient")   // if another local client on the network has responded
        return QSupplementaryServices::OtherLocalClientResponded;
    if(mode == "unsupported")   // if the last user-inititated request is unsupported
        return QSupplementaryServices::OperationNotSupported;
    if(mode == "timeout")       // if the network has not answered in time
        return QSupplementaryServices::NetworkTimeout;
    
    qWarning() << "ofonoModeToQtAction: unknown mode " << mode;
    return QSupplementaryServices::NoFurtherActionRequired;
}

void OFonoSupplementaryServices::onIncomingUssd(const QString &mode, const QString &message)
{
    emit unstructuredNotification(ofonoModeToQtAction(mode), message);
}