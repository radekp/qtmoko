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

#include "gsmkeyactions.h"
#include "gsmkeyfilter.h"
#include "qabstractmessagebox.h"
#include "dialercontrol.h"
#include <QTelephonyConfiguration>
#include <QCallSettings>
#include <QSupplementaryServices>
#include <QCallBarring>
#include <QCallForwarding>
#include <QPinManager>


/*!
    \class GsmKeyActions
    \inpublicgroup QtCellModule
    \brief The GsmKeyActions class handles special GSM actions for requesting and controlling supplementary services.

    The GsmKeyActions class handles special GSM actions for requesting and
    controlling supplementary services.  The following actions are
    supported:

    \table
    \row \o \c{*#06#} \o Display the IMEI serial number of the phone.
    \row \o \c{[*]*03*ZZ*OLD_PASSWORD*NEW_PASSWORD*NEW_PASSWORD#}
         \o Change the password for supplementary service \c{ZZ}.
    \row \o \c{**04*OLD_PIN*NEW_PIN*NEW_PIN#}
         \o Change the \c{SIM PIN} to a new value.
    \row \o \c{**042*OLD_PIN*NEW_PIN*NEW_PIN#}
         \o Change the \c{SIM PIN2} to a new value.
    \row \o \c{**05*PIN_UNBLOCKING_KEY*NEW_PIN*NEW_PIN#}
         \o Unblock the \c{SIM PIN} and change it to a new value.
    \row \o \c{**052*PIN_UNBLOCKING_KEY*NEW_PIN*NEW_PIN#}
         \o Unblock the \c{SIM PIN2} and change it to a new value.
    \row \o \c{*#30#}
         \o Query the state of the caller id presentation (CLIP) service.
    \row \o \c{*#31#}
         \o Query the state of the caller id restriction (CLIR) service.
    \row \o \c{*31#number}
         \o Dial \c{number} with caller id explicitly enabled.
    \row \o \c{#31#number}
         \o Dial \c{number} with caller id explicitly disabled.
    \row \o \c{*#76#}
         \o Query the state of the connected id presentation (COLP) service.
    \endtable

    Supplementary service actions that are not recognized are sent
    to the network for processing.
    
    This class is part of the Qt Extended server and cannot be used by other Qt Extended applications.
    \internal
*/

class GsmKeyActionsPrivate
{
public:
    GsmKeyActionsPrivate()
    {
        imeiRequested = false;
        suppRequested = false;
    }

    GsmKeyFilter *filter;
    QTelephonyConfiguration *config;
    QSupplementaryServices *supp;
    bool imeiRequested;
    bool suppRequested;
    DialerControl *control;
    QString serviceTitle;
};

/*!
    Create a GSM key action handler and attach it to \a parent.
*/
GsmKeyActions::GsmKeyActions( QObject *parent )
    : QObject( parent )
{
    d = new GsmKeyActionsPrivate();

    d->filter = new GsmKeyFilter( this );

    d->config = new QTelephonyConfiguration( "modem", this );
    connect( d->config, SIGNAL(notification(QString,QString)),
             this, SLOT(imeiReply(QString,QString)) );
    d->supp = new QSupplementaryServices( "modem", this );
    connect( d->supp, SIGNAL(supplementaryServiceResult(QTelephony::Result)),
             this, SLOT(supplementaryServiceResult(QTelephony::Result)) );

    // Filter for IMEI requests.
    d->filter->addAction( "*#06#", this, SLOT(imeiRequest()) );

    // Filter for PIN, PUK, and password requests.
    d->filter->addAction
        ( QRegExp( "\\*?\\*03\\*([0-9]*\\*[0-9]*\\*[0-9]*\\*[0-9]*)#" ),
          this, SLOT(changePassword(QString)) );
    d->filter->addAction( QRegExp( "\\*\\*04\\*([0-9]*\\*[0-9]*\\*[0-9]*)#" ),
                          this, SLOT(changePin(QString)) );
    d->filter->addAction( QRegExp( "\\*\\*042\\*([0-9]*\\*[0-9]*\\*[0-9]*)#" ),
                          this, SLOT(changePin2(QString)) );
    d->filter->addAction( QRegExp( "\\*\\*05\\*([0-9]*\\*[0-9]*\\*[0-9]*)#" ),
                          this, SLOT(unblockPin(QString)) );
    d->filter->addAction( QRegExp( "\\*\\*052\\*([0-9]*\\*[0-9]*\\*[0-9]*)#" ),
                          this, SLOT(unblockPin2(QString)) );

    // Filter for caller id requests.
    d->filter->addService
        ( "31", this,
          SLOT(callerIdRestriction(GsmKeyFilter::ServiceAction,QStringList)) );

    // Filter for call forwarding requests.
    d->filter->addService
        ( "21", this,
          SLOT(callForwarding(GsmKeyFilter::ServiceAction,QStringList)) );
    d->filter->addService
        ( "67", this,
          SLOT(callForwarding(GsmKeyFilter::ServiceAction,QStringList)) );
    d->filter->addService
        ( "61", this,
          SLOT(callForwarding(GsmKeyFilter::ServiceAction,QStringList)) );
    d->filter->addService
        ( "62", this,
          SLOT(callForwarding(GsmKeyFilter::ServiceAction,QStringList)) );
    d->filter->addService
        ( "002", this,
          SLOT(callForwarding(GsmKeyFilter::ServiceAction,QStringList)) );
    d->filter->addService
        ( "004", this,
          SLOT(callForwarding(GsmKeyFilter::ServiceAction,QStringList)) );

    // Filter for call barring requests.
    d->filter->addService
        ( "33", this,
          SLOT(callBarring(GsmKeyFilter::ServiceAction,QStringList)) );
    d->filter->addService
        ( "331", this,
          SLOT(callBarring(GsmKeyFilter::ServiceAction,QStringList)) );
    d->filter->addService
        ( "332", this,
          SLOT(callBarring(GsmKeyFilter::ServiceAction,QStringList)) );
    d->filter->addService
        ( "35", this,
          SLOT(callBarring(GsmKeyFilter::ServiceAction,QStringList)) );
    d->filter->addService
        ( "351", this,
          SLOT(callBarring(GsmKeyFilter::ServiceAction,QStringList)) );
    d->filter->addService
        ( "330", this,
          SLOT(callBarring(GsmKeyFilter::ServiceAction,QStringList)) );
    d->filter->addService
        ( "333", this,
          SLOT(callBarring(GsmKeyFilter::ServiceAction,QStringList)) );
    d->filter->addService
        ( "353", this,
          SLOT(callBarring(GsmKeyFilter::ServiceAction,QStringList)) );

    // Handle call management requests.
    d->control = DialerControl::instance();
    connect( d->filter, SIGNAL(setBusy()), d->control, SLOT(sendBusy()) );
    connect( d->filter, SIGNAL(releaseHeld()), d->control, SLOT(endHeldCalls()) );
    connect( d->filter, SIGNAL(releaseActive()), d->control, SLOT(endCall()) );
    connect( d->filter, SIGNAL(releaseAllAcceptIncoming()), this, SLOT(releaseAllAcceptIncoming()) );
    connect( d->filter, SIGNAL(release(int)), d->control, SLOT(endCall(int)) );
    connect( d->filter, SIGNAL(activate(int)), d->control, SLOT(activateCall(int)) );
    connect( d->filter, SIGNAL(swap()), this, SLOT(holdOrSwap()) );
    connect( d->filter, SIGNAL(join()), d->control, SLOT(join()) );
    connect( d->filter, SIGNAL(transfer()), d->control, SLOT(transfer()) );
    connect( d->filter, SIGNAL(deflect(QString)), d->control, SLOT(deflect(QString)) );

    connect( d->control, SIGNAL(modifyDial(QDialOptions&,bool&)),
             this, SLOT(modifyDial(QDialOptions&,bool&)) );
}

/*!
    Destroy this GSM key action handler.
*/
GsmKeyActions::~GsmKeyActions()
{
    delete d;
}

/*!
    Returns true if \a number is obviously a number that should be
    dialed via a GSM network without giving the user the option
    to choose GSM vs VoIP; otherwise returns false.
*/
bool GsmKeyActions::isGsmNumber( const QString& number )
{
    // If the number starts with '*' or '#' and contains another '#',
    // then assume that this is a supplementary service request
    // which will need to be sent via a GSM network to be handled.
    if ( number.startsWith( QChar('*') ) || number.startsWith( QChar('#') ) ) {
        if ( number.indexOf( QChar('#'), 1 ) != -1 )
            return true;
    }
    return false;
}

void GsmKeyActions::imeiRequest()
{
    d->imeiRequested = true;
    d->config->request( "serial" );     // No tr
}

void GsmKeyActions::imeiReply( const QString& name, const QString& value )
{
    if ( name == "serial" && d->imeiRequested ) {   // No tr
        d->imeiRequested = false;
        QAbstractMessageBox::information
            ( 0, tr("IMEI"), value, QAbstractMessageBox::Ok );
    }
}

// Convert a supplementary service identifier into a barring type.
// This list comes from Annex B in GSM 02.30.
static QCallBarring::BarringType barringType( int service, bool& valid )
{
    valid = true;
    switch ( service ) {

        case 33:        return QCallBarring::OutgoingAll;
        case 331:       return QCallBarring::OutgoingInternational;
        case 332:       return QCallBarring::OutgoingInternationalExceptHome;
        case 35:        return QCallBarring::IncomingAll;
        case 351:       return QCallBarring::IncomingWhenRoaming;
        case 330:       return QCallBarring::AllBarringServices;
        case 333:       return QCallBarring::AllOutgoingBarringServices;
        case 353:       return QCallBarring::AllIncomingBarringServices;
        default:        valid = false; return (QCallBarring::BarringType)(-1);

    }
}

// Convert a supplementary service identifier into a call forwarding type.
// This list comes from Annex B in GSM 02.30.
static QCallForwarding::Reason forwardingType( int service, bool& valid )
{
    valid = true;
    switch ( service ) {

        case 21:        return QCallForwarding::Unconditional;
        case 67:        return QCallForwarding::MobileBusy;
        case 61:        return QCallForwarding::NoReply;
        case 62:        return QCallForwarding::NotReachable;
        case 2:         return QCallForwarding::All;
        case 4:         return QCallForwarding::AllConditional;
        default:        valid = false; return (QCallForwarding::Reason)(-1);

    }
}

// Convert a basic service group into a call class.
// From column 3 in the table in Annex C of GSM 22.030.
static QTelephony::CallClass callClass( const QString& group, bool& valid )
{
    if ( group.isEmpty() )
        return (QTelephony::CallClass)
                 ( QTelephony::CallClassDefault | QTelephony::CallClassSMS );
    switch ( group.toInt() ) {

        case 10:    return (QTelephony::CallClass)
                               ( QTelephony::CallClassDefault |
                                 QTelephony::CallClassSMS );
        case 11:    return QTelephony::CallClassVoice;
        case 12:    return QTelephony::CallClassData;
        case 13:    return QTelephony::CallClassFax;
        case 16:    return QTelephony::CallClassSMS;
        case 19:    return QTelephony::CallClassDefault;
        case 20:    return QTelephony::CallClassData;
        case 21:    return QTelephony::CallClassDedicatedPacketAccess;
        case 22:    return QTelephony::CallClassDedicatedPADAccess;
        case 24:    return QTelephony::CallClassDataCircuitSync;
        case 25:    return QTelephony::CallClassDataCircuitAsync;

    }
    valid = false;
    return QTelephony::CallClassDefault;
}

class GsmBarringAction : public QObject
{
    Q_OBJECT
public:
    GsmBarringAction( QObject *parent = 0 );
    ~GsmBarringAction();

    void requestBarringStatus( QCallBarring::BarringType type );
    void setBarringStatus( QCallBarring::BarringType type,
                           const QString& password, QTelephony::CallClass cls,
                           bool lock );
    void unlockAll( const QString& password );
    void unlockAllIncoming( const QString& password );
    void unlockAllOutgoing( const QString& password );
    void changeBarringPassword( QCallBarring::BarringType type,
                                const QString& oldPassword,
                                const QString& newPassword );

private slots:
    void barringStatus( QCallBarring::BarringType type,
                        QTelephony::CallClass cls );
    void setBarringStatusResult( QTelephony::Result result );
    void unlockResult( QTelephony::Result result );
    void changeBarringPasswordResult( QTelephony::Result result );

private:
    QCallBarring *barring;
    bool barringStatusExpected;
    bool setBarringStatusResultExpected;
    bool unlockResultExpected;
    bool changeBarringPasswordResultExpected;

    void reportBarringResult( QTelephony::Result result );
};

GsmBarringAction::GsmBarringAction( QObject *parent )
    : QObject( parent )
{
    barring = new QCallBarring( "modem", this );        // No tr
    barringStatusExpected = false;
    setBarringStatusResultExpected = false;
    unlockResultExpected = false;
    changeBarringPasswordResultExpected = false;

    connect( barring,
             SIGNAL(barringStatus(QCallBarring::BarringType,QTelephony::CallClass)),
             this,
             SLOT(barringStatus(QCallBarring::BarringType,QTelephony::CallClass)) );
    connect( barring, SIGNAL(setBarringStatusResult(QTelephony::Result)),
             this, SLOT(setBarringStatusResult(QTelephony::Result)) );
    connect( barring, SIGNAL(unlockResult(QTelephony::Result)),
             this, SLOT(unlockResult(QTelephony::Result)) );
    connect( barring, SIGNAL(changeBarringPasswordResult(QTelephony::Result)),
             this, SLOT(changeBarringPasswordResult(QTelephony::Result)) );
}

GsmBarringAction::~GsmBarringAction()
{
}

void GsmBarringAction::requestBarringStatus( QCallBarring::BarringType type )
{
    barringStatusExpected = true;
    barring->requestBarringStatus( type );
}

void GsmBarringAction::setBarringStatus
    ( QCallBarring::BarringType type,
      const QString& password, QTelephony::CallClass cls, bool lock )
{
    setBarringStatusResultExpected = true;
    barring->setBarringStatus( type, password, cls, lock );
}

void GsmBarringAction::unlockAll( const QString& password )
{
    unlockResultExpected = true;
    barring->unlockAll( password );
}

void GsmBarringAction::unlockAllIncoming( const QString& password )
{
    unlockResultExpected = true;
    barring->unlockAllIncoming( password );
}

void GsmBarringAction::unlockAllOutgoing( const QString& password )
{
    unlockResultExpected = true;
    barring->unlockAllOutgoing( password );
}

void GsmBarringAction::changeBarringPassword
    ( QCallBarring::BarringType type, const QString& oldPassword,
      const QString& newPassword )
{
    changeBarringPasswordResultExpected = true;
    barring->changeBarringPassword( type, oldPassword, newPassword );
}

void GsmBarringAction::barringStatus
    ( QCallBarring::BarringType type, QTelephony::CallClass cls )
{
    Q_UNUSED(type);
    if ( barringStatusExpected ) {
        if ( cls != QTelephony::CallClassNone ) {
            QAbstractMessageBox::information
                (0, tr("Call Barring"), tr("<p>Call barring is active."));
        } else {
            QAbstractMessageBox::information
                (0, tr("Call Barring"), tr("<p>Call barring is disabled."));
        }
        deleteLater();
    }
}

void GsmBarringAction::reportBarringResult( QTelephony::Result result )
{
    if ( result != QTelephony::OK ) {
        QAbstractMessageBox::information
            (0, tr("Call Barring"), tr("<p>An error occurred while changing the call barring settings."));
    } else {
        QAbstractMessageBox::information
            (0, tr("Call Barring"), tr("<p>Call barring request was successful."));
    }
}

void GsmBarringAction::setBarringStatusResult( QTelephony::Result result )
{
    if ( setBarringStatusResultExpected ) {
        reportBarringResult( result );
        deleteLater();
    }
}

void GsmBarringAction::unlockResult( QTelephony::Result result )
{
    Q_UNUSED(result);
    if ( unlockResultExpected ) {
        reportBarringResult( result );
        deleteLater();
    }
}

void GsmBarringAction::changeBarringPasswordResult( QTelephony::Result result )
{
    Q_UNUSED(result);
    if ( changeBarringPasswordResultExpected ) {
        reportBarringResult( result );
        deleteLater();
    }
}

void GsmKeyActions::changePassword( const QString& request )
{
    QStringList args = request.split( QChar('*') );

    // Bail out if the new passwords do not match.
    QStringList pins = args.mid(1);
    if ( !checkNewPins( tr("Change Password"), pins ) ) 
        return;

    // Determine the call barring type to be used.  We only support
    // password changes for call barring at this time.
    QCallBarring::BarringType type;
    bool valid = true;
    if ( args[0].isEmpty() )
        type = QCallBarring::AllBarringServices;
    else
        type = barringType( args[0].toInt(), valid );
    if ( !valid ) {
        // We don't know what this is, so let the network deal with it.
        d->supp->sendSupplementaryServiceData( "*03*" + request + QChar('#') );
        return;
    }

    // Perform the requested operation.
    GsmBarringAction *ba = new GsmBarringAction( this );
    ba->changeBarringPassword( type, pins[0], pins[1] );
}

class GsmChangePinAction : public QObject
{
    Q_OBJECT
public:
    GsmChangePinAction( QObject *parent = 0 );
    ~GsmChangePinAction();

    void enterPuk( const QString& type, const QString& puk,
                   const QString& pin );
    void changePin( const QString& type, const QString& oldPin,
                    const QString& newPin );

private slots:
    void pinStatus( const QString& type, QPinManager::Status status );
    void changePinResult( const QString& type, bool valid );

private:
    QPinManager *manager;
    QString expectedType;
    bool changePinExpected;
    bool enterPukExpected;
};

GsmChangePinAction::GsmChangePinAction( QObject *parent )
    : QObject( parent )
{
    manager = new QPinManager( "modem", this );     // No tr
    changePinExpected = false;
    enterPukExpected = false;
    connect( manager,
             SIGNAL(pinStatus(QString,QPinManager::Status,QPinOptions)),
             this,
             SLOT(pinStatus(QString,QPinManager::Status)) );
    connect( manager, SIGNAL(changePinResult(QString,bool)),
             this, SLOT(changePinResult(QString,bool)) );
}

GsmChangePinAction::~GsmChangePinAction()
{
}

void GsmChangePinAction::enterPuk
    ( const QString& type, const QString& puk, const QString& pin )
{
    expectedType = type;
    enterPukExpected = true;
    manager->enterPuk( type, puk, pin );
}

void GsmChangePinAction::changePin
    ( const QString& type, const QString& oldPin, const QString& newPin )
{
    expectedType = type;
    changePinExpected = true;
    manager->changePin( type, oldPin, newPin );
}

void GsmChangePinAction::pinStatus
    ( const QString& type, QPinManager::Status status )
{
    if ( enterPukExpected && type == expectedType ) {
        if ( status == QPinManager::Valid ) {
            QAbstractMessageBox::information
                (0, tr("Success"), tr("<p>Successfully changed PIN."));
        } else {
            QMessageBox::warning
                (0, tr("Failure"), tr("<p>Could not change PIN."));
        }
        deleteLater();
    }
}

void GsmChangePinAction::changePinResult( const QString& type, bool valid )
{
    if ( changePinExpected && type == expectedType ) {
        if ( valid ) {
            QAbstractMessageBox::information
                (0, tr("Success"), tr("<p>Successfully changed PIN."));
        } else {
            QMessageBox::warning
                (0, tr("Failure"), tr("<p>Could not change PIN."));
        }
        deleteLater();
    }
}

void GsmKeyActions::changePin( const QString& request )
{
    QStringList pins = request.split( QChar('*') );
    if ( checkNewPins( tr("Change SIM PIN"), pins ) ) {
        GsmChangePinAction *cp = new GsmChangePinAction( this );
        cp->changePin( "SIM PIN", pins[0], pins[1] );   // No tr
    }
}

void GsmKeyActions::changePin2( const QString& request )
{
    QStringList pins = request.split( QChar('*') );
    if ( checkNewPins( tr("Change SIM PIN2"), pins ) ) {
        GsmChangePinAction *cp = new GsmChangePinAction( this );
        cp->changePin( "SIM PIN2", pins[0], pins[1] );   // No tr
    }
}

void GsmKeyActions::unblockPin( const QString& request )
{
    QStringList pins = request.split( QChar('*') );
    if ( checkNewPins( tr("Unblock SIM PIN"), pins ) ) {
        GsmChangePinAction *cp = new GsmChangePinAction( this );
        cp->enterPuk( "SIM PUK", pins[0], pins[1] );   // No tr
    }
}

void GsmKeyActions::unblockPin2( const QString& request )
{
    QStringList pins = request.split( QChar('*') );
    if ( checkNewPins( tr("Unblock SIM PIN2"), pins ) ) {
        GsmChangePinAction *cp = new GsmChangePinAction( this );
        cp->enterPuk( "SIM PUK2", pins[0], pins[1] );   // No tr
    }
}

void GsmKeyActions::modifyDial( QDialOptions& options, bool& handledAlready )
{
    QString number = options.number();

    // See if the key filter wants to handle this number.
    if ( d->filter->filter( number, GsmKeyFilter::BeforeDial ) ) {
        handledAlready = true;
        return;
    }

    // Strip off prefixes that are used to modify the dial request.
    for (;;) {
        // GSM 02.30, Annex B indicates that *31# and #31# can be
        // used to change the caller id settings for a specific call.
        if ( number.startsWith( "*31#" ) ) {        // No tr
            options.setCallerId( QDialOptions::SendCallerId );
            number = number.mid(4);
        } else if ( number.startsWith( "#31#" ) ) { // No tr
            options.setCallerId( QDialOptions::SuppressCallerId );
            number = number.mid(4);
        } else {
            break;
        }
    }
    options.setNumber( number );

    // Check if this is an unstructured supplementary service request to be sent to the network.
    // According to the spec, we have to look for something like:
    // 1, 2 or 3 digits from the set (*, #) followed by 1X(Y), where X=any number 0‑9,
    // Y=any number 0‑9, then, optionally "*" followed by any number of any characters, and concluding with "#".
    if ( number.contains( QRegExp("^[*#]{1,3}1[0-9][0-9]?([*][^#]*)?#$") ) ) {
        d->supp->sendUnstructuredData( number );
        handledAlready = true;
        return;
    }
}

void GsmKeyActions::filterKeys( const QString& input, bool& filtered )
{
    GsmKeyFilter::Flags flags = GsmKeyFilter::Immediate;
    if ( d->control->hasIncomingCall() )
        flags |= GsmKeyFilter::Incoming;
    if ( d->control->hasActiveCalls() || d->control->hasCallsOnHold() )
        flags |= GsmKeyFilter::OnCall;
    if ( d->filter->filter( input, flags ) )
        filtered = true;
}

void GsmKeyActions::filterSelect( const QString& input, bool& filtered )
{
    GsmKeyFilter::Flags flags = GsmKeyFilter::Send;
    if ( d->control->hasIncomingCall() )
        flags |= GsmKeyFilter::Incoming;
    if ( d->control->hasActiveCalls() || d->control->hasCallsOnHold() )
        flags |= GsmKeyFilter::OnCall;
    if ( d->filter->filter( input, flags ) )
        filtered = true;
}

void GsmKeyActions::testKeys( const QString& input, bool& filterable )
{
    if ( d->filter->filter( input, GsmKeyFilter::TestOnly ) )
        filterable = true;
}

void GsmKeyActions::callerIdRestriction
        ( GsmKeyFilter::ServiceAction action, const QStringList& args )
{
    QCallSettings settings( "modem" );
    if ( action == GsmKeyFilter::Interrogate ) {
        // TODO: get the current value and display it
    } else {
        // Let the network deal with the other request types.
        sendServiceToNetwork( action, args );
    }
}

class GsmForwardingAction : public QObject
{
    Q_OBJECT
public:
    GsmForwardingAction( QObject *parent = 0 );
    ~GsmForwardingAction();

    void requestForwardingStatus( QCallForwarding::Reason reason );
    void setForwarding( QCallForwarding::Reason reason,
                        const QCallForwarding::Status& status,
                        bool enable );

private slots:
    void forwardingStatus( QCallForwarding::Reason reason,
                           const QList<QCallForwarding::Status>& status );
    void setForwardingResult
            ( QCallForwarding::Reason reason, QTelephony::Result result );

private:
    QCallForwarding *cf;
    QCallForwarding::Reason forwardingStatusExpected;
    QCallForwarding::Reason setForwardingExpected;
};

GsmForwardingAction::GsmForwardingAction( QObject *parent )
    : QObject( parent )
{
    forwardingStatusExpected = (QCallForwarding::Reason)(-1);
    setForwardingExpected = (QCallForwarding::Reason)(-1);
    cf = new QCallForwarding( "modem", this );
    connect( cf, SIGNAL(forwardingStatus(QCallForwarding::Reason,QList<QCallForwarding::Status>)),
             this, SLOT(forwardingStatus(QCallForwarding::Reason,QList<QCallForwarding::Status>)) );
    connect( cf, SIGNAL(setForwardingResult(QCallForwarding::Reason,QTelephony::Result)),
             this, SLOT(setForwardingResult(QCallForwarding::Reason,QTelephony::Result)) );
}

GsmForwardingAction::~GsmForwardingAction()
{
}

void GsmForwardingAction::requestForwardingStatus
            ( QCallForwarding::Reason reason )
{
    forwardingStatusExpected = reason;
    cf->requestForwardingStatus( reason );
}

void GsmForwardingAction::setForwarding
        ( QCallForwarding::Reason reason,
          const QCallForwarding::Status& status, bool enable )
{
    setForwardingExpected = reason;
    cf->setForwarding( reason, status, enable );
}

void GsmForwardingAction::forwardingStatus
        ( QCallForwarding::Reason reason,
          const QList<QCallForwarding::Status>& status )
{
    if ( reason == forwardingStatusExpected ) {
        if ( status.isEmpty() ) {
            QAbstractMessageBox::information
                (0, tr("Call Forwarding"), tr("<p>Call forwarding is not active."));
        } else {
            QAbstractMessageBox::information
                (0, tr("Call Forwarding"), tr("<p>Call forwarding is active and forwarding to %1.").arg(status[0].number) );
        }
        deleteLater();
    }
}

void GsmForwardingAction::setForwardingResult
        ( QCallForwarding::Reason reason, QTelephony::Result result )
{
    if ( reason == setForwardingExpected ) {
        if ( result != QTelephony::OK ) {
            QAbstractMessageBox::information
                (0, tr("Call Forwarding"), tr("<p>An error occurred while changing the call forwarding settings."));
        } else {
            QAbstractMessageBox::information
                (0, tr("Call Forwarding"), tr("<p>Call forwarding request was successful."));
        }
        deleteLater();
    }
}

void GsmKeyActions::callForwarding
        ( GsmKeyFilter::ServiceAction action, const QStringList& args )
{
    // Extract the arguments for the request.
    QCallForwarding::Status status;
    status.number = ( args.size() >= 2 ? args[1] : QString() );
    QString group = ( args.size() >= 3 ? args[2] : QString() );
    status.time = ( args.size() >= 4 ? args[3].toInt() : 0 );
    bool valid;
    QCallForwarding::Reason reason = forwardingType( args[0].toInt(), valid );
    status.cls = callClass( group, valid );
    if ( !valid ) {
        // We don't know what the call class is, so it may be one of
        // the operator-specific extension classes.  Send to the network.
        sendServiceToNetwork( action, args, tr("Call Forwarding") );
        return;
    }

    // Determine what kind of operation to perform.
    GsmForwardingAction *fa;
    switch ( action ) {

        case GsmKeyFilter::Activate:
        {
            // Turn on call forwarding of this type.
            fa = new GsmForwardingAction( this );
            fa->setForwarding( reason, status, true );
        }
        break;

        case GsmKeyFilter::Deactivate:
        {
            // Turn off call forwarding of this type.
            fa = new GsmForwardingAction( this );
            fa->setForwarding( reason, status, false );
        }
        break;

        case GsmKeyFilter::Interrogate:
        {
            // Get the current status of call forwarding.
            fa = new GsmForwardingAction( this );
            fa->requestForwardingStatus( reason );
        }
        break;

        case GsmKeyFilter::Registration:
        case GsmKeyFilter::Erasure:
        {
            // Pass registration and erasure requests direct to the network.
            sendServiceToNetwork( action, args, tr("Call Forwarding") );
        }
        break;
    }
}

void GsmKeyActions::callBarring
        ( GsmKeyFilter::ServiceAction action, const QStringList& args )
{
    // Extract the arguments for the request.
    QString password = ( args.size() >= 2 ? args[1] : QString() );
    QString group = ( args.size() >= 3 ? args[2] : QString() );
    bool valid;
    QCallBarring::BarringType type = barringType( args[0].toInt(), valid );
    QTelephony::CallClass cls = callClass( group, valid );
    if ( !valid ) {
        // We don't know what the call class is, so it may be one of
        // the operator-specific extension classes.  Send to the network.
        sendServiceToNetwork( action, args, tr("Call Barring") );
        return;
    }

    // Determine what kind of operation to perform.
    GsmBarringAction *ba;
    switch ( action ) {
        case GsmKeyFilter::Activate:
        case GsmKeyFilter::Registration:
        {
            // Turn on the call barring lock of this type.
            ba = new GsmBarringAction( this );
            ba->setBarringStatus( type, password, cls, true );
        }
        break;

        case GsmKeyFilter::Deactivate:
        case GsmKeyFilter::Erasure:
        {
            // Turn off the call barring lock of this type.
            ba = new GsmBarringAction( this );
            if ( type == QCallBarring::AllBarringServices )
                ba->unlockAll( password );
            else if ( type == QCallBarring::AllOutgoingBarringServices )
                ba->unlockAllOutgoing( password );
            else if ( type == QCallBarring::AllIncomingBarringServices )
                ba->unlockAllIncoming( password );
            else
                ba->setBarringStatus( type, password, cls, false );
        }
        break;

        case GsmKeyFilter::Interrogate:
        {
            // Get the current status of the call barring lock.
            ba = new GsmBarringAction( this );
            ba->requestBarringStatus( type );
        }
        break;
    }
}

void GsmKeyActions::callerIdPresentation
    ( GsmKeyFilter::ServiceAction action, const QStringList& args )
{
    if ( action == GsmKeyFilter::Interrogate ) {
        // TODO: get the current value and display it
    } else {
        // Let the network deal with the other request types.
        sendServiceToNetwork( action, args );
    }
}

void GsmKeyActions::connectedIdPresentation
    ( GsmKeyFilter::ServiceAction action, const QStringList& args )
{
    if ( action == GsmKeyFilter::Interrogate ) {
        // TODO: get the current value and display it
    } else {
        // Let the network deal with the other request types.
        sendServiceToNetwork( action, args );
    }
}

void GsmKeyActions::holdOrSwap()
{
    if ( d->control->hasCallsOnHold() )
        d->control->unhold();
    else
        d->control->hold();
}

void GsmKeyActions::releaseAllAcceptIncoming()
{
    if ( d->control->hasActiveCalls() )
        d->control->endCall();
    d->control->accept();
}

bool GsmKeyActions::checkNewPins
    ( const QString& title, const QStringList& pins )
{
    if ( pins[1] != pins[2] ) {
        QAbstractMessageBox::critical
            ( 0, title, tr("Mismatch: Retry new PIN"),
              QAbstractMessageBox::Ok );
        return false;
    } else {
        return true;
    }
}

// Send a supplementary service request to the network because while a
// filter activated for it, it cannot be handled directly by us.
void GsmKeyActions::sendServiceToNetwork
    ( GsmKeyFilter::ServiceAction action, const QStringList& args,
      const QString& title )
{
    QString data;
    switch ( action ) {
        case GsmKeyFilter::Activate:        data = "*"; break;
        case GsmKeyFilter::Deactivate:      data = "#"; break;
        case GsmKeyFilter::Interrogate:     data = "*#"; break;
        case GsmKeyFilter::Registration:    data = "**"; break;
        case GsmKeyFilter::Erasure:         data = "##"; break;
    }
    data += args.join( QChar('*') );
    data += QChar('#');
    d->suppRequested = true;
    if ( !title.isEmpty() )
        d->serviceTitle = title;
    else
        d->serviceTitle = tr("Network Service");
    d->supp->sendSupplementaryServiceData( data );
}

void GsmKeyActions::supplementaryServiceResult( QTelephony::Result result )
{
    if ( d->suppRequested ) {
        d->suppRequested = false;
        if ( result != QTelephony::OK ) {
            QAbstractMessageBox::information
                (0, d->serviceTitle, tr("<p>An error occurred while requesting the service."));
        } else {
            QAbstractMessageBox::information
                (0, d->serviceTitle, tr("<p>Service request was successful."));
        }
    }
}

#include "gsmkeyactions.moc"
