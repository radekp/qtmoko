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

#include <qmodemcallprovider.h>
#include <qmodemcall.h>
#include <qmodemservice.h>
#include <qmodemllindicators.h>
#include <qatutils.h>
#include <qatresult.h>
#include <qatresultparser.h>
#include "qmodempppdmanager_p.h"
#include <qtopialog.h>
#include <qtimer.h>

/*!
    \class QModemCallProvider
    \inpublicgroup QtCellModule

    \brief The QModemCallProvider class implements a mechanism for AT-based phone call providers to hook into the telephony system.
    \ingroup telephony::modem

    This class provides a number of methods, such as dialVoiceCommand(),
    releaseCallCommand(), putOnHoldCommand(), etc, that can be used to
    customize the AT commands that are used by QModemCall for specific
    operations.  Modem vendor plug-ins override these methods in
    their own phone call provider.

    Client applications should use QPhoneCall and QPhoneCallManager to make
    and receive phone calls.  The QModemCallProvider class is intended for the
    phone server.

    QModemCall instances are created by the QModemCallProvider::create()
    function.  If a modem vendor plug-in needs to change some of the
    functionality in this class, they should do the following:

    \list
        \o Inherit from QModemCall and override the functionality that is different.
        \o Inherit from QModemCallProvider and override QModemCallProvider::create()
           to instantiate the new class that inherits from QModemCall.
    \endlist

    See the documentation for QModemCall for more information.

    \sa QModemCall, QModemDataCall, QPhoneCallProvider, QPhoneCallImpl
*/

class QModemCallProviderPrivate
{
public:
    QModemCallProviderPrivate()
	: useDetectTimer(true)
	, useMisssedTimer(true)
    {
        incomingModemIdentifier = 0;
        ringBlock = false;
        usedIds = 0;
        vtsType = 0;
        pppdManager = 0;
    }

    QModemService *service;
    QString incomingNumber;
    QString incomingCallType;
    uint incomingModemIdentifier;
    QTimer *detectTimer;
    QTimer *missedTimer;
    QTimer *hangupTimer;
    bool ringBlock;
    uint usedIds;
    int vtsType;
    QModemPPPdManager *pppdManager;
    bool useDetectTimer;
    bool useMisssedTimer;
};

// Time to wait between first seeing an incoming call and actually
// reporting it to the higher layers.  This timeout is needed to allow
// all information from RING, +CRING, +CLIP, +CCWA, and similar commands
// to be collected up.
#define INCOMING_DETECT_TIMEOUT     800

// Time to wait after seeing the last ring indication before assuming
// that the call has gone away and was missed.
#define INCOMING_MISSED_TIMEOUT     5500

// Time to wait just after a call to QModemCallProvider::hangup()
// before allowing notifications like "NO CARRIER" to work again.
#define HANGUP_TIMEOUT              200

/*!
    Constructs a new AT-based phone call provider for \a service.
*/
QModemCallProvider::QModemCallProvider( QModemService *service )
    : QPhoneCallProvider( service->service(), service )
{
    d = new QModemCallProviderPrivate();
    d->service = service;
    d->detectTimer = new QTimer( this );
    d->detectTimer->setSingleShot( true );
    connect( d->detectTimer, SIGNAL(timeout()), this, SLOT(detectTimeout()) );
    d->missedTimer = new QTimer( this );
    d->missedTimer->setSingleShot( true );
    connect( d->missedTimer, SIGNAL(timeout()), this, SLOT(missedTimeout()) );
    d->hangupTimer = new QTimer( this );
    d->hangupTimer->setSingleShot( true );

    connect( service, SIGNAL(resetModem()), this, SLOT(resetModem()) );

    // Create the pppd manager object for data calls on the internal modem.
    d->pppdManager = new QModemPPPdManager( service->multiplexer(), this );

    // Advertise the call types that we understand.
    QStringList types;
    types += "Voice";
    types += "Data";
    types += "Fax";
    types += "Video";
    types += "IP";
    setCallTypes( types );
}

/*!
    Destroys this AT-based phone call provider and all QPhoneCallImpl instances
    associated with it.
*/
QModemCallProvider::~QModemCallProvider()
{
    delete d;
}

/*!
    Sets the ringing state in the phone library, indicating a call
    from \a number of the type \a callType.  Modem vendor plug-ins
    call this if the modem reports incoming calls with something other
    than \c RING or \c +CRING.

    Either \a number or \a callType may be empty indicating that the
    information is not available yet.  The information may become
    available shortly on a different unsolicited result code
    (e.g. \c +CLIP or \c +CCWA).

    If \a modemIdentifier is not zero, it indicates the modem identifier
    for the call.  If \a modemIdentifier is zero, the next available
    modem identifier is used.

    \sa QModemCall::callType(), QModemCall::number(), QModemCall::modemIdentifier()
    \sa hasRepeatingRings()
*/
void QModemCallProvider::ringing
    ( const QString& number, const QString& callType, uint modemIdentifier )
{
    llIndicatorsRinging();
    
    // If we already announced an incoming call, then reset the
    // missed call timer but otherwise ignore this.  This prevents
    // us from creating a new incoming call for every new RING.
    if ( incomingCall() ) {
        if ( hasRepeatingRings() && useMissedTimer() )
            d->missedTimer->start( INCOMING_MISSED_TIMEOUT );
        return;
    }

    // Record the new information that we got.
    if ( !number.isEmpty() )
        d->incomingNumber = number;
    if ( !callType.isEmpty() )
        d->incomingCallType = callType;
    if ( modemIdentifier ) {
        releaseModemIdentifier( d->incomingModemIdentifier );
        useModemIdentifier( modemIdentifier );
        d->incomingModemIdentifier = modemIdentifier;
    } else if ( !d->incomingModemIdentifier ) {
        d->incomingModemIdentifier = nextModemIdentifier();
    }

    // Start the detection timer if it isn't already running.
    if ( !d->detectTimer->isActive() && useDetectTimer() )
        d->detectTimer->start( INCOMING_DETECT_TIMEOUT );

    // Reset the missed call timer if necessary.
    if ( hasRepeatingRings() && useMissedTimer() )
        d->missedTimer->start( INCOMING_MISSED_TIMEOUT );
}

/*!
    Called by the modem vendor plug-in to indicate that it in determined that
    \a call was hung up remotely.  This is usually called in response to a
    proprietary unsolicited result code.

    If \a call is null, then the modem vendor plug-in has determined
    that the active call has hung up but it was unable to be any
    more precise than that.

    \sa QModemCall::hangup()
*/
void QModemCallProvider::hangupRemote( QModemCall *call )
{
    beginStateTransaction();
    if ( call ) {

        // A specific call has hung up.
        //bool wasActive = ( call->state() == QPhoneCall::Connected );
        call->setState( QPhoneCall::HangupRemote );

    } else {

        // The active call has hung up.
        changeGroup( QPhoneCall::Connected, QPhoneCall::HangupRemote,
                     QPhoneCallImpl::None );
    }
    endStateTransaction();

    // If "NO CARRIER", "NO DIALTONE", etc occurs in the next small
    // amount of time, then assume that it is a duplicate of this
    // hangup request.  This is needed to work around modems that
    // send an unsolicited response followed by "NO CARRIER", even
    // though the unsolicited response is more than adequate on its own.
    d->hangupTimer->start( HANGUP_TIMEOUT );
}

/*!
    \reimp
*/
QPhoneCallImpl *QModemCallProvider::create
        ( const QString& identifier, const QString& callType )
{
    if ( callType == "Voice" )      // No tr
        return new QModemCall( this, identifier, callType );
    else
        return new QModemDataCall( this, identifier, callType );
}

/*!
    Resolves a call type on a "+CRING" notification into the particular
    type of call that it represents.  The \a type is guaranteed to
    be in lower case on entry to this function.

    \sa resolveCallMode()
*/
QString QModemCallProvider::resolveRingType( const QString& type ) const
{
    if ( type == "voice" || type.startsWith( "voice," ) )    // No tr
        return "Voice";         // No tr
    else if ( type == "fax" || type.startsWith( "fax," ) )   // No tr
        return "Fax";           // No tr
    else if ( type.contains( "sync" ) )                      // No tr
        return "Data";          // No tr

    // Don't know what this is, so let ringing() guess at it later.
    return QString();
}

/*!
    Resolves a call \a mode as reported by the \c{AT+CLCC} command.

    \sa resolveRingType()
*/
QString QModemCallProvider::resolveCallMode( int mode ) const
{
    if ( mode == 0 || mode == 3 || mode == 4 || mode == 5 ) {
        return "Voice";         // No tr
    } else if ( mode == 1 || mode == 6 || mode == 7 ) {
        return "Data";          // No tr
    } else if ( mode == 2 || mode == 8 ) {
        return "Fax";           // No tr
    } else {
        // Don't know what this is.  Assume voice.
        return "Voice";         // No tr
    }
}

/*!
    Returns true if the modem automatically re-sends \c RING every few
    seconds while a call is incoming, and stops sending \c RING if
    the caller hangs up.  Returns false if the modem does not resend
    \c RING every few seconds and instead uses some other mechanism
    to notify Qt Extended that a remote hangup has occurred.  The default
    return value is true.

    \sa ringing()
*/
bool QModemCallProvider::hasRepeatingRings() const
{
    return true;
}

/*!
    \enum QModemCallProvider::AtdBehavior
    \brief This enum defines the behavior of the \c ATD modem command when dialing voice calls.

    \value AtdOkIsConnect When \c ATD reports \c OK, the call has connected.
    \value AtdOkIsDialing When \c ATD reports \c OK, the call is dialing, but not yet connected.  The \c AT+CLCC command must be used to determine when it transitions from dialing to connected.
    \value AtdOkIsDialingWithStatus When \c ATD reports \c OK, the call is dialing, but not yet connected.  The modem vendor plug-in will call setState() on the call when it transitions to connected.
    \value AtdUnknown When \c ATD reports \c OK, it is unknown whether the call is dialing or connected and \c AT+CLCC must be used to determine the status.  This is the default.
*/

/*!
    Returns the behavior of the \c ATD modem command when dialing
    voice calls.

    The defined behavior in 3GPP TS 27.007 is for the \c ATD command
    to immediately return to command mode when it has a trailing
    semi-colon (i.e. \c{ATDnumber;}), even if the call has not
    yet connected.  Not all modems support this correctly.

    This function can be used to alter how the system uses the \c ATD
    command when dialing voice calls to accommodate non-standard modems.

    \c AtdOkIsConnect indicates that \c ATD blocks until the call
    has connected before reporting \c OK.  \c AtdOkIsDialing and
    \c AtdOkIsDialingWithStatus indicates that the modem obeys
    3GPP TS 27.007 and returns immediately to command mode.

    In the case of \c AtdOkIsDialing, the \c AT+CLCC command is polled
    to determine when the call transitions from dialing to connected.
    The modem vendor plug-in can avoid polling if it has call status
    tracking.  In that case it should return \c AtdOkIsDialingWithStatus
    from this function and call setState() once the transition occurs.

    \c AtdUnknown indicates that it is not known which of these modes is
    supported by the modem.  A separate \c AT+CLCC command is used to
    determine what state the call is actually in once \c ATD reports \c OK.

    The default implementation returns AtdUnknown.

    \sa QModemCallProvider::AtdBehavior
*/
QModemCallProvider::AtdBehavior QModemCallProvider::atdBehavior() const
{
    return AtdUnknown;
}

/*!
    Aborts an \c ATD dial command for the call \a modemIdentifier.
    The \a scope parameter is passed from QModemCall::hangup().

    The default implementation calls atchat()->abortDial(),
    followed by either \c{AT+CHLD=1} or \c{AT+CHLD=1n} depending
    upon the value of \a scope.

    \sa QAtChat::abortDial(), QModemCall::hangup()
*/
void QModemCallProvider::abortDial
        ( uint modemIdentifier, QPhoneCall::Scope scope )
{
    atchat()->abortDial();
    if ( scope == QPhoneCall::CallOnly ) {
        atchat()->chat( "AT+CHLD=1" + QString::number( modemIdentifier ) );
    } else {
        atchat()->chat( "AT+CHLD=1" );
    }
}

/*!
    Returns true if a particular kind of call is part of the normal hold group.
    On some systems, video calls are separate from the call grouping for
    voice and fax calls.  Returns true only for \c Voice by default.
    The \a type parameter indicates the type of call (\c Voice, \c Video,
    \c Fax, etc).
*/
bool QModemCallProvider::partOfHoldGroup( const QString& type ) const
{
    return ( type == "Voice" ); // No tr
}

/*!
    Returns the phone call object associated with the current incoming call.
    Returns null if there is no incoming call.

    \sa dialingCall()
*/
QModemCall *QModemCallProvider::incomingCall() const
{
    QList<QPhoneCallImpl *> list = calls();
    QList<QPhoneCallImpl *>::ConstIterator it;
    for ( it = list.begin(); it != list.end(); ++it ) {
        if ( (*it)->state() == QPhoneCall::Incoming )
            return (QModemCall *)*it;
    }
    return 0;
}

/*!
    Returns the phone call object associated with the current dialing
    or alerting call.  Returns null if there is no such call.

    \sa incomingCall()
*/
QModemCall *QModemCallProvider::dialingCall() const
{
    QList<QPhoneCallImpl *> list = calls();
    QList<QPhoneCallImpl *>::ConstIterator it;
    for ( it = list.begin(); it != list.end(); ++it ) {
        if ( (*it)->state() == QPhoneCall::Dialing ||
             (*it)->state() == QPhoneCall::Alerting )
            return (QModemCall *)*it;
    }
    return 0;
}

/*!
    Returns the AT command to use to dial the supplementary service
    specified by \a options.  The default implementation returns
    \c{ATDnumber}.

    \sa dialVoiceCommand()
*/
QString QModemCallProvider::dialServiceCommand
            ( const QDialOptions& options ) const
{
    return "ATD" + options.number();
}

/*!
    Returns the AT command to use to dial the voice call specified
    by \a options.  The default implementation returns
    \c{ATDnumber[i][g];} where the \c i flag indicates the caller ID
    disposition and the \c g flag indicates the closed user group
    disposition.

    \sa QDialOptions::number(), QDialOptions::callerId()
    \sa QDialOptions::closedUserGroup()

    \sa dialServiceCommand()
*/
QString QModemCallProvider::dialVoiceCommand
            ( const QDialOptions& options ) const
{
    QString suffix;
    if ( options.callerId() == QDialOptions::SendCallerId )
        suffix += "i";
    else if ( options.callerId() == QDialOptions::SuppressCallerId )
        suffix += "I";
    if ( options.closedUserGroup() )
        suffix += "g";
    return "ATD" + options.number() + suffix + ";";
}

/*!
    Returns the AT command to use to release the call \a modemIdentifier.
    The default implementation returns \c{AT+CHLD=1modemIdentifier}.

    \sa releaseActiveCallsCommand(), releaseHeldCallsCommand()
*/
QString QModemCallProvider::releaseCallCommand( uint modemIdentifier ) const
{
    return "AT+CHLD=1" + QString::number( modemIdentifier );
}

/*!
    Returns the AT command to use to release all active calls.
    The default implementation returns \c{AT+CHLD=1}.

    \sa releaseCallCommand(), releaseHeldCallsCommand()
*/
QString QModemCallProvider::releaseActiveCallsCommand() const
{
    return "AT+CHLD=1";
}

/*!
    Returns the AT command to use to release all held calls.
    The default implementation returns \c{AT+CHLD=0}.

    \sa releaseCallCommand(), releaseActiveCallsCommand()
*/
QString QModemCallProvider::releaseHeldCallsCommand() const
{
    return "AT+CHLD=0";
}

/*!
    Returns the AT command to use to put the currently active calls on hold.
    The default implementation returns \c{AT+CHLD=2}.

    \sa activateCallCommand(), activateHeldCallsCommand(), QModemCall::hold()
*/
QString QModemCallProvider::putOnHoldCommand() const
{
    return "AT+CHLD=2";
}

/*!
    Returns the AT command to use to reject the incoming call and set
    the busy state for the caller.  The default implementation
    returns \c{AT+CHLD=0}.

    \sa acceptCallCommand(), QModemCall::hangup()
*/
QString QModemCallProvider::setBusyCommand() const
{
    return "AT+CHLD=0";
}

/*!
    Returns the AT command to use to accept an incoming call.
    If \a otherActiveCalls is true, then there are other active
    calls within the system.  The default implementation
    returns \c{AT+CHLD=2} if \a otherActiveCalls is true, or
    \c{ATA} otherwise.

    \sa setBusyCommand(), QModemCall::accept()
*/
QString QModemCallProvider::acceptCallCommand( bool otherActiveCalls ) const
{
    if ( otherActiveCalls )
        return "AT+CHLD=2";
    else
        return "ATA";
}

/*!
    Returns the AT command to use to activate the call \a modemIdentifier.
    If \a otherActiveCalls is true, then there are other active
    calls within this system.  The default implementation returns
    \c{AT+CHLD=2modemIdentifier} if \a otherActiveCalls is true,
    or \c{AT+CHLD=2} if the call being activated is the only one
    in the system.

    \sa activateHeldCallsCommand(), putOnHoldCommand(), QModemCall::activate()
*/
QString QModemCallProvider::activateCallCommand
            ( uint modemIdentifier, bool otherActiveCalls ) const
{
    if ( otherActiveCalls )
        return "AT+CHLD=2" + QString::number( modemIdentifier );
    else
        return "AT+CHLD=2";
}

/*!
    Returns the AT command to use to place the currently active calls on
    hold and activate the held calls.  The default implementation
    returns \c{AT+CHLD=2}.

    \sa activateCallCommand(), putOnHoldCommand(), QModemCall::activate()
*/
QString QModemCallProvider::activateHeldCallsCommand() const
{
    return "AT+CHLD=2";
}

/*!
    Returns the AT command to use to join the active and held calls
    into a single multi-party conversation.  If \a detachSubscriber
    is true, then detach the local party from the conversation after
    joining the calls.  Returns \c{AT+CHLD=4} if \a detachSubscriber
    is true, or \c{AT+CHLD=3} otherwise.

    \sa QModemCall::join()
*/
QString QModemCallProvider::joinCallsCommand( bool detachSubscriber ) const
{
    if ( detachSubscriber )
        return "AT+CHLD=4";
    else
        return "AT+CHLD=3";
}

/*!
    Returns the AT command to use to deflect the incoming call to \a number.
    The default implementation returns \c{AT+CTFR=number}.

    \sa QModemCall::transfer()
*/
QString QModemCallProvider::deflectCallCommand( const QString& number ) const
{
    return "AT+CTFR=" + number;
}

/*!
    Returns the phone call associated with modem identifier \a id.
    Returns null if there is no such call.
*/
QModemCall *QModemCallProvider::callForIdentifier( uint id ) const
{
    QList<QPhoneCallImpl *> list = calls();
    QList<QPhoneCallImpl *>::ConstIterator it;
    for ( it = list.begin(); it != list.end(); ++it ) {
        if ( ((QModemCall *)(*it))->modemIdentifier() == id )
            return (QModemCall *)*it;
    }
    return 0;
}

/*!
  Returns the AT commands to use to set up a GPRS session. The returned list should not contain
  \c{AT+CGDCONT} and \c{ATD*99***1#} as these commands are automatically inserted.
  The default Implementation returns \c{AT+CGATT=1}.
*/
QStringList QModemCallProvider::gprsSetupCommands() const
{
    return QStringList() << "AT+CGATT=1";
}

/*!
    Returns the modem service that this call provider is associated with.
*/
QModemService *QModemCallProvider::service() const
{
    return d->service;
}

/*!
    Returns the AT chat handler for this modem call provider.
*/
QAtChat *QModemCallProvider::atchat() const
{
    return d->service->primaryAtChat();
}

/*!
    Returns the serial multiplexer for this modem call provider.
*/
QSerialIODeviceMultiplexer *QModemCallProvider::multiplexer() const
{
    return d->service->multiplexer();
}

/*!
    Called when the modem resets after PIN entry, to initialize
    the call provider.
*/
void QModemCallProvider::resetModem()
{
    // Register for standard notifications that indicate incoming calls.
    atchat()->registerNotificationType( "RING", this, SLOT(ring()) );
    atchat()->registerNotificationType( "+CRING:", this, SLOT(cring(QString)) );
    atchat()->registerNotificationType( "+CLIP:", this, SLOT(clip(QString)), true );
    atchat()->registerNotificationType( "+COLP:", this, SLOT(colp(QString)), true );
    atchat()->registerNotificationType( "+CCWA: \"", this, SLOT(ccwa(QString)) );
    connect( atchat(), SIGNAL(callNotification(QString)),
             this, SLOT(callNotification(QString)) );

    // Turn on +CRING notifications because they give us more info than RING.
    atchat()->chat( "AT+CRC=1" );

    // Turn on caller-ID presentation for incoming calls.
    d->service->retryChat( "AT+CLIP=1" );

    // Turn on COLP presentation for outgoing calls.
    d->service->retryChat( "AT+COLP=1" );

    // Turn on unsolicited notifications for incoming calls.
    d->service->retryChat( "AT+CCWA=1" );
}

void QModemCallProvider::ring()
{
    ringing( QString(), QString() );
}

void QModemCallProvider::cring( const QString& msg )
{
    // Notification of an incoming call, with an explicit type code.
    QString type = msg.mid( 7 ).toLower().trimmed();
    ringing( QString(), resolveRingType( type ) );
}

void QModemCallProvider::callNotification( const QString& msg )
{
    if ( msg.startsWith( "NO CARRIER" ) ||
         msg.startsWith( "NO ANSWER" ) ||
         msg.startsWith( "NO DIALTONE" ) ||
         msg.startsWith( "BUSY" ) ) {

        // Foreground call was aborted by remote side or network error.
        if ( !d->hangupTimer->isActive() ) {
            hangupRemote( 0 );
        }

    }
}

void QModemCallProvider::clip( const QString& msg )
{
    // Caller ID information for an incoming call.
    uint posn = 6;
    QString number = QAtUtils::nextString( msg, posn );
    uint numberType = QAtUtils::parseNumber( msg, posn );
    number = QAtUtils::decodeNumber( number, numberType );

    // Report the number via ringing() with an unknown call type.
    // We will get the real call type from +CRING or AT+CLCC.
    ringing( number, QString() );
}

void QModemCallProvider::colp( const QString& msg )
{
    // Caller ID information for an outgoing call.
    uint posn = 6;
    QString number = QAtUtils::nextString( msg, posn );
    uint numberType = QAtUtils::parseNumber( msg, posn );
    number = QAtUtils::decodeNumber( number, numberType );

    // Find the current Dialing or Alerting call and emit a notification on it.
    QModemCall *call = dialingCall();
    if ( call )
        call->emitNotification( QPhoneCall::ConnectedLineId, number );
}

void QModemCallProvider::ccwa( const QString& msg )
{
    // A call waiting notification was received: handle like "RING".
    uint posn = 6;
    QString number = QAtUtils::nextString( msg, posn );
    uint numberType = QAtUtils::parseNumber( msg, posn );
    number = QAtUtils::decodeNumber( number, numberType );

    // Report the number via ringing() with an unknown call type.
    // We will get the real call type from +CRING or AT+CLCC.
    ringing( number, QString() );
}

void QModemCallProvider::detectTimeout()
{
    // If we still don't have a call type at this point, use AT+CLCC
    // to try to discover the incoming call type.
    if ( d->incomingCallType.isEmpty() ) {
        atchat()->chat( "AT+CLCC", this, SLOT(clccIncoming(bool,QAtResult)) );
    } else {
        announceCall();
    }
}

void QModemCallProvider::missedTimeout(QModemCall *call)
{
    d->detectTimer->stop();
    if ( call ) {
        qLog(Modem) << "Reporting missed call.";
        call->setState( QPhoneCall::Missed );
    }
}

void QModemCallProvider::missedTimeout()
{
    missedTimeout( incomingCall() );
}

void QModemCallProvider::stopRingTimers()
{
    // Stop the ring timers because the call has been accepted or hung up.
    d->detectTimer->stop();
    d->missedTimer->stop();
}

void QModemCallProvider::announceCall()
{
    // We now assume that we have enough information to announce
    // the call to the rest of the system.
    qLog(Modem) << "Qtopia detected an incoming call, notifying applications.";

    // Create the new call object and set it to incoming.
    QModemCall *call = (QModemCall *)create( QString(), d->incomingCallType );
    call->setModemIdentifier( d->incomingModemIdentifier );
    call->setNumber( d->incomingNumber );
    call->setActions( QPhoneCallImpl::Accept );
    call->setState( QPhoneCall::Incoming );

    // Reset the incoming values for the next call.
    d->incomingNumber = QString();
    d->incomingCallType = QString();
    d->incomingModemIdentifier = 0;
}

void QModemCallProvider::clccIncoming( bool ok, const QAtResult& result )
{
    QString callType;
    if ( ok ) {
        QAtResultParser parser( result );
        while ( parser.next( "+CLCC:" ) ) {
            uint id = parser.readNumeric();
            parser.readNumeric();       // Don't need the direction code.
            uint state = parser.readNumeric();
            uint mode = parser.readNumeric();
            if ( state == 4 || state == 5 ) {
                // Make sure that we use the correct modem identifier.
                releaseModemIdentifier( d->incomingModemIdentifier );
                useModemIdentifier( id );
                d->incomingModemIdentifier = id;

                // Convert the mode into something useful.
                callType = resolveCallMode( mode );
                break;
            }
        }
    } else {
        // The AT+CLCC command failed for some reason.  It may not
        // be responding properly or the modem doesn't support it.
        // Assume that the call is voice.
        callType = "Voice";     // No tr
    }
    if ( callType.isEmpty() ) {
        // We could not determine the call type, so the incoming call
        // has probably disappeared.  Flag it as a missed voice call.
        d->incomingCallType = "Voice";      // No tr
        announceCall();
        d->missedTimer->stop();
        missedTimeout();
    } else {
        d->incomingCallType = callType;
        announceCall();
    }
}

// Mark a modem identifier as currently in use.
void QModemCallProvider::useModemIdentifier( uint id )
{
    if ( id > 0 && id <= 32 )
        d->usedIds |= ( ((uint)1) << (id - 1) );
}

// Release a modem identifier that is no longer required.
void QModemCallProvider::releaseModemIdentifier( uint id )
{
    if ( id > 0 && id <= 32 )
        d->usedIds &= ~( ((uint)1) << (id - 1) );
}

/*!
    Allocates the next modem identifier in rotation that is not
    currently used by a call.
*/
uint QModemCallProvider::nextModemIdentifier()
{
    uint id;
    for ( id = 1; id <= 32; ++id ) {
        if ( ( d->usedIds & ( ((uint)1) << (id - 1) ) ) == 0 ) {
            d->usedIds |= ( ((uint)1) << (id - 1) );
            return id;
        }
    }
    return 0;
}

bool QModemCallProvider::otherActiveCalls( QModemCall *notThis )
{
    QList<QPhoneCallImpl *> calls = this->calls();
    QList<QPhoneCallImpl *>::ConstIterator iter;
    QModemCall *callp;
    for ( iter = calls.begin(); iter != calls.end(); ++iter ) {
        callp = (QModemCall *)(*iter);
        if ( callp != notThis && callp->callType() == "Voice" ) { // No tr
            if ( callp->state() == QPhoneCall::Connected ||
                 callp->state() == QPhoneCall::Hold ) {
                return true;
            }
        }
    }
    return false;
}

void QModemCallProvider::changeGroup
    ( QPhoneCall::State oldState, QPhoneCall::State newState,
      QPhoneCallImpl::Actions newActions )
{
    QList<QPhoneCallImpl *> calls = this->calls();
    QList<QPhoneCallImpl *>::ConstIterator iter;
    for ( iter = calls.begin(); iter != calls.end(); ++iter ) {
        if ( (*iter)->callType() != "Voice" )   // No tr
            continue;
        if ( (*iter)->state() == oldState ) {
            (*iter)->setActions( newActions );
            (*iter)->setState( newState );
        }
    }
}

bool QModemCallProvider::hasGroup( QPhoneCall::State state )
{
    QList<QPhoneCallImpl *> calls = this->calls();
    QList<QPhoneCallImpl *>::ConstIterator iter;
    for ( iter = calls.begin(); iter != calls.end(); ++iter ) {
        if ( (*iter)->callType() != "Voice" )   // No tr
            continue;
        if ( (*iter)->state() == state ) {
            return true;
        }
    }
    return false;
}

void QModemCallProvider::activateWaitingOrHeld()
{
    // Note: The GSM 02.30 spec says that commands that "activate the held
    // or waiting call" will choose the waiting call ahead of a held call.

    QModemCall *callp = incomingCall();
    QPhoneCallImpl::Actions actions;
    if ( callp ) {

        // The waiting call has been activated.
        actions = QPhoneCallImpl::Tone | QPhoneCallImpl::Transfer;
        if ( !hasGroup( QPhoneCall::Hold ) )
            actions |= QPhoneCallImpl::Hold;
        else
            actions |= QPhoneCallImpl::Join | QPhoneCallImpl::JoinAndDetach;
        callp->setActions( actions );
        callp->setState( QPhoneCall::Connected );

    } else {

        // The held calls have been activated.
        actions = QPhoneCallImpl::Tone | QPhoneCallImpl::Transfer;
        if ( !hasGroup( QPhoneCall::Connected ) &&
             !hasGroup( (QPhoneCall::State)(-1) ) ) // During swap transition.
            actions |= QPhoneCallImpl::Hold;
        else
            actions |= QPhoneCallImpl::Join | QPhoneCallImpl::JoinAndDetach;
        QList<QPhoneCallImpl *> calls = this->calls();
        QList<QPhoneCallImpl *>::ConstIterator iter;
        for ( iter = calls.begin(); iter != calls.end(); ++iter ) {
            if ( (*iter)->callType() != "Voice" )   // No tr
                continue;
            if ( (*iter)->state() == QPhoneCall::Hold ) {
                (*iter)->setActions( actions );
                (*iter)->setState( QPhoneCall::Connected );
            }
        }

    }
}

QModemPPPdManager *QModemCallProvider::pppdManager() const
{
    return d->pppdManager;
}

int QModemCallProvider::vtsType() const
{
    return d->vtsType;
}

void QModemCallProvider::setVtsType( int type )
{
    d->vtsType = type;
}

/*
 * Setting this to false means that we won't listen to CRING, RING, CLIP and other
 * indications but assume someone will call ring with the right parameter
 * and then announce the call
*/
void QModemCallProvider::setUseDetectTimer(bool useTimer)
{
    d->useDetectTimer = useTimer;
}

bool QModemCallProvider::useDetectTimer() const
{
    return d->useDetectTimer;
}

void QModemCallProvider::setUseMissedTimer(bool useTimer)
{
    d->useMisssedTimer = useTimer;
}

bool QModemCallProvider::useMissedTimer() const
{
    return d->useMisssedTimer;
}

void QModemCallProvider::llIndicatorsRinging()
{
    QModemLLIndicators::Ringing();
}