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

#include "vendor_neo_p.h"
#include <qmodemindicators.h>
#include <qatutils.h>
#include <qatresultparser.h>
#include <QProcess>
#include <QTimer>
#include <stdio.h>
#include <stdlib.h>
#include <QFile>
#include <QTextStream>
#include <QSettings>

#include <alsa/asoundlib.h>

#include <qmodemcallvolume.h>
#include <qmodemsiminfo.h>

static bool supportsStk = false;

NeoCallProvider::NeoCallProvider( QModemService *service )
    : QModemCallProvider( service )
{
    modemService = service;
    service->primaryAtChat()->registerNotificationType
        ( "%CPI:", this, SLOT(cpiNotification(QString)) );
    service->primaryAtChat()->registerNotificationType
        ( "%CNAP:", this, SLOT(cnapNotification(QString)) );
    setUseMissedTimer(false);
    setUseDetectTimer(false);
}

NeoCallProvider::~NeoCallProvider()
{
}

QModemCallProvider::AtdBehavior NeoCallProvider::atdBehavior() const
{
    // When ATD reports OK or NO CARRIER, it indicates that it is
    // back in command mode. We just want to ignore these in QModemCall
    // as we will have %CPI that is going to give us the right state.
    return AtdOkIgnore;
}

void NeoCallProvider::abortDial( uint id, QPhoneCall::Scope scope )
{
    // Use the ATH command to abort outgoing calls, instead of AT+CHLD=1.
    //atchat()->chat( "ATH" );

    // Use default behaviour of CR followed by AT+CHLD - seems to work better.
    QModemCallProvider::abortDial( id, scope );
}

QString NeoCallProvider::acceptCallCommand( bool otherActiveCalls ) const
{
    // do undocumented echo cancellation and noise reduction
    modemService->primaryAtChat()->chat( "AT@ST=\"-26\"" );
    modemService->primaryAtChat()->chat( "AT%N0187" );
    return QModemCallProvider::acceptCallCommand( otherActiveCalls );
}

void NeoCallProvider::cpiNotification( const QString& msg )
{
    // Call progress notification for the NEO device.
    // %CPI: <id>,<msgType>,<ibt>,<tch>,<dir>,<mode>,<number>,<ton>,<alpha>
    // where <id> is the call identifier, and <msgType> is one of:
    // 0 = SETUP, 1 = DISCONNECT, 2 = ALERT, 3 = PROCEED,
    // 4 = SYNCHRONIZATION, 5 = PROGRESS, 6 = CONNECTED,
    // 7 = RELEASE, 8 = REJECT
    // dir: 0 = mobile originated, 1 = mobile terminated, 2 = network initiaited mobile
    // originated call, 3 = redialing mobile originated
    uint posn = 5;
    uint identifier = QAtUtils::parseNumber( msg, posn );

    uint status = QAtUtils::parseNumber( msg, posn );
    QAtUtils::skipField( msg, posn );
    QAtUtils::skipField( msg, posn );
    uint direction = QAtUtils::parseNumber( msg, posn );
    QModemCall *call = callForIdentifier( identifier );

    if ( status == 6 && call &&
         ( call->state() == QPhoneCall::Dialing ||
           call->state() == QPhoneCall::Alerting ) ) {

        // This is an indication that a "Dialing" connection
        // is now in the "Connected" state.
        call->setConnected();

    } else if ( ( status == 1 || status == 7 ) && call &&
                ( call->state() == QPhoneCall::Dialing ||
                  call->state() == QPhoneCall::Alerting ) ) {

        // We never managed to connect.
        hangupRemote( call );

    } else if ( status == 2 && call &&
                call->state() == QPhoneCall::Dialing ) {

        // Call is moving from Dialing to Alerting.
        call->setState( QPhoneCall::Alerting );

    } else if ( ( status == 1 || status == 7 ) && call &&
                ( call->state() == QPhoneCall::Connected ||
                  call->state() == QPhoneCall::Hold ) ) {

        // This is an indication that the connection has been lost.
        hangupRemote( call );

    } else if ( ( status == 1 || status == 7 ) && call &&
                call->state() == QPhoneCall::Incoming ) {

        // This is an indication that an incoming call was missed.
        call->setState( QPhoneCall::Missed );

    } else if ( ( status == 2 || status == 4 || status == 0) && !call && direction == 1) {
        // only for incoming calls
        // if you make an outgoing call, these states can occur, but
        // we don't want an aborted outgoing call to be registered as missed
        // therefore we use the direction parameter

        // This is a newly waiting call.  Treat it the same as "RING".
        uint mode = QAtUtils::parseNumber( msg, posn );
        QString callType;
        if ( mode == 1 || mode == 6 || mode == 7 )
            callType = "Data";  // No tr
        else if ( mode == 2 || mode == 8 )
            callType = "Fax";   // No tr
        else
            callType = "Voice"; // No tr
        QString number = QAtUtils::nextString( msg, posn );
        uint type = QAtUtils::parseNumber( msg, posn );
        ringing( QAtUtils::decodeNumber( number, type ), callType, identifier );

	// We got everything we need, indicate the call to the outside world
	announceCall();
    }
}

void NeoCallProvider::cnapNotification( const QString& msg )
{
    // Calling name presentation from the network.
    uint posn = 6;
    QAtUtils::skipField( msg, posn );	    // pres_mode
    QAtUtils::skipField( msg, posn );	    // dcs
    QAtUtils::skipField( msg, posn );	    // name_length
    QString name = QAtUtils::nextString( msg, posn );
    QModemCall *call = incomingCall();
    if ( call )
        call->emitNotification( QPhoneCall::CallingName, name );
}

QString NeoCallProvider::dialVoiceCommand(const QDialOptions& options) const
{
    // do undocumented echo cancellation and noise reduction
    modemService->primaryAtChat()->chat( "AT@ST=\"-26\"" );
    modemService->primaryAtChat()->chat( "AT%N0187" );
    return QModemCallProvider::dialVoiceCommand(options);
}

NeoSimToolkit::NeoSimToolkit( QModemService *service )
    : QModemSimToolkit( service )
{
    supportsStk = false;
    lastCommand.setType( QSimCommand::NoCommand );
    mainMenu = lastCommand;
    lastResponseWasExit = false;

    service->primaryAtChat()->registerNotificationType
        ( "%SATA:", this, SLOT(sataNotification(QString)) );
    service->primaryAtChat()->registerNotificationType
	( "%SATN:", this, SLOT(satnNotification(QString)) );
}

NeoSimToolkit::~NeoSimToolkit()
{
}

void NeoSimToolkit::initialize()
{
    // We don't need to do anything here, because SIM toolkit initialization
    // happens during the detection code.
    QModemSimToolkit::initialize();
}

void NeoSimToolkit::begin()
{
    if ( !supportsStk ) {

	// SIM toolkit functionality is not available.
	emit beginFailed();

    } else if ( lastCommand.type() == QSimCommand::SetupMenu ) {

	// We just fetched the main menu, so return what we fetched.
	emit command( lastCommand );

    } else if ( mainMenu.type() == QSimCommand::SetupMenu ) {

	// We have a cached main menu from a previous invocation.
	lastCommand = mainMenu;
	lastCommandBytes = mainMenuBytes;
	emit command( mainMenu );

    } else {

	// The SIM toolkit is in an unknown state, so we cannot proceed.
	// If the NEO could perform a proper STK reset, we might have
	// been able to do something.
	emit beginFailed();

    }
}

void NeoSimToolkit::sendResponse( const QSimTerminalResponse& resp )
{
    if ( resp.command().type() == QSimCommand::SelectItem &&
         resp.result() == QSimTerminalResponse::BackwardMove ) {
        lastResponseWasExit = true;
    } else {
        lastResponseWasExit = false;
    }
    service()->primaryAtChat()->chat
        ( "AT%SATR=\"" + QAtUtils::toHex( resp.toPdu() ) + "\"" );
}

void NeoSimToolkit::sendEnvelope( const QSimEnvelope& env )
{
    service()->primaryAtChat()->chat
        ( "AT%SATE=\"" + QAtUtils::toHex( env.toPdu() ) + "\"" );
}

void NeoSimToolkit::sataNotification( const QString& msg )
{
    // SIM toolkit command indication.
    QByteArray bytes = QAtUtils::fromHex( msg.mid(6) );
    if ( bytes.size() > 0 ) {

        lastCommandBytes = bytes;
        lastCommand = QSimCommand::fromPdu( bytes );
        if ( lastCommand.type() == QSimCommand::SetupMenu ) {
            // Cache the main menu, because we won't get it again!
            mainMenuBytes = bytes;
            mainMenu = lastCommand;
        }
        qLog(AtChat)<< "SIM command of type" << (int)(lastCommand.type());
        emitCommandAndRespond( lastCommand );

    } else if ( lastResponseWasExit &&
                mainMenu.type() == QSimCommand::SetupMenu ) {

        // We exited from a submenu and we got an empty "%SATA"
        // response.  This is the NEO's way of telling us that we
        // now need to display the main menu.  It would have been
        // better if the NEO resent the menu to us itself.
        lastCommandBytes = mainMenuBytes;
        lastCommand = mainMenu;
        qLog(AtChat)<< "Simulating SIM command of type"<< (int)(lastCommand.type());
        emit command( lastCommand );

    }
}

void NeoSimToolkit::satnNotification( const QString& )
{
    // Nothing to do here at present.  Just ignore the %SATN notifications.
}

/*
 * Reimplementation because we don't want the standard notifications
 * as we have a CPI which should give us everything that we need.
 * We don't need:
 *    +CRING (CPI gives us the calltype)
 *    +CLIP  (CPI gives us the number)
 *    RING
 */
void NeoCallProvider::resetModem()
{
    // We don't want RING, CRING, CLIP, COLP, COWA  but callNotification
    // disable all of these and do not call the base class
    atchat()->chat( "AT+CRC=0" );
    service()->retryChat( "AT+CLIP=0" );
    service()->retryChat( "AT+COLP=0" );
    service()->retryChat( "AT+CCWA=1" );
}

NeoPhoneBook::NeoPhoneBook( QModemService *service )
    : QModemPhoneBook( service ),
      m_phoneBookIsReady( false ),
      m_smsIsReady( false )
{
    this->service = service;
    qLog(AtChat)<<"NeoPhoneBook::NeoPhoneBook";
    // Turn on status notification messages for finding out when
    // the phone book is ready to use.

    service->primaryAtChat()->registerNotificationType
        ( "%CSTAT:", this, SLOT(cstatNotification(QString)) );
    service->primaryAtChat()->chat( "AT%CSTAT=1" );
}

NeoPhoneBook::~NeoPhoneBook()
{
}

bool NeoPhoneBook::hasModemPhoneBookCache() const
{
    return true;
}

bool NeoPhoneBook::hasEmptyPhoneBookIndex() const
{
    return true;
}

void NeoPhoneBook::cstatNotification( const QString& msg )
{
    QString entity = msg.mid( 8, 3);

//    bool phonebkOk = false;
//    bool smsOk = false;
    // PHB (phone book)
    // SMS
    // RDY (Ready when both PHB and SMS have reported they are ready)
    uint status = msg.mid(13).toInt();

    if (status != 1)
        return;

    if (entity == "PHB")
        m_phoneBookIsReady = true;
    else if (entity == "SMS")
        m_smsIsReady = true;
    else if (entity == "RDY") {
        m_smsIsReady = true;
        m_phoneBookIsReady = true;
    }

    if  (m_smsIsReady && m_phoneBookIsReady) {
        qLog(Modem) << __PRETTY_FUNCTION__ << "simready";
        this->service->post("simready");
        phoneBooksReady();
    }
}

NeoPinManager::NeoPinManager( QModemService *service )
    : QModemPinManager( service )
{
}

NeoPinManager::~NeoPinManager()
{
}

bool NeoPinManager::emptyPinIsReady() const
{
    return true;
}


// Known bands, by mask.
typedef struct
{
    const char *name;
    int         value;

} BandInfo;
static BandInfo const bandInfo[] = {
    {"GSM 900",             1},
    {"DCS 1800",            2},
    {"PCS 1900",            4},
    {"E-GSM",               8},
    {"GSM 850",             16},
    {"Tripleband 900/1800/1900", 15},
};
#define numBands    ((int)(sizeof(bandInfo) / sizeof(BandInfo)))

NeoBandSelection::NeoBandSelection( QModemService *service )
    : QBandSelection( service->service(), service, Server )
{
    this->service = service;
}

NeoBandSelection::~NeoBandSelection()
{
}

void NeoBandSelection::requestBand()
{
    service->primaryAtChat()->chat
        ( "AT%BAND?", this, SLOT(bandQuery(bool,QAtResult)) );
}

void NeoBandSelection::requestBands()
{
//     QStringList list;
//     for ( int index = 0; index < numBands; ++index ) {
//         list += QString( bandInfo[index].name );
//     }
//     emit bands( list );

    service->primaryAtChat()->chat
        ( "AT%BAND=?", this, SLOT(bandList(bool,QAtResult)) );
}

void NeoBandSelection::setBand( QBandSelection::BandMode mode, const QString& value )
{
    if ( mode == Automatic ) {
        service->primaryAtChat()->chat
            ( "AT%BAND=0", this, SLOT(bandSet(bool,QAtResult)) );
    } else {
        int bandValue = 0;
        QStringList names = value.split(", ");
        foreach ( QString name, names ) {
            bool seen = false;
            for ( int index = 0; index < numBands; ++index ) {
                if ( name == bandInfo[index].name ) {
                    bandValue |= bandInfo[index].value;
                    seen = true;
                    break;
                }
            }
            if ( !seen ) {
                // The band name is not valid.
                emit setBandResult( QTelephony::OperationNotSupported );
                return;
            }
        }
        if ( !bandValue ) {
            // No band names supplied.
            emit setBandResult( QTelephony::OperationNotSupported );
            return;
        }
        service->primaryAtChat()->chat
            ( "AT%BAND=1," + QString::number( bandValue ),
              this, SLOT(bandSet(bool,QAtResult)) );
    }
}

// Convert a band value into a name.  Returns an empty list if unknown.
static QStringList bandValueToName( int bandValue )
{
    QStringList bands;
    for ( int index = 0; index < numBands; ++index ) {
        if ( ( bandValue & bandInfo[index].value ) == bandInfo[index].value ) {
            bandValue &= ~bandInfo[index].value;
            bands += QString( bandInfo[index].name );
        }
    }
    return bands;
}

void NeoBandSelection::bandQuery( bool, const QAtResult& result )
{

    QAtResultParser parser( result );
    int bandValue;
        qLog(Modem)<<"bandQuery";
    if ( parser.next( "%BAND:" ) ) {
        bandValue = (int)parser.readNumeric();
    } else {
        // Command did not work, so assume "Auto".
        bandValue = 4;
    }
    for ( int index = 0; index < numBands; ++index ) {
        if ( bandValue == bandInfo[index].value ) {
            emit band( Manual, bandInfo[index].name );
            return;
        }
    }
    emit band( Automatic, QString() );



//     QAtResultParser parser( result );
//     int bandValue;
//     qLog(AtChat)<<"bandQuery";
//     if ( parser.next( "%BAND:" ) ) {
//         if ( parser.readNumeric() != 0 ) {
//             bandValue = (int)parser.readNumeric();
//             QStringList bands = bandValueToName( bandValue );
//             if ( bands.size() > 0 ) {
//                 emit band( Manual, bands.join(", ") );
//                 return;
//             }

//         }
//     }

//     emit band( Automatic, QString() );

}

void NeoBandSelection::bandList( bool, const QAtResult& result )
{
    QAtResultParser parser( result );
    QStringList bandNames;
    if ( parser.next( "%BAND:" ) ) {

        parser.readList();  // Skip list of supported modes.
        QList<QAtResultParser::Node> list = parser.readList();
        foreach ( QAtResultParser::Node node, list ) {

            if ( node.isNumber() ) {
                bandNames += bandValueToName( (int)node.asNumber() );
                qLog(Modem)<<  (int)node.asNumber();
            } else if ( node.isRange() ) {
                int first = (int)node.asFirst();
                int last = (int)node.asLast();
                qLog(Modem)<<"isRange"<<first<<last;
                while ( first <= last ) {
                    qLog(Modem)<< bandValueToName( first ) << first;
                    bandNames += bandValueToName( first ).join(" | ");
                    ++first;
                }
            }
        }
    }
     emit bands( bandNames );
}

void NeoBandSelection::bandSet( bool, const QAtResult& result )
{
    emit setBandResult( (QTelephony::Result)result.resultCode() );
}

NeoModemService::NeoModemService
        ( const QString& service, QSerialIODeviceMultiplexer *mux,
          QObject *parent )
    : QModemService( service, mux, parent )
{
    connect( this, SIGNAL(resetModem()), this, SLOT(reset()) );
    // Register a wakeup command to ping the modem if we haven't
    // sent anything during the last 5 seconds.  This command may
    // not get a response, but the modem should then become responsive
    // to the next command that is sent afterwards.
    primaryAtChat()->registerWakeupCommand( QChar(0x1a), 5000 );

    // Turn on dynamic signal quality notifications.
    // Register for "%CSQ" notifications to get signal quality updates.
    primaryAtChat()->registerNotificationType
        ( "%CSQ:", this, SLOT(csq(QString)) );
    chat("AT%CSQ=1");
    QTimer::singleShot( 2500, this, SLOT(firstCsqQuery()) );


    // Turn on SIM toolkit support in the modem.  This must be done
    // very early in the process, to ensure that it happens before
    // the first AT+CFUN command.

    // until we get something intelligent from the modem...
    // chat( "AT%SATC=1,\"FFFFFFFFFF\"", this, SLOT(configureDone(bool)) );

    // Enable %CPRI for ciphering indications.
    //    chat( "AT%CPRI=1" );

    // Make the modem send unsolicited reports at any time
    // the "user is not typing".  i.e. don't intersperse unsolicited
    // notifications and command echo as it will confuse QAtChat.
    chat( "AT%CUNS=1" );

    // Enable the reporting of timezone and local time information.
    primaryAtChat()->registerNotificationType
        ( "%CTZV:", this, SLOT(ctzu(QString)), true );
    chat( "AT%CTZV=1" );

    // Turn on call progress indications, with phone number information.
    chat( "AT%CPI=2" );

    //  chat("AT%CMGRS=1"); //message transmission to get any failed sms during suspend

    QString deepsleep="adaptive";
    QSettings cfg("Trolltech", "Modem");
    cfg.beginGroup("DeepSleep");
    deepsleep = cfg.value("Active",deepsleep).toString();
    qLog(Modem) << "DeepSleep value:" << deepsleep;

    if (deepsleep == "never")
        chat("AT%SLEEP=2"); 
    else 
        chat("AT%SLEEP=4");

    // Turn cell id information back on.
    chat( "AT+CREG=2" );
    chat( "AT+CGREG=2" );

    QTimer::singleShot(40000, this, SLOT(sendRego()));
}

NeoModemService::~NeoModemService()
{
}

void NeoModemService::sendRego()
{
    qLog(Hardware) << __PRETTY_FUNCTION__;
    //begin really ugky hack
    QSettings cfg("Trolltech", "PhoneProfile");
    cfg.beginGroup("Profiles");
    if( !cfg.value("PlaneMode",false).toBool()) {
        //       chat("AT%NRG=0,0"); //force auto operations
        chat("AT+COPS=0"); //force auto operations
    }
}

void NeoModemService::initialize()
{
#if 0
    if ( !supports<QSimToolkit>() )
        addInterface( new NeoSimToolkit( this ) );
#endif

    if ( !supports<QPinManager>() )
        addInterface( new NeoPinManager( this ) );

    if ( !supports<QPhoneBook>() )
        addInterface( new NeoPhoneBook( this ) );


    if ( !supports<QBandSelection>() )
        addInterface( new NeoBandSelection( this ) );

    if ( !supports<QSimInfo>() )
        addInterface( new NeoSimInfo( this ) );

    if ( !callProvider() )
        setCallProvider( new NeoCallProvider( this ) );

   if ( !supports<QVibrateAccessory>() )
        addInterface( new NeoVibrateAccessory( this ) );

   if ( !supports<QServiceNumbers>() )
        addInterface( new NeoServiceNumbers( this ) );

    if ( !supports<QCallVolume>() )
        addInterface( new NeoCallVolume(this));

    if ( !supports<QPreferredNetworkOperators>() )
        addInterface( new NeoPreferredNetworkOperators(this));

//    if ( ! supports <QModemNetworkRegistration>())
//        addInterface( new NeoModemNetworkRegistration(this));

   QModemService::initialize();
}

void NeoModemService::csq( const QString& msg )
{
    // Automatic signal quality update, in the range 0-31.
    if ( msg.contains( QChar(',') ) ) {
        uint posn = 6;
        uint rssi = QAtUtils::parseNumber( msg, posn );
        indicators()->setSignalQuality( (int)rssi, 31 );
    }
}

void NeoModemService::firstCsqQuery()
{
    // Perform an initial AT+CSQ? which should cause the modem to
    // respond with a +CSQ notification.  This is needed to shut
    // off AT+CSQ polling in qmodemindicators.cpp when the modem is
    // quiet and not sending periodic +CSQ notifications at startup.
    chat( "AT%CSQ?" );
}

void NeoModemService::ctzu( const QString& msg )
{
    // Timezone information from the network.  Format is "yy/mm/dd,hh:mm:ss+/-tz".
    // There is no dst indicator according to the spec, but we parse an extra
    // argument just in case future modem firmware versions fix this oversight.
    // If there is no extra argument, the default value of zero will be used.
    uint posn = 7;
    qLog(Modem) << __PRETTY_FUNCTION__ << msg;
    QString time = QAtUtils::nextString( msg, posn );
    int dst = ((int)QAtUtils::parseNumber( msg, posn )) * 60;
    int zoneIndex = time.length();
    while ( zoneIndex > 0 && time[zoneIndex - 1] != QChar('-') &&
            time[zoneIndex - 1] != QChar('+') )
        --zoneIndex;
    int zoneOffset;
    if ( zoneIndex > 0 && time[zoneIndex - 1] == QChar('-') ) {
        zoneOffset = time.mid(zoneIndex - 1).toInt() * 15;
    } else if ( zoneIndex > 0 && time[zoneIndex - 1] == QChar('+') ) {
        zoneOffset = time.mid(zoneIndex).toInt() * 15;
    } else {
        // Unknown timezone information.
        return;
    }
    QString timeString;
    if (zoneIndex > 0)
        timeString = time.mid(0, zoneIndex - 1);
    else
        timeString = time;
    QDateTime t = QDateTime::fromString(timeString, "yy/MM/dd,HH:mm:ss");
    if (!t.isValid())
        t = QDateTime::fromString(timeString, "yyyy/MM/dd,HH:mm:ss"); // Just in case.
    QDateTime utc = QDateTime(t.date(), t.time(), Qt::UTC);
    utc = utc.addSecs(-zoneOffset * 60);
    indicators()->setNetworkTime( utc.toTime_t(), zoneOffset, dst );
}

void NeoModemService::configureDone( bool ok )
{
    supportsStk = ok;
}

void NeoModemService::reset()
{
    qLog(Modem)<<" NeoModemService::reset()";
    chat("AT%CWUP=1");
    chat("ATE0");

    // Turn on "%CNAP" notifications, which supply the caller's
    // name on an call.  Only supported on some networks.
    chat( "AT%CNAP=1" );
}

void NeoModemService::sendSuspendDone()
{
    suspendDone();
}

void NeoModemService::suspend()
{
    qLog(Modem)<<" NeoModemService::suspend()";
    // Turn off cell id information on +CREG and +CGREG as it will
    // cause unnecessary wakeups when moving between cells.

    chat("AT+CREG=0");
    chat("AT+CGREG=0");

    // Turn off timezone notifications.
    chat("AT+CTZR=0");
    chat("AT%CTZU=0");

    chat("AT+CSCB=0");

    chat("AT+CGEREP=0,0");
    //   chat("AT+CNMI=2,1,0,0,0");

    chat("AT%CBHZ=0");

    // Turn off signal quality notifications while the system is suspended.
    chat( "AT%CSQ=0", this, SLOT(mcsqOff()) );
}

void NeoModemService::wake()
{
    qLog(Modem)<<" NeoModemService::wake()";

    chat("ATE0");
    // Turn cell id information back on.
    chat( "AT+CREG=2" );
    chat( "AT+CGREG=2" );

     // Turn on timezone notifications again.
    chat( "AT+CTZR=1" );
    chat( "AT%CTZU=1" );

    chat("AT+CSCB=1");

    //   chat("AT%SLEEP=2");

    // Turn cell broadcast location messages back on again.
    //    chat("AT+CSCB=2");

    // Re-enable signal quality notifications when the system wakes up again.
    // Turn on dynamic signal quality notifications.

    // Re-enable signal quality notifications when the system wakes up again.
    chat( "AT%CSQ=1", this, SLOT(mcsqOn()) );
}

void NeoModemService::mcsqOff()
{
    QTimer::singleShot( 500, this, SLOT(sendSuspendDone()) );
}

void NeoModemService::mcsqOn()
{
    wakeDone();
}


 NeoVibrateAccessory::NeoVibrateAccessory
        ( QModemService *service )
    : QVibrateAccessoryProvider( service->service(), service )
{
    setSupportsVibrateOnRing( true );
    setSupportsVibrateNow( false );
}

NeoVibrateAccessory::~NeoVibrateAccessory()
{
}

void NeoVibrateAccessory::setVibrateOnRing( const bool value )
{
    qLog(Modem)<<"setVibrateOnRing "<<value;
    setVibrateNow(value);
}

void NeoVibrateAccessory::setVibrateNow( const bool value, int timeoutMs, int strength )
{
    qLog(Modem)<<"setVibrateNow "<<value;
    QString vibFile("/sys/class/leds/gta02::vibrator");
    if ( value ) { //turn on
        QFile trigger( vibFile + "/trigger");
        trigger.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate);
        QTextStream out(&trigger);
        out<<"timer";
        trigger.close();

        QFile delayOn( vibFile + "/delay_on");
        delayOn.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate);
        QTextStream out1(&delayOn);
        out1<<"500";
        delayOn.close();

        QFile delayOff(vibFile + "/delay_off");
        delayOff.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate);
        QTextStream out2(&delayOff);
        out2<<"1000";
        delayOff.close();

    } else { //turn off
        QFile trigger( vibFile + "/trigger");
        trigger.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate);
        QTextStream out(&trigger);
        out<<"none";
        trigger.close();
    }

    QVibrateAccessoryProvider::setVibrateNow( value );
}

NeoServiceNumbers::NeoServiceNumbers( QModemService *service )
    : QModemServiceNumbers( service )
{
    this->service = service;
}

NeoServiceNumbers::~NeoServiceNumbers()
{
}

void NeoServiceNumbers::requestServiceNumber
        ( QServiceNumbers::NumberId id )
{
    if ( id == QServiceNumbers::VoiceMail ) {
        QModemServiceNumbers::requestServiceNumberFromFile( id );
    } else {
        QModemServiceNumbers::requestServiceNumber( id );
    }
}

void NeoServiceNumbers::setServiceNumber
        ( QServiceNumbers::NumberId id, const QString& number )
{
    if ( id == QServiceNumbers::VoiceMail ) {
        QModemServiceNumbers::setServiceNumberInFile( id, number );
    } else {
        QModemServiceNumbers::setServiceNumber( id, number );
    }
}

NeoCallVolume::NeoCallVolume(NeoModemService *service)
    : QModemCallVolume(service)
{
   this->service = service;


    QtopiaIpcAdaptor *adaptor
            = new QtopiaIpcAdaptor( "QPE/NeoModem", this );

    QtopiaIpcAdaptor::connect
            ( adaptor, MESSAGE(setSpeakerVolumeRange(int, int)),
              this, SLOT(setSpeakerVolumeRange(int,int)) );
    QtopiaIpcAdaptor::connect
            ( adaptor, MESSAGE(setMicVolumeRange(int, int)),
              this, SLOT(setMicVolumeRange(int,int)) );

    QtopiaIpcAdaptor::connect
            ( adaptor, MESSAGE(setOutputVolume(int)),
              this, SLOT(setSpeakerVolume(int)) );
    QtopiaIpcAdaptor::connect
            ( adaptor, MESSAGE(setMicVolume(int)),
              this, SLOT(setMicrophoneVolume(int)) );

}

NeoCallVolume::~NeoCallVolume()
{

}

bool NeoCallVolume::hasDelayedInit() const
{
    return true;
}

void NeoCallVolume::setSpeakerVolume( int volume )
{
    int boundedVolume = qBound(value("MinimumSpeakerVolume").toInt(), volume,
                               value("MaximumSpeakerVolume").toInt());

    setValue( "SpeakerVolume", boundedVolume );
    emit speakerVolumeChanged(boundedVolume);
}

void NeoCallVolume::setMicrophoneVolume( int volume )
{
    int boundedVolume = qBound(value("MinimumMicrophoneVolume").toInt(), volume,
                               value("MaximumMicrophoneVolume").toInt());

    setValue( "MicrophoneVolume", boundedVolume );
    emit microphoneVolumeChanged(boundedVolume);
}


void NeoCallVolume::setSpeakerVolumeRange(int min,int max)
{
    setValue( "MinimumSpeakerVolume", min );
    setValue( "MaximumSpeakerVolume", max );
}

void NeoCallVolume::setMicVolumeRange(int min,int max)
{
    setValue( "MinimumMicrophoneVolume", min );
    setValue( "MaximumMicrophoneVolume", max );
}


// Number of milliseconds between polling attempts on AT+CIMI command.
#ifndef CIMI_TIMEOUT
#define CIMI_TIMEOUT    2000
#endif

class NeoSimInfoPrivate
{
public:
    NeoModemService *service;
    QTimer *checkTimer;
    int count;
    bool simPinRequired;
};

NeoSimInfo::NeoSimInfo( NeoModemService *service )
    : QSimInfo( service->service(), service, QCommInterface::Server )
{
    d = new NeoSimInfoPrivate();
    d->service = service;
    d->checkTimer = new QTimer( this );
    d->checkTimer->setSingleShot( true );
    connect( d->checkTimer, SIGNAL(timeout()), this, SLOT(requestIdentity()) );
    d->count = 0;
    d->simPinRequired = false;

    // Perform an initial AT+CIMI request to get the SIM identity.
    QTimer::singleShot( 0, this, SLOT(requestIdentity()) );

    // Hook onto the posted event of the service to determine
    // the current sim pin status
    connect( service, SIGNAL(posted(QString)), this, SLOT(serviceItemPosted(QString)) );
}

NeoSimInfo::~NeoSimInfo()
{
    delete d;
}

void NeoSimInfo::simInserted()
{
    if ( !d->checkTimer->isActive() )
        requestIdentity();
}

void NeoSimInfo::simRemoved()
{
    setIdentity( QString() );
}

void NeoSimInfo::requestIdentity()
{
    d->service->primaryAtChat()->chat
        ( "AT+CIMI", this, SLOT(cimi(bool,QAtResult)) );
}

void NeoSimInfo::cimi( bool ok, const QAtResult& result )
{
    QString id = extractIdentity( result.content().trimmed() );

    qLog(Modem) << __PRETTY_FUNCTION__ << id;

    if ( ok && !id.isEmpty() ) {
        // We have a valid SIM identity.
        setIdentity( id );
    } else {
        // No SIM identity, so poll again in a few seconds for the first two minutes.
        setIdentity( QString() );

        if ( d->count < 120000/CIMI_TIMEOUT ) {
            d->checkTimer->start( CIMI_TIMEOUT );
            d->count++;
        } else {
            d->count = 0;
            // If not waiting for SIM pin to be entered by the user
            if ( !d->simPinRequired ) {
                // post a message to modem service to stop SIM PIN polling
                d->service->post( "simnotinserted" );
                emit notInserted();
            }
        }
        // If we got a definite "not inserted" or "sim failure" error,
        // then emit notInserted().
        qLog(Modem) << __PRETTY_FUNCTION__ <<  result.resultCode();
        if ( result.resultCode() == QAtResult::SimNotInserted
          || result.resultCode() == QAtResult::SimFailure)
            emit notInserted();
    }
}

void NeoSimInfo::serviceItemPosted( const QString &item )
{
    if ( item == "simpinrequired" )
        d->simPinRequired = true;
    else if ( item == "simpinentered" )
        d->simPinRequired = false;
}

// Extract the identity information from the content of an AT+CIMI response.
// It is possible that we got multiple lines, including some unsolicited
// notifications from the modem that are not yet recognised.  Skip over
// such garbage and find the actual identity.
QString NeoSimInfo::extractIdentity( const QString& content )
{
    QStringList lines = content.split( QChar('\n') );
    foreach ( QString line, lines ) {
        if ( line.length() > 0 ) {
            uint ch = line[0].unicode();
            if ( ch >= '0' && ch <= '9' )
                return line;
        }
    }
    return QString();
}

NeoPreferredNetworkOperators::NeoPreferredNetworkOperators( QModemService *service )
    : QModemPreferredNetworkOperators( service )
{
    // We have to delete an entry before we can write operator details into it.
    setDeleteBeforeUpdate( true );

    // Quote operator numbers when modifying preferred operator entries.
    setQuoteOperatorNumber( true );
}

NeoPreferredNetworkOperators::~NeoPreferredNetworkOperators()
{
}

