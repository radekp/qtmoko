/****************************************************************************
**
** This file is part of the Qt Extended Opensource Package.
**
** Copyright (C) 2009 Trolltech ASA.
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

#include "phoneserver.h"
#include <qtopiaservices.h>
#include <qtopiaipcenvelope.h>
#include <QValueSpaceObject>
#include <qtopialog.h>

/*!
    \class PhoneServer
    \inpublicgroup QtTelephonyModule
    \brief The PhoneServer class represents the central dispatch server for phone requests.

    \ingroup QtopiaServer::Telephony

    Typical phone hardware only allows one process to access the
    AT command stream at any one time.  Because of this, Qt Extended Phone
    multiplexes multiple process' requests through the phone server,
    which is the only process that may access the actual hardware.

    The Qt Extended phone server is responsible for starting all telephony
    services, including those for GSM, VoIP, and other network types.

    At start up, the Qt Extended phone server sends a \c{start()} message to
    all applications that are registered as implementing the
    \l{TelephonyService}{Telephony} service.  This is the usual method
    for starting VoIP and third-party telephony services.

    If Qt Extended is not configured with the \c -no-modem flags, it will also
    start the default built-in AT command handler for the \c modem service using
    QModemService::createVendorSpecific(); otherwise the \c modem service is either not required, or 
    is provided via a \l{TelephonyService}{Telephony} service implementation.

    This class is part of the Qt Extended server and cannot be used by other Qt Extended applications.
    \sa QTelephonyService, QModemService
*/

/*!
    \internal
    Returns the number of telephony services available.
*/
static int executeTelephony( const QString& message )
{
    QStringList channels = QtopiaService::channels( "Telephony" );  // No tr
    int count = 0;
    foreach ( QString channel, channels ) {
        qLog(VoIP) << "Sending message:" << message << " to channel:" << channel;
        QtopiaIpcEnvelope e( channel, message );
        count++;
    }
    return count;
}

/*!
    Constructs a new PhoneServer attached to \a parent.
*/
PhoneServer::PhoneServer( QObject* parent )
    : QObject(parent)
{
    // Launch the third-party telephony agents.
    int serviceCount = executeTelephony( "Telephony::start()" );       // No tr
    if (!serviceCount) {
        qLog(VoIP) << "No third-party telephony agents found to send \"Telephony::start()\" to";
    }

    status = new QValueSpaceObject("/Telephony", this);

    // Create the AT-based modem service. If QTOPIA_PHONE is set we create a
    // handler for it. Possible case sensitive values are:
    // QTOPIA_PHONE=Dummy, QTOPIA_PHONE=AT, QTOPIA_PHONE=Fso, QTOPIA_PHONE=oFono
    char *env = getenv( "QTOPIA_PHONE" );
    QTelephonyService *service = 0;
    QByteArray target(env ? env : "AT");

    QList<TelephonyServiceFactory *> providers = ::qtopiaTasks<TelephonyServiceFactory>();
    foreach( TelephonyServiceFactory* factory, providers )
    {
        if ( factory->serviceName() == target ) {
            service = factory->service();
            service->setParent( this ); //take ownership as factory doesn't own it
        }

    }
    if (service) {
        service->initialize();
        serviceCount++;
    }

    status->setAttribute("AvailableServiceCount", serviceCount);
}

/*!
    Destructs the PhoneServer.
*/
PhoneServer::~PhoneServer()
{
    // Shut down the third-party telephony agents.
    executeTelephony( "Telephony::stop()" );        // No tr
    status->removeAttribute("AvailableServiceCount");
}

QTOPIA_TASK(PhoneServer, PhoneServer);
