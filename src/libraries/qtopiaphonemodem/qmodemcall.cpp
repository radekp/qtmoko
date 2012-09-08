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

#include <qmodemcall.h>
#include <qmodemllindicators.h>
#include <qserialiodevicemultiplexer.h>
#include <qatchat.h>
#include <qatutils.h>
#include <qatresult.h>
#include <qatresultparser.h>
#include <qserialsocket.h>
#include "qmodempppdmanager_p.h"
#include <qtopialog.h>
#include <qvaluespace.h>
#include <qtimer.h>
#include <QApplication>

/*!
    \class QModemCall
    \inpublicgroup QtCellModule

    \brief The QModemCall class implements phone call functionality for AT-based modems.
    \ingroup telephony::modem

    QModemCall inherits from QPhoneCallImpl to provide the back end server
    infrastructure for QPhoneCall.

    Client applications should use QPhoneCall and QPhoneCallManager to make
    and receive phone calls.  The QModemCall class is intended for the
    server-side \c modem telephony service.

    QModemCall instances are created by the QModemCallProvider::create()
    function.  If a modem vendor plug-in needs to change some of the
    functionality in this class, they should do the following:

    \list
        \o Inherit from QModemCall and override the functionality that is different.
        \o Inherit from QModemCallProvider and override QModemCallProvider::create()
           to instantiate the new class that inherits from QModemCall.
    \endlist

    The following example demonstrates overriding the hold() method
    to use a proprietary modem command instead of the standard
    \c{AT+CHLD=2}:

    \code
    class MyModemCall : public QModemCall
    {
        Q_OBJECT
    public:
        MyModemCall( QModemCallProvider *provider,
                     const QString& identifier,
                     const QString& callType )
            : QModemCall( provider, identifier, callType ) {}

        void hold();
    }

    void MyModemCall::hold()
    {
        if ( state() == QPhoneCall::Connected ) {
            provider()->atchat()->chat( "AT*XHLD=2" );
            setActions( QPhoneCallImpl::ActivateCall );
            setState( QPhoneCall::Hold );
        }
    }
    \endcode

    For voice calls, there is an alternative way to modify the AT commands
    that correspond to voice call operations.  Methods in QModemCallProvider()
    such as QModemCallProvider::dialVoiceCommand(),
    QModemCallProvider::putOnHoldCommand(), etc. can be overridden to
    change just the AT command without modifying the rest of the logic in
    QModemCall.  This will usually be an easier way to modify
    the AT commands than inheriting from QModemCall.

    \sa QPhoneCallImpl, QPhoneCall, QModemDataCall, QModemCallProvider
*/

class QModemCallPrivate
{
public:
    QModemCallProvider *provider;
    uint modemIdentifier;
    QValueSpaceObject *modemIdObject;
};

/*!
    Constructs a new modem call that is attached to \a provider and
    is associated with a globally-unique \a identifier.  The \a callType
    specifies the type of call (\c Voice, \c Data, \c Fax, etc).

    This constructor is called from QModemCallProvider::create() or from
    a subclass implementation that overrides QModemCallProvider::create().
*/
QModemCall::QModemCall
        ( QModemCallProvider *provider, const QString& identifier,
          const QString& callType )
    : QPhoneCallImpl( provider, identifier, callType )
{
    d = new QModemCallPrivate();
    d->provider = provider;
    d->modemIdentifier = 0;
    d->modemIdObject = new QValueSpaceObject
        ( "/Communications/QPhoneCallProvider/ModemIdentifiers", this );
}


/*!
    Destroys this modem call object.
*/
QModemCall::~QModemCall()
{
    d->provider->releaseModemIdentifier( d->modemIdentifier );
    delete d;
}

/*!
    \reimp
*/
void QModemCall::dial( const QDialOptions& options )
{
    QString number = options.number();
    setNumber( number );

    // Assign an identifier to this call.
    setModemIdentifier( provider()->nextModemIdentifier() );

    // If the number starts with '*' or '#', then this is a request
    // for a supplementary service, not an actual phone call.
    // So we dial and then immediately hang up, allowing the network
    // to send us the SS/USSD response when it is ready.
    if ( number.startsWith("*") || number.startsWith("#") ) {
        provider()->atchat()->chat
            ( provider()->dialServiceCommand( options ) );
        setState( QPhoneCall::ServiceHangup );
        return;
    }

    // If we have both active and held calls, then we cannot dial at all.
    if ( provider()->hasGroup( QPhoneCall::Connected ) &&
         provider()->hasGroup( QPhoneCall::Hold ) ) {
        setState( QPhoneCall::OtherFailure );
        return;
    }

    // Send the ATD command to the device.
    provider()->atchat()->chat
            ( provider()->dialVoiceCommand( options ),
              this, SLOT(dialRequestDone(bool)) );

    // All other calls that were connected are now on hold.
    provider()->beginStateTransaction();
    provider()->changeGroup( QPhoneCall::Connected, QPhoneCall::Hold,
                             QPhoneCallImpl::None );

    // No actions are possible except tone() until we connect.
    // Tones will be queued up for sending once connected.
    setActions( QPhoneCallImpl::Tone );

    // Change to the "Dialing" state and notify everyone who is interested.
    setState( QPhoneCall::Dialing );
    provider()->endStateTransaction();
}

/*!
    \reimp
*/
void QModemCall::hangup( QPhoneCall::Scope scope )
{
    qLog(Modem) << "QModemCall::hangup()";
    QModemLLIndicators::Hangup();
    
    if ( state() == QPhoneCall::Connected ||
         state() == QPhoneCall::Dialing ||
         state() == QPhoneCall::Alerting ) {
        // Hangup an active call.
        if ( scope == QPhoneCall::CallOnly ) {
            if ( state() == QPhoneCall::Dialing ||
                 state() == QPhoneCall::Alerting ) {
                provider()->abortDial( modemIdentifier(), scope );
            } else {
                provider()->atchat()->chat
                    ( provider()->releaseCallCommand( modemIdentifier() ) );
            }
            qLog(Modem) << "hung up the call";
            setState( QPhoneCall::HangupLocal );
        } else if ( provider()->hasGroup( QPhoneCall::Incoming ) ) {
            if ( state() == QPhoneCall::Dialing ||
                 state() == QPhoneCall::Alerting ) {
                provider()->abortDial( modemIdentifier(), scope );
                qLog(Modem) << "hung up the dialing call while there was an incoming call";
                setState( QPhoneCall::HangupLocal );
            } else {
                // We have to release the calls in the group by their specific id's
                // to avoid accidentally accepting the incoming call with AT+CHLD=1.
                qLog(Modem) << "hangup groups while there is an incoming call";
                QList<QPhoneCallImpl *> calls = provider()->calls();
                QList<QPhoneCallImpl *>::ConstIterator iter;
                QModemCall *callp;
                provider()->beginStateTransaction();
                for ( iter = calls.begin(); iter != calls.end(); ++iter ) {
                    callp = (QModemCall *)*iter;
                    if ( callp->state() == QPhoneCall::Connected ) {
                        provider()->atchat()->chat
                            ( provider()->releaseCallCommand( callp->modemIdentifier() ) );
                        callp->setState( QPhoneCall::HangupLocal );
                    }
                }
                provider()->endStateTransaction();
            }
        } else {
            if ( state() == QPhoneCall::Dialing ||
                 state() == QPhoneCall::Alerting ) {
                provider()->abortDial( modemIdentifier(), scope );
            } else {
                provider()->atchat()->chat
                    ( provider()->releaseActiveCallsCommand() );
            }
            qLog(Modem) << "hangup groups";
            provider()->beginStateTransaction();
            provider()->changeGroup( QPhoneCall::Connected, QPhoneCall::HangupLocal, QPhoneCallImpl::None );
            provider()->changeGroup( QPhoneCall::Dialing, QPhoneCall::HangupLocal, QPhoneCallImpl::None );
            provider()->changeGroup( QPhoneCall::Alerting, QPhoneCall::HangupLocal, QPhoneCallImpl::None );
            provider()->activateWaitingOrHeld();
            provider()->endStateTransaction();
        }

    } else if ( state() == QPhoneCall::Hold ) {

        // Hangup a call that is currently on hold.
        if ( scope == QPhoneCall::CallOnly ) {
            provider()->atchat()->chat
                ( provider()->releaseCallCommand( modemIdentifier() ) );
            setState( QPhoneCall::HangupLocal );
        } else if ( !provider()->hasGroup( QPhoneCall::Incoming ) ) {
            provider()->atchat()->chat
                ( provider()->releaseHeldCallsCommand() );
            provider()->beginStateTransaction();
            provider()->changeGroup( QPhoneCall::Hold, QPhoneCall::HangupLocal, QPhoneCallImpl::None );
            provider()->endStateTransaction();
        } else {
            // There is a waiting call.  We cannot use "AT+CHLD=0" or
            // it will hangup the waiting call rather than release the
            // held calls.  So we have to instead iterate over all
            // held calls and close them one by one.
            QList<QPhoneCallImpl *> calls = provider()->calls();
            QList<QPhoneCallImpl *>::ConstIterator iter;
            QModemCall *callp;
            provider()->beginStateTransaction();
            for ( iter = calls.begin(); iter != calls.end(); ++iter ) {
                callp = (QModemCall *)*iter;
                if ( callp->state() == QPhoneCall::Hold ) {
                    provider()->atchat()->chat
                        ( provider()->releaseCallCommand( callp->modemIdentifier() ) );
                    callp->setState( QPhoneCall::HangupLocal );
                }
            }
            provider()->endStateTransaction();
        }

    } else if ( state() == QPhoneCall::Incoming ) {

        // Reject an incoming call and set the busy state for the caller.
        provider()->atchat()->chat( provider()->setBusyCommand() );
        setState( QPhoneCall::HangupLocal );

    }
}

/*!
    \reimp
*/
void QModemCall::accept()
{
    QModemLLIndicators::Accept();
    
    // Bail out if this is not the currently registered incoming call.
    if ( state() != QPhoneCall::Incoming ) {
        qLog(Modem) << "QModemCall::accept(): Not the currently registered call";
        return;
    }

    // Stop the ring and clip timers.
    provider()->stopRingTimers();

    // Send the appropriate command to accept the call.
    QString command;
    if ( provider()->hasGroup( QPhoneCall::Connected ) ||
         provider()->hasGroup( QPhoneCall::Hold ) ) {
        command = provider()->acceptCallCommand( true );
    } else {
        command = provider()->acceptCallCommand( false );
    }
    provider()->atchat()->chat( command, this, SLOT(acceptDone(bool)) );

    if( provider()->partOfHoldGroup( callType() ) )
    {
        provider()->beginStateTransaction();

        // All active calls will be put on hold by this operation.
        provider()->changeGroup( QPhoneCall::Connected, QPhoneCall::Hold,
                                 QPhoneCallImpl::ActivateGroup |
                                 QPhoneCallImpl::Join |
                                 QPhoneCallImpl::JoinAndDetach );

        // Set the actions that can be performed.
        QPhoneCallImpl::Actions actions;
        actions = QPhoneCallImpl::Tone | QPhoneCallImpl::Transfer;
        if ( !provider()->hasGroup( QPhoneCall::Hold ) )
            actions |= QPhoneCallImpl::Hold;
        else
            actions |= QPhoneCallImpl::Join | QPhoneCallImpl::JoinAndDetach;
        setActions( actions );

        // This call is now the active one.
        setState( QPhoneCall::Connected );

        provider()->endStateTransaction();
    }
}

class QHoldUserData : public QAtResult::UserData
{
public:
    QHoldUserData( const QString& command )
    { this->command = command; }

    QString command;
};

/*!
    \reimp
*/
void QModemCall::hold()
{
    if ( state() == QPhoneCall::Connected ) {

        // Put the active calls on hold and activate the waiting or held call.
        provider()->atchat()->chat( provider()->putOnHoldCommand(),
                                    this, SLOT(holdRequestDone(bool)) );

    }
}

/*!
    \reimp
*/
void QModemCall::activate( QPhoneCall::Scope scope )
{
    // Bail out if the call is not on hold or there is an incoming call.
    if ( state() != QPhoneCall::Hold && state() != QPhoneCall::Connected
            || provider()->incomingCall() != 0 )
        return;

    // Single or group operation?
    if ( scope == QPhoneCall::CallOnly ) {

        // Place everything on hold except for this call.
        // Note: Some Ericsson phones seem to have a bug where "AT+CHLD=2n"
        // cannot be used to resume a held call if it is the only call
        // in the system; "AT+CHLD=2" must be used instead.
        bool otherCalls = provider()->otherActiveCalls( this );
        provider()->atchat()->chat
            ( provider()->activateCallCommand
                    ( modemIdentifier(), otherCalls ),
              this, SLOT(activateSingleRequestDone(bool)) );

    } else {

        // Swap the held and active calls.
        provider()->atchat()->chat
            ( provider()->activateHeldCallsCommand(),
              this, SLOT(activateMultipleRequestDone(bool)) );

    }
}

void QModemCall::activateSingleRequestDone( bool ok )
{
    if ( ok ) {
        bool otherCalls = provider()->otherActiveCalls( this );
        provider()->beginStateTransaction();

        // Update the state of all calls to match the new conditions.
        provider()->changeGroup( QPhoneCall::Hold, QPhoneCall::Hold,
                                 QPhoneCallImpl::ActivateGroup |
                                 QPhoneCallImpl::Join |
                                 QPhoneCallImpl::JoinAndDetach );
        provider()->changeGroup( QPhoneCall::Connected, QPhoneCall::Hold,
                                 QPhoneCallImpl::ActivateGroup |
                                 QPhoneCallImpl::Join |
                                 QPhoneCallImpl::JoinAndDetach );
        if ( otherCalls ) {
            setActions( QPhoneCallImpl::Tone |
                        QPhoneCallImpl::Transfer );
        } else {
            setActions( QPhoneCallImpl::Tone |
                        QPhoneCallImpl::Transfer |
                        QPhoneCallImpl::Hold );
        }
        setState( QPhoneCall::Connected );

        provider()->endStateTransaction();
    } else {
        emit requestFailed( QPhoneCall::ActivateFailed );
    }
}

void QModemCall::activateMultipleRequestDone( bool ok )
{
    if ( ok ) {
        provider()->beginStateTransaction();
        provider()->changeGroup( QPhoneCall::Connected, (QPhoneCall::State)(-1), QPhoneCallImpl::None );
        provider()->activateWaitingOrHeld();
        provider()->changeGroup( (QPhoneCall::State)(-1), QPhoneCall::Hold,
                                 QPhoneCallImpl::ActivateGroup |
                                 QPhoneCallImpl::Join |
                                 QPhoneCallImpl::JoinAndDetach );
        provider()->endStateTransaction();
    } else {
        emit requestFailed( QPhoneCall::ActivateFailed );
    }
}

/*!
    \reimp
*/
void QModemCall::join( bool detachSubscriber )
{
    if ( state() == QPhoneCall::Connected || state() == QPhoneCall::Hold ) {
        if ( detachSubscriber ) {
            provider()->atchat()->chat
                ( provider()->joinCallsCommand( detachSubscriber ),
                  this, SLOT(detachRequestDone(bool)) );
        } else {
            provider()->atchat()->chat
                ( provider()->joinCallsCommand( detachSubscriber ),
                  this, SLOT(joinRequestDone(bool)) );
        }
    }
}


static QString formatTones( QString tones, int vtsType )
{
    QString expanded = "AT+VTS=";
    int posn;
    if ( vtsType == 1 )
        expanded += '"';
    expanded += tones[0];
    posn = 1;
    while ( posn < tones.length() ) {
        if ( vtsType == 1 ) {
            expanded += "\";+VTS=\"";
        } else {
            expanded += ";+VTS=";
        }
        expanded += tones[posn];
        ++posn;
    }
    if ( vtsType == 1 )
        expanded += '"';
    return expanded;
}


class QToneUserData : public QAtResult::UserData
{
public:
    QToneUserData( const QString& tones ) { this->tones = tones; }

    QString tones;
};

/*!
    \reimp
*/
void QModemCall::tone( const QString& tones )
{
    if ( state() == QPhoneCall::Connected && tones.length() > 0 ) {
        provider()->atchat()->chat( formatTones( tones, provider()->vtsType() ),
                              this, SLOT(vtsRequestDone(bool,QAtResult)),
                              new QToneUserData( tones )
#ifdef QT_QWS_GTA04
	// The GTA04 modem doesn't provide any response to AT+VTS, so
	// don't wait for a response.
			      , true
#endif
			      );
    }
}

void QModemCall::vtsRequestDone( bool ok, const QAtResult& result )
{
    if ( ok || provider()->vtsType() == 1 )
        return;

    // It looks like that the phone requires DTMF tones to
    // be quoted or use ";+VTS=" as the separator.
    // The spec is unclear and there are phones that do it both ways.
    provider()->setVtsType( provider()->vtsType() + 1 );

    // Re-send the command with quotes inserted.
    QString tones = ((QToneUserData *)result.userData())->tones;
    provider()->atchat()->chat( formatTones( tones, provider()->vtsType() ),
                          this, SLOT(vtsRequestDone(bool,QAtResult)),
                          new QToneUserData( tones ) );
}

/*!
    \reimp
*/
void QModemCall::transfer( const QString& number )
{
    if ( state() == QPhoneCall::Incoming ) {

        // Transfer the incoming call to a new number.
        provider()->atchat()->chat( "AT+CTFR=" + QAtUtils::encodeNumber( number ) );
        setState( QPhoneCall::HangupLocal );

    } else if ( state() == QPhoneCall::Connected &&
                provider()->incomingCall() != 0 ) {

        // Transfer the active calls to a new number.  We cannot do
        // this if there is an incoming call because "AT+CTFR" will
        // work on the incoming call rather than the active call.
        provider()->atchat()->chat( "AT+CTFR=" + number );
        provider()->beginStateTransaction();
        provider()->changeGroup( QPhoneCall::Connected, QPhoneCall::HangupLocal, QPhoneCallImpl::None );
        provider()->changeGroup( QPhoneCall::Hold, QPhoneCall::Connected,
                                 QPhoneCallImpl::Hold |
                                 QPhoneCallImpl::Tone |
                                 QPhoneCallImpl::Transfer );
        provider()->endStateTransaction();

    }
}

void QModemCall::acceptDone( bool ok )
{
    if ( !ok ) {

        // "ATA" failed, so the connection was probably hung up
        // by the remote caller while we were sending the command.
        provider()->missedTimeout( this );

    }
}

void QModemCall::dialRequestDone( bool ok )
{
    qLog(Modem) << "QModemCall::dialRequestDone()";
    if ( state() != QPhoneCall::Dialing && state() != QPhoneCall::Alerting ) {
        // If the state has already transitioned away from Dialing,
        // then we have probably already seen a connect transition,
        // the other party is busy/unavailable, or the call was
        // aborted by hangup().
        return;
    }

    QModemCallProvider::AtdBehavior ab = provider()->atdBehavior();
    if (ab == QModemCallProvider::AtdOkIgnore) {
        qLog(Modem) << "ignore the ATD result: " << ok;
        return;
    }

    if ( ok ) {
        // The ATD could respond with OK if it has actually connected,
        // or if it has simply returned to command mode.  We now need
        // to figure out what happened.  Note: if atdBehavior() returns
        // AtdOkIsDialingWithStatus, we wait until the modem vendor
        // plug-in tells us that the transition has occurred.
        if ( ab == QModemCallProvider::AtdOkIsConnect ) {
            qLog(Modem) << "dialing was ok, we are connected";
            setConnected();
        } else if ( ab == QModemCallProvider::AtdUnknown ||
                    ab == QModemCallProvider::AtdOkIsDialing ) {
            dialCheck();
        }
    } else {
        qLog(Modem) << "dialing the call failed, could not connect. hanging up.";
        // The dial command failed, probably because we couldn't connect.
        setState( QPhoneCall::HangupLocal );
    }
}

void QModemCall::dialCheck()
{
    provider()->atchat()->chat
        ( "AT+CLCC", this, SLOT(clccDialCheck(bool,QAtResult)) );
}

void QModemCall::clccDialCheck( bool, const QAtResult& result )
{
    // Bail out if the call is no longer dialing because it was aborted.
    if ( state() != QPhoneCall::Dialing && state() != QPhoneCall::Alerting )
        return;

    // Find the current state of the call in the AT+CLCC results.
    QAtResultParser parser( result );
    while ( parser.next( "+CLCC:" ) ) {
        uint id = parser.readNumeric();
        parser.readNumeric();       // Don't need the direction code.
        uint state = parser.readNumeric();
        if ( id != modemIdentifier() )
            continue;               // Not the call we are interested in.
        if ( state == 2 ) {
            // The call is still dialing, so retry in a second.
            QTimer::singleShot( 1000, this, SLOT(dialCheck()) );
            return;
        } else if ( state == 0 ) {
            // The call is now connected.
            qLog(Modem) << "dialing was ok, we are connected";
            setConnected();
            return;
        } else if ( state == 1 ) {
            // The call is connected, but on hold.
            qLog(Modem) << "dialing was ok, we are on hold";
            setState( QPhoneCall::Hold );
            return;
        }
    }

    // If we get here, then AT+CLCC failed.  Assume that it
    // does not work on this device and that the call is connected.
    // Hopefully we will get "NO CARRIER", "BUSY", or something similar
    // to tell us when the dial attempt fails.
    qLog(Modem) << "dialing was ok, we are assuming connected";
    setConnected();
}

void QModemCall::joinRequestDone( bool ok )
{
    if ( ok ) {
        provider()->beginStateTransaction();
        provider()->changeGroup( QPhoneCall::Connected, QPhoneCall::Connected,
                                 QPhoneCallImpl::Hold |
                                 QPhoneCallImpl::Tone |
                                 QPhoneCallImpl::Transfer );
        provider()->changeGroup( QPhoneCall::Hold, QPhoneCall::Connected,
                                 QPhoneCallImpl::Hold |
                                 QPhoneCallImpl::Tone |
                                 QPhoneCallImpl::Transfer );
        provider()->endStateTransaction();
    } else {
        emit requestFailed( QPhoneCall::JoinFailed );
    }
}

void QModemCall::detachRequestDone( bool ok )
{
    if ( ok ) {
        provider()->beginStateTransaction();
        provider()->changeGroup( QPhoneCall::Connected, QPhoneCall::HangupLocal, QPhoneCallImpl::None );
        provider()->changeGroup( QPhoneCall::Hold, QPhoneCall::HangupLocal, QPhoneCallImpl::None );
        provider()->endStateTransaction();
    } else {
        emit requestFailed( QPhoneCall::JoinFailed );
    }
}

void QModemCall::holdRequestDone( bool ok )
{
    if ( ok ) {
        provider()->beginStateTransaction();
        provider()->changeGroup( QPhoneCall::Connected, (QPhoneCall::State)(-1), QPhoneCallImpl::None );
        provider()->activateWaitingOrHeld();
        provider()->changeGroup( (QPhoneCall::State)(-1), QPhoneCall::Hold,
                                 QPhoneCallImpl::ActivateCall |
                                 QPhoneCallImpl::ActivateGroup );
        provider()->endStateTransaction();
    } else {
        emit requestFailed( QPhoneCall::HoldFailed );
    }
}

/*!
    Returns the modem identifier associated with this call.  Modem identifiers
    are used with commands like \c{AT+CHLD} to identify specific calls.
    Returns zero if the modem identifier has not been assigned yet.

    \sa setModemIdentifier(), QPhoneCall::modemIdentifier()
*/
uint QModemCall::modemIdentifier() const
{
    return d->modemIdentifier;
}

/*!
    Sets the modem identifier associated with this call to \a id.
    The QModemCallProvider::nextModemIdentifier() function can be
    used to assign the next call identifier in rotation.

    \sa modemIdentifier(), QPhoneCall::modemIdentifier()
*/
void QModemCall::setModemIdentifier( uint id )
{
    d->modemIdentifier = id;
    if ( id != 0 )
        d->modemIdObject->setAttribute( identifier(), (int)id );
    else
        d->modemIdObject->removeAttribute( identifier() );
}

/*!
    Returns the QModemCallProvider instance associated with this call object.
*/
QModemCallProvider *QModemCall::provider() const
{
    return d->provider;
}

/*!
    Called by modem vendor plug-ins to indicate that a call has
    transitioned from \c Dialing or \c Alerting to \c Connected.

    The QModemCall class adjusts the call's state() and actions()
    appropriately and emits the relevant signals.  The state() and
    actions() on other calls may also be affected, indicating new
    actions that can be taken now that there are several calls in
    the system.

    \sa state(), actions()
*/
void QModemCall::setConnected()
{
    QPhoneCallImpl::Actions actions;
    provider()->beginStateTransaction();
    actions = QPhoneCallImpl::Tone | QPhoneCallImpl::Transfer;
    if ( provider()->hasGroup( QPhoneCall::Hold ) ) {
        provider()->changeGroup( QPhoneCall::Hold, QPhoneCall::Hold,
                                 QPhoneCallImpl::ActivateGroup |
                                 QPhoneCallImpl::Join |
                                 QPhoneCallImpl::JoinAndDetach );
        actions |= QPhoneCallImpl::Join | QPhoneCallImpl::JoinAndDetach;
    } else {
        actions |= QPhoneCallImpl::Hold;
    }
    setActions( actions );
    setState( QPhoneCall::Connected );
    provider()->endStateTransaction();
}

/*!
    \reimp
*/
void QModemCall::setState( QPhoneCall::State value )
{
    if ( value >= QPhoneCall::HangupLocal ) {
        // Remove the modem identifier from the value space.
        if ( d->modemIdentifier != 0 )
            d->modemIdObject->removeAttribute( identifier() );

        // If this was an incoming call, then stop the ring timers for it.
        if ( state() == QPhoneCall::Incoming )
            provider()->stopRingTimers();
    }
    QPhoneCallImpl::setState( value );
}

/*!
    \class QModemDataCall
    \inpublicgroup QtCellModule

    \brief The QModemDataCall class implements data call functionality for AT-based modems.
    \ingroup telephony::modem

    QModemDataCall inherits from QModemCall to provide specific behaviors for
    data calls within the server-side \c modem telephony service.

    Client applications should use QPhoneCall and QPhoneCallManager to make
    and receive data calls.  The QModemDataCall class is intended for the
    server-side \c modem telephony service.

    QModemDataCall instances are created by the QModemCallProvider::create()
    function.  If a modem vendor plug-in needs to change some of the
    functionality in this class, they should inherit a new class from
    QModemDataCall and override QModemCallProvider::create() to instantiate
    this new class.  See the description of QModemCall for more information.

    \sa QPhoneCallImpl, QPhoneCall, QModemCall, QModemCallProvider
*/

class QModemDataCallPrivate
{
public:
    QSerialIODevice *channel;
    QSerialIODevice *setup;
    bool hangingUp;
    QDialOptions options;
    int faxClass;
    QSerialSocketServer *server;
    QSerialSocket *socket;
    char *buf;
    QModemPPPdManager *pppdManager;
    int contextSetupCounter;
};

/*!
    Construct a new modem data call that is attached to \a provider and
    is associated with a globally-unique \a identifier.  The \a callType
    specifies the type of call (\c Data, \c Fax, etc).
*/
QModemDataCall::QModemDataCall
        ( QModemCallProvider *provider, const QString& identifier,
          const QString& callType )
    : QModemCall( provider, identifier, callType )
{
    d = new QModemDataCallPrivate();
    d->channel = 0;
    d->setup = 0;
    d->hangingUp = false;
    d->faxClass = 0;
    d->server = 0;
    d->socket = 0;
    d->buf = 0;
    d->pppdManager = 0;
    d->contextSetupCounter = 0;
}

/*!
    Destroy this modem data call object.
*/
QModemDataCall::~QModemDataCall()
{
    destroyServer();
    if ( d->buf )
        delete d->buf;
    delete d;
}

void QModemDataCall::createChannels()
{
    // Find a multiplexing channel to use to effect the dial or accept.
    // Try "aux" first and then "data", because it is possible that
    // "data" will need to be used separately for GPRS in the near
    // future and we don't want to block off that slot unnecessarily.
    d->channel = provider()->multiplexer()->channel( "aux" );
    if ( !d->channel || d->channel->isOpen() ) {

        // No "aux" channel, or it is already in use, so try "data".
        d->channel = provider()->multiplexer()->channel( "data" );
        if ( !d->channel || d->channel->isOpen() ) {

            // The "data" channel is not present, or open, so we cannot dial.
            qLog(Modem) << "No multiplexing channels available for data call";
            setState( QPhoneCall::OtherFailure );
            return;

        }

        // Get the setup channel to use with "data".
        d->setup = provider()->multiplexer()->channel( "datasetup" );

    } else {

        // If we are using the "aux" channel, it is also the setup channel.
        d->setup = d->channel;

    }
}

/*!
    \reimp
*/
void QModemDataCall::dial( const QDialOptions& options )
{
    // Ignore the request if a call is already active.
    if ( d->channel )
        return;

    // Save the dial options for later.
    d->options = options;
    setNumber( options.number() );

    // Assign an identifier to this call if not demand-dialing.
    if ( !options.ipDemandDialing() )
        setModemIdentifier( provider()->nextModemIdentifier() );

    // If we need to use an IP module, then start up the pppd manager.
    if ( options.useIpModule() ) {
        // Get the pppd manager object for data calls on the internal modem.
        d->pppdManager = provider()->pppdManager();
        connect( d->pppdManager, SIGNAL(dataCallActive()),
                 this, SLOT(pppCallActive()) );
        connect( d->pppdManager, SIGNAL(dataCallInactive()),
                 this, SLOT(pppCallInactive()) );
        connect( d->pppdManager, SIGNAL(dataStateUpdate(QPhoneCall::DataState)),
                 this, SLOT(pppStateUpdate(QPhoneCall::DataState)) );

        // Check that the pppd manager will allow us to do this.
        if ( ! d->pppdManager->dataCallsPossible() ) {
            qLog(Network)
                << "QModemDataCall::dial - data calls are not possible";
            setState( QPhoneCall::OtherFailure );
            return;
        }
        if ( d->pppdManager->isActive() ) {
            qLog(Network)
                << "QModemDataCall::dial - an existing data call is active";
            setState( QPhoneCall::OtherFailure );
            return;
        }

        // We are now in the dialing state.
        setActions( QPhoneCallImpl::None );
        setState( QPhoneCall::Dialing );

        // Start the pppd session.
        if ( ! d->pppdManager->start( options ) ) {
            qLog(Network)
                << "QModemDataCall::dial - could not start pppd";
            setState( QPhoneCall::OtherFailure );
        }
        return;
    }

    // Create the channels for data traffic and setup.
    createChannels();

    if (d->channel && !d->channel->isOpen())
        d->channel->open(QIODevice::ReadWrite);
    if (d->setup && !d->setup->isOpen())
        d->setup->open(QIODevice::ReadWrite);

    // We are now in the dialing state.
    setActions( QPhoneCallImpl::None );
    setState( QPhoneCall::Dialing );

    // Choose between fax and data dialing modes.
    if ( callType() == QLatin1String("Fax") ) {
        d->setup->atchat()->chat
            ( "AT+FCLASS=?", this, SLOT(fclass(bool,QAtResult)) );
    } else if ( options.contextId() != 0 ) {
        // Setting up a GPRS session.
        d->setup->atchat()->chat
            ( "AT+CGDCONT=" + QString::number( options.contextId() ) + ",\"" +
              QAtUtils::quote( options.pdpType() ) + "\",\"" +
              QAtUtils::quote( options.apn() ) + "\"",
              this, SLOT(contextSetupDone(bool,QAtResult)) );
    } else {
        // We need AT+FCLASS=0 to select data, but if the modem
        // does not support fax, this will fail.  That is OK.
        d->setup->atchat()->chat( "AT+FCLASS=0" );
        selectBearer();
    }
}

void QModemDataCall::contextSetupDone( bool ok, const QAtResult& )
{
    if ( !ok ) {
        qLog(Modem) << "Data call not available";
        setState( QPhoneCall::OtherFailure );
        return;
    }

    QStringList setupCmds = provider()->gprsSetupCommands();
    if ( setupCmds.count() > d->contextSetupCounter ) {
        d->contextSetupCounter++;
        d->setup->atchat()->chat( setupCmds[(d->contextSetupCounter)-1], this, SLOT(contextSetupDone(bool,QAtResult)) );
    } else {
        d->contextSetupCounter = 0;
        performDial();
    }
}

void QModemDataCall::fclass( bool ok, const QAtResult& result )
{
    // If the command failed, then fax is probably not supported.
    if ( !ok ) {
        qLog(Modem) << "Fax not supported by modem";
        setState( QPhoneCall::OtherFailure );
        return;
    }

    // Determine the best fax class to use from the AT+FCLASS=? result.
    QAtResultParser parser( result );
    parser.next( "+FCLASS" );
    QString line = parser.line();
    int posn = 0;
    int maxClass = 1;
    int currentClass = 0;
    bool first = true;
    while ( posn < line.length() ) {
        uint ch = line[posn++].unicode();
        if ( ch >= '0' && ch <= '9' ) {
            currentClass = currentClass * 10 + ch - '0';
            first = false;
        } else if ( !first ) {
            if ( currentClass > maxClass )
                maxClass = currentClass;
            currentClass = 0;
            first = true;
        }
    }
    if ( maxClass >= 2 )
        d->faxClass = 2;
    else
        d->faxClass = 1;

    // Set the class and then move on to the bearer.
    d->setup->atchat()->chat( "AT+FCLASS=" + QString::number( d->faxClass ) );
    selectBearer();
}

void QModemDataCall::selectBearer()
{
    if ( d->options.gsmSpeed() != -1 ) {
        // Explicitly-supplied bearer speed value, so send immediately.
        sendBearer();
    } else {
        // Query the available values to select something appropriate.
        d->setup->atchat()->chat
            ( "AT+CBST=?", this, SLOT(bearers(bool,QAtResult)) );
    }
}

void QModemDataCall::sendBearer()
{
    // Compose and send the AT+CBST command to the modem.
    QString bearer = "AT+CBST=";
    bearer += QString::number( d->options.gsmSpeed() );
    bearer += ",";
    bearer += QString::number( (int)d->options.bearer() );
    bearer += ",";
    bearer += QString::number( (int)d->options.transparentMode() );
    d->setup->atchat()->chat( bearer, this, SLOT(bearerDone(bool)) );
}

void QModemDataCall::bearerDone( bool ok )
{
    if ( !ok ) {
        // The AT+CBST command failed.  It is possible that the
        // command is not supported at all.  Issue AT+CBST? to
        // test to see if the command is supported.  If not,
        // continue on to the dial.  If it is, then we have tried
        // to select a non-existent bearer and the dial should stop.
        d->setup->atchat()->chat( "AT+CBST?", this, SLOT(testBearer(bool)) );
    } else {
        performDial();
    }
}

void QModemDataCall::testBearer( bool ok )
{
    if ( ok ) {
        // The AT+CBST command works, so the bearer set attempt was a failure.
        // That means that the dial must be aborted.  We report it as
        // "OtherFailure" so the higher layers can tell the difference
        // between parameter problems and failure to connect.
        setState( QPhoneCall::OtherFailure );
    } else {
        // The AT+CBST command is not supported, so move on to the dial.
        performDial();
    }
}

void QModemDataCall::bearers( bool ok, const QAtResult& result )
{
    // If the AT+CBST=? command is not supported, then skip to the dial step
    // if we are making a data call, or abort if we are making a video call.
    if ( !ok ) {
        if ( callType() == QLatin1String("Video") )
            setState( QPhoneCall::OtherFailure );
        else
            performDial();
        return;
    }

    // Determine if we are creating a video or data call.  Video calls
    // need to use different bearer values for the various speeds.
    bool isVideo = ( callType() == QLatin1String("Video") );

    // Convert the baud rate into a set of candidate gsm speed values.
    // These values come from 3GPP TS 27.007.
    QList<int> speeds;
    switch ( d->options.speed() ) {
        case 300:
        {
            speeds += 1;
            speeds += 65;
        }
        break;

        case 1200:
        {
            speeds += 2;
            speeds += 34;
            speeds += 66;
        }
        break;

        case 2400:
        {
            speeds += 4;
            speeds += 5;
            speeds += 36;
            speeds += 68;
        }
        break;

        case 4800:
        {
            speeds += 6;
            speeds += 38;
            speeds += 70;
        }
        break;

        case 9600:
        {
            speeds += 7;
            speeds += 12;
            speeds += 39;
            speeds += 71;
        }
        break;

        case 14400:
        {
            speeds += 14;
            speeds += 43;
            speeds += 75;
        }
        break;

        case 19200:
        {
            speeds += 15;
            speeds += 47;
            speeds += 79;
        }
        break;

        case 28800:
        {
            if ( isVideo ) {
                speeds += 130;
            } else {
                speeds += 16;
                speeds += 48;
                speeds += 80;
            }
        }
        break;

        case 32000:
        {
            if ( isVideo )
                speeds += 131;
            else
                speeds += 120;
        }
        break;

        case 33600:
        {
            if ( isVideo )
                speeds += 132;
            else
                speeds += 17;
        }
        break;

        case 38400:
        {
            speeds += 49;
            speeds += 81;
        }
        break;

        case 48000:
        {
            speeds += 50;
            speeds += 82;
        }
        break;

        case 56000:
        {
            if ( isVideo ) {
                speeds += 133;
            } else {
                speeds += 51;
                speeds += 83;
                speeds += 115;
            }
        }
        break;

        case 64000:
        {
            if ( isVideo ) {
                speeds += 134;
            } else {
                speeds += 84;
                speeds += 116;
                speeds += 121;
            }
        }
        break;

        default:
        {
            if ( isVideo ) {
                // Search for the best video transfer rate.
                speeds += 134;
                speeds += 133;
                speeds += 132;
                speeds += 131;
                speeds += 130;
            } else {
                // Use auto-bauding if we don't know the data speed.
                speeds += 0;
            }
        }
        break;
    }

    // Get the list of modem-supported speeds.
    QList<int> modemSpeeds;
    QAtResultParser parser( result );
    parser.next( "+CBST:" );
    QString line = parser.line();
    int posn = 0;
    int value = 0;
    int range = -1;
    bool first = true;
    while ( posn < line.length() ) {
        uint ch = line[posn++].unicode();
        if ( ch >= '0' && ch <= '9' ) {
            value = value * 10 + ch - '0';
            first = false;
        } else if ( ch == ')' ) {
            break;
        } else if ( ch == '-' ) {
            range = value;
            value = 0;
            first = true;
        } else if ( !first ) {
            if ( range != -1 ) {
                while ( range <= value ) {
                    modemSpeeds += range;
                    ++range;
                }
            } else {
                modemSpeeds += value;
            }
            value = 0;
            range = -1;
            first = true;
        }
    }
    if ( !first ) {
        if ( range != -1 ) {
            while ( range <= value ) {
                modemSpeeds += range;
                ++range;
            }
        } else {
            modemSpeeds += value;
        }
    }

    // Find the first desired speed that is actually supported.
    int gsmSpeed = -1;
    foreach ( int temp, speeds ) {
        if ( modemSpeeds.contains( temp ) ) {
            gsmSpeed = temp;
            break;
        }
    }
    if ( gsmSpeed == -1 ) {
        qLog(Modem) << "Could not find a suitable bearer for the data call";
        setState( QPhoneCall::OtherFailure );
        return;
    }

    // Set the explicit speed and send the AT+CBST command.
    d->options.setGsmSpeed( gsmSpeed );
    sendBearer();
}

void QModemDataCall::performDial()
{
    // Compose the dial command and send it.
    d->setup->atchat()->chat
        ( "ATD" + number(), this, SLOT(dialRequestDone(bool)) );
}

/*!
    \reimp
*/
void QModemDataCall::hangup( QPhoneCall::Scope /*scope*/ )
{
    // If this is an IP module based call, then stop the pppd data session.
    if ( d->pppdManager ) {
        if ( state() == QPhoneCall::Dialing ||
             state() == QPhoneCall::Alerting ||
             state() == QPhoneCall::Connected ) {
            d->pppdManager->stop();
        }
        return;
    }

    if ( !d->setup ) {
        // We are probably hanging up an incoming call, so we don't
        // have channel information yet.  Assume the primary channel.
        d->setup = provider()->multiplexer()->channel( "primary" );
    }

    // Stop "carrierChanged" from detecting our local hangup as a remote one.
    d->hangingUp = true;

    if ( state() == QPhoneCall::Dialing ||
         state() == QPhoneCall::Alerting ) {

        // Abort an outstanding ATD request if we are dialing.
        d->setup->atchat()->abortDial();

        // Make sure we follow it with "ATH", because some modems
        // don't pay any attention to the abort request.  If there are
        // other calls in the system, use AT+CHLD to select the specific
        // call to avoid hanging up everything else as well.
        if ( provider()->calls().size() > 1 ) {
            d->setup->atchat()->chat( "AT+CHLD=1" + QString::number( modemIdentifier() ) );
        } else {
            d->setup->atchat()->chat( "ATH" );
        }

        // Set the hangup state.
        setState( QPhoneCall::HangupLocal );

    } else if ( state() == QPhoneCall::Connected ) {

        // Stop data delivery and turn atchat back on for the data channel.
        destroyServer();

        // We are already connected.  If the data and setup channels
        // are the same, then lower DTR, and wait before sending ATH.
        if ( d->channel == d->setup ) {
            qLog(Modem) << "Hanging up data call by lowering DTR";
            d->channel->setDtr( false );
            QTimer::singleShot( 200, this, SLOT(dtrLowered()) );
        } else {
            // Send ATH on the setup channel to hang up the data call.
            // If there are other calls in the system, use AT+CHLD to
            // select the specific call to avoid hanging up everything
            // else as well.
            if ( provider()->calls().size() > 1 ) {
                d->setup->atchat()->chat( "AT+CHLD=1" + QString::number( modemIdentifier() ) );
            } else {
                d->setup->atchat()->chat( "ATH" );
            }
            setState( QPhoneCall::HangupLocal );
        }

    } else if ( state() == QPhoneCall::Incoming ) {

        // Reject an incoming call and set the busy state for the caller.
        d->setup->atchat()->chat( "AT+CHLD=0" );
        setState( QPhoneCall::HangupLocal );

    }
}

/*!
    \reimp
*/
void QModemDataCall::accept()
{
    if ( state() != QPhoneCall::Incoming || d->channel || d->pppdManager )
        return;

    // Create the channels for data traffic and setup.
    createChannels();

    // Issue "ATA" to accept the incoming call.
    d->setup->atchat()->chat( "ATA", this, SLOT(acceptDone(bool)) );
}

/*!
    \reimp
*/
void QModemDataCall::hold()
{
    // Hold is not supported for data calls.
    emit requestFailed( QPhoneCall::HoldFailed );
}

/*!
    \reimp
*/
void QModemDataCall::activate( QPhoneCall::Scope /*scope*/ )
{
    // Because hold is not supported, activate() makes no sense either.
}

/*!
    \reimp
*/
void QModemDataCall::join( bool /*detachSubscriber*/ )
{
    // We don't support joins for data calls.
    emit requestFailed( QPhoneCall::JoinFailed );
}

/*!
    \reimp
*/
void QModemDataCall::tone( const QString& /*tones*/ )
{
    // We don't support DTMF tones for data calls.
}

/*!
    \reimp
*/
void QModemDataCall::transfer( const QString& /*number*/ )
{
    // We don't support transfers for data calls.
}

void QModemDataCall::dialRequestDone( bool ok )
{
    if ( ok ) {
        // Set up data delivery to the client app and switch to connected.
        createServer();
        setActions( QPhoneCallImpl::None ); // Holding is not possible.
        setState( QPhoneCall::Connected );
    } else {
        // The dial request failed, so shift to the hangup state.
        setState( QPhoneCall::HangupLocal );
    }
}

void QModemDataCall::acceptDone( bool ok )
{
    if ( ok ) {
        // Set up data delivery to the client app and switch to connected.
        createServer();
        setActions( QPhoneCallImpl::None ); // Holding is not possible.
        setState( QPhoneCall::Connected );
    } else {
        // The accept request failed - the other side probably hung up.
        setState( QPhoneCall::HangupRemote );
    }
}

void QModemDataCall::dtrLowered()
{
    // If the carrier line is still set, then lowering DTR
    // didn't do anything.  Try sending "+++" and "ATH" instead.
    // If it did work, then the call is ended.
    if ( !d->channel )
        return;
    if ( d->channel->carrier() ) {
        d->channel->setDtr( true );
        QTimer::singleShot( 1000, this, SLOT(sendSwitchBack()) );
    } else {
        d->channel->setDtr( true );
        setState( QPhoneCall::HangupLocal );
    }
}

void QModemDataCall::sendSwitchBack()
{
    discardInput();
    qLog(Modem) << "Sending +++ to modem";
    if ( d->channel )
        d->channel->write( "+++", 3 );
    QTimer::singleShot( 1000, this, SLOT(sendHangup()) );
}

void QModemDataCall::sendHangup()
{
    discardInput();
    qLog(Modem) << "Sending ATH to hangup data call";
    if ( d->channel )
        d->channel->write( "ATH\r", 4 );
    QTimer::singleShot( 100, this, SLOT(hangupDone()) );
}

void QModemDataCall::hangupDone()
{
    discardInput();
    setState( QPhoneCall::HangupLocal );
}

void QModemDataCall::remoteHangupDone()
{
    discardInput();
    setState( QPhoneCall::HangupRemote );
}

void QModemDataCall::carrierChanged( bool value )
{
    // If we were connected and the carrier just dropped, then
    // the remote side has hung up the call.  We wait a bit before
    // acknowledging it, to clear out "NO CARRIER" and similar
    // random data that usually happens at hangup time.
    if ( state() == QPhoneCall::Connected && !value && !d->hangingUp ) {
        QTimer::singleShot( 200, this, SLOT(remoteHangupDone()) );
    }
}

// Discard all input to get rid of bogus characters while
// sending +++ and ATH to the modem.
void QModemDataCall::discardInput()
{
    if ( d->channel ) {
        char buf[64];
        while ( d->channel->read( buf, sizeof(buf) ) > 0 )
            ;   // Nothing to do here.
    }
}

/*!
    \reimp
*/
void QModemDataCall::setState( QPhoneCall::State value )
{
    if ( value >= QPhoneCall::HangupLocal ) {
        // The call has ended, so make sure the data channel is closed.
        destroyServer();
        d->channel = 0;
        d->setup = 0;
    }
    QModemCall::setState( value );
}

#define DATACALL_BUFSIZ     1024

void QModemDataCall::createServer()
{
    // Bail out if we have already done this.
    if ( d->server )
        return;

    // Create a buffer for data traffic.
    if ( !d->buf )
        d->buf = new char [DATACALL_BUFSIZ];

    // Create the server listener.
    d->server = new QSerialSocketServer( 0, true, this );
    connect( d->server, SIGNAL(incoming(QSerialSocket*)),
             this, SLOT(incoming(QSerialSocket*)) );
    setDataPort( d->server->port() );

    // Turn off atchat on the channel so it doesn't eat the incoming data.
    if ( d->channel == d->setup )
        d->channel->atchat()->suspend();
}

void QModemDataCall::destroyServer()
{
    if ( d->server )
        delete d->server;
    if ( d->socket ) {
        //we have to disconnect first or we'd call destroyServer again
        disconnect( d->socket, SIGNAL(closed()), this, SLOT(socketClosed()) );
        delete d->socket;
        disconnect( d->channel, SIGNAL(readyRead()),
                    this, SLOT(channelReadyRead()) );
        disconnect( d->channel, SIGNAL(carrierChanged(bool)),
                    this, SLOT(carrierChanged(bool)) );
    }
    if ( d->channel ) {
        d->channel->close();
        if ( d->channel == d->setup )
            d->channel->atchat()->resume();
    }
    d->server = 0;
    d->socket = 0;
}

void QModemDataCall::incoming( QSerialSocket *sock )
{
    if ( d->socket ) {
        // If we already have a socket, then close the second one.
        // Only one application can be processing the data.
        delete sock;
        return;
    }
    d->socket = sock;
    connect( d->channel, SIGNAL(readyRead()), this, SLOT(channelReadyRead()) );
    connect( d->channel, SIGNAL(carrierChanged(bool)),
             this, SLOT(carrierChanged(bool)) );
    connect( d->socket, SIGNAL(readyRead()), this, SLOT(socketReadyRead()) );
    connect( d->socket, SIGNAL(closed()), this, SLOT(socketClosed()) );

    if ( !d->channel->isOpen() )
        d->channel->open( QIODevice::ReadWrite );
    d->channel->setDtr( true );
    // If there is data ready on the channel, write it to the socket now,
    // because we may have already missed the last readyRead() signal.
    channelReadyRead();
}

void QModemDataCall::channelReadyRead()
{
    qint64 len = 0;
    while ( ( len = d->channel->read( d->buf, DATACALL_BUFSIZ ) ) > 0 ) {
        d->socket->write( d->buf, len );
    }
}

void QModemDataCall::socketReadyRead()
{
    qint64 len = 0;
    while ( ( len = d->socket->read( d->buf, DATACALL_BUFSIZ ) ) > 0 ) {
        d->channel->write( d->buf, len );
    }
}

void QModemDataCall::socketClosed()
{
    //destroyServer();

    d->channel->setDtr( false );
    hangup( QPhoneCall::CallOnly );
}

void QModemDataCall::pppCallActive()
{
    qLog(Network) << "QModemDataCall::pppCallActive";
    setActions( QPhoneCallImpl::None ); // Holding is not possible.
    if ( d->options.ipDemandDialing() && !modemIdentifier() ) {
        setModemIdentifier( provider()->nextModemIdentifier() );
    }
    setState( QPhoneCall::Connected );
}

void QModemDataCall::pppCallInactive()
{
    qLog(Network) << "QModemDataCall::pppCallInactive";
    if ( d->options.ipDemandDialing() ) {
        provider()->releaseModemIdentifier( modemIdentifier() );
        setModemIdentifier(0);
        setState( QPhoneCall::Dialing );
    } else {
        provider()->multiplexer()->channel( "data" )->close();
        emit notification( QPhoneCall::DataStateUpdate, QString::number( (int)QPhoneCall::PPPdStopped ) );
        setState( QPhoneCall::HangupLocal );
    }
}

void QModemDataCall::pppStateUpdate( QPhoneCall::DataState value )
{
    if ( value & ( QPhoneCall::PPPdStopped | QPhoneCall::PPPdFailed ) ) {
        // The pppd process has shut down.  If the command channel
        // is not valid, but the data channel is, then we need to
        // drop DTR, wait, raise DTR, and close data.  This is for
        // the QNullSerialIODeviceMultiplexer class, which switches
        // off the command channel while data traffic is in progress.
        // Once data traffic ends, we need to switch back.
        QSerialIODevice *command = provider()->multiplexer()->channel( "primary" );
        QSerialIODevice *data = provider()->multiplexer()->channel( "data" );
        if ( command && data && !command->isValid() && data->isOpen() ) {
            data->setDtr( false );
            QTimer::singleShot( 250, this, SLOT(raiseDtr()) );
        } else if ( data && data->isOpen() ) {
            //we always have to do this when in demand dialing and
            //when we start and stop pppd very fast it never actually calls the disconnect script
            //therefore pppCallInactive is not called and the data channel not close what
            //prevents subsequent connections
            //In general closing the channel and hanging up should always be safe as we only come through
            //this code when pppd died recently.
            provider()->multiplexer()->channel( "data" )->close();
            setState( QPhoneCall::HangupLocal );
        }
    }
    emit notification( QPhoneCall::DataStateUpdate,
                       QString::number( (int)value ) );
}

void QModemDataCall::raiseDtr()
{
    // Raise the DTR signal and close the data channel, which will revert
    // control back to the command channel with the null multiplexer.
    QSerialIODevice *data = provider()->multiplexer()->channel( "data" );
    data->setDtr( true );
    data->close();
}
