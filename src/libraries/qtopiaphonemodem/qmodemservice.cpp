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

#include <qmodemservice.h>
#include <qmodemserviceplugin.h>
#include <qmodemsiminfo.h>
#include <qmodemnetworkregistration.h>
#include <qmodempreferrednetworkoperators.h>
#include <qmodemcallbarring.h>
#include <qmodemcallforwarding.h>
#include <qmodemcallsettings.h>
#include <qmodemsmsreader.h>
#include <qmodemsmssender.h>
#include <qmodemcallprovider.h>
#include <qmodemsimtoolkit.h>
#include <qmodemphonebook.h>
#include <qmodemcellbroadcast.h>
#include <qmodemservicenumbers.h>
#include <qmodempinmanager.h>
#include <qmodemsupplementaryservices.h>
#include <qmodemindicators.h>
#include <qmodemllindicators.h>
#include <qmodemsimfiles.h>
#include <qmodemsimgenericaccess.h>
#include <qmodemconfiguration.h>
#include <qmodemvibrateaccessory.h>
#include <qmodemrffunctionality.h>
#include <qmodemgprsnetworkregistration.h>
#include <qmodemcallvolume.h>
#include <qservicechecker.h>
#include <qserialport.h>
#include <qretryatchat.h>
#include <qpluginmanager.h>
#include <custom.h>
#include <qtopialog.h>
#include <qslotinvoker.h>
#include <qtopiaipcenvelope.h>
#include <qtopiaipcadaptor.h>
#include <QTimer>
#include <QMap>

/*!
    \class QModemService
    \inpublicgroup QtCellModule

    \brief The QModemService class implements telephony functionality for AT-based modems.
    \ingroup telephony::modem

    The default implementation uses AT commands from the GSM standards
    3GPP TS 07.07, 3GPP TS 07.05, 3GPP TS 27.007, and 3GPP TS 27.005.

    The implementation can be customized for proprietary modem command sets
    by inheriting QModemService and overriding QModemService::initialize(),
    which should create new interfaces to replace the default
    implementations.

    For example, consider a modem that uses different commands for
    network registration than the GSM standard specifies.  The modem
    integrator would create a new modem service class that overrides
    the default implementation of network registration:

    \code
    class MyModemService : public QModemService
    {
        ...
        void initialize();
        ...
    };

    void MyModemService::initialize()
    {
        if ( !supports<QNetworkRegistration>() )
            addInterface( new MyNetworkRegistration(this) );
        QModemService::initialize();
    }
    \endcode

    The new functionality can then be implemented in the
    \c MyNetworkRegistration class, which will inherit from
    QNetworkRegistrationServer.  The \c{MyModemService::initialize()}
    method first calls \c{supports()} to check if an instance
    of QNetworkRegistration has already been added.

    This modem service handles the following posted events,
    via QModemService::post():

    \list
        \o \c{needsms} - ask that the SMS system be initialized and made
           ready for use.  QModemService responds by posting
           \c{smsready} when SMS becomes available.  Modem vendor
           plug-ins can override the needSms() method to provide an
           alternative implementation.
        \o \c{smsready} - the SMS system is initialized and ready for use.
    \endlist

    \sa QTelephonyService
*/

class QModemServicePrivate
{
public:
    QModemServicePrivate()
    {
        smsRetryCount = 0;
        provider = 0;
        firstInitDone = false;
    }

    QSerialIODeviceMultiplexer *mux;
    QAtChat *primaryAtChat;
    QAtChat *secondaryAtChat;
    int smsRetryCount;
    QStringList pending;
    QMultiMap< QString, QSlotInvoker * > invokers;
    QModemCallProvider *provider;
    QModemIndicators *indicators;
    bool firstInitDone;
};

/*!
    Creates a new modem service handler called \a service and
    attaches it to \a parent.

    The \a device parameter specifies the serial device and baud
    rate to use to communicate with the modem (for example,
    \c{/dev/ttyS0:115200}).

    If \a device is empty, the default serial device specified
    by the \c QTOPIA_PHONE_DEVICE environment variable is used.
*/
QModemService::QModemService
        ( const QString& service, const QString& device, QObject *parent )
    : QTelephonyService( service, parent )
    , phoneVs("/Communications/Calls")
{
    QSerialIODevice *dev;
    QSerialIODeviceMultiplexer *mux;

    if ( device.isEmpty() ) {
        // Create a multiplexer for the default serial device.
        mux = QSerialIODeviceMultiplexer::create();
    } else {
        // Create a multiplexer for the specified serial device.
        dev = QSerialPort::create( device );
        if ( !dev )
            dev = new QNullSerialIODevice();
        mux = QSerialIODeviceMultiplexer::create( dev );
    }

    init( mux );
}

/*!
    Creates a new modem service handler called \a service and
    attaches it to \a parent.  The \a mux parameter specifies the
    serial device multiplexer to use for accessing the modem.

    This version of the constructor may be needed if the modem
    is accessed via some mechanism other than kernel-level
    serial devices.  The caller would construct an appropriate
    multiplexer wrapper around the new mechanism before calling
    this constructor.
*/
QModemService::QModemService
        ( const QString& service, QSerialIODeviceMultiplexer *mux,
          QObject *parent )
    : QTelephonyService( service, parent )
    , phoneVs("/Communications/Calls")
{
    init( mux );
}

void QModemService::init( QSerialIODeviceMultiplexer *mux )
{
    d = new QModemServicePrivate();
    d->mux = mux;
    d->primaryAtChat = mux->channel( "primary" )->atchat();
    d->secondaryAtChat = mux->channel( "secondary" )->atchat();

    // Make sure that the primary command channel is open and valid.
    QSerialIODevice *primary = d->mux->channel( "primary" );
    if ( !primary->isOpen() )
        primary->open( QIODevice::ReadWrite );

    // Set a slightly different debug mode for the secondary channel,
    // to make it easier to see which command goes where.
    if ( d->primaryAtChat != d->secondaryAtChat )
        d->secondaryAtChat->setDebugChars( 'f', 't', 'n', '?' );

    connectToPost( "needsms", this, SLOT(needSms()) );
    connectToPost( "simready", this, SLOT(firstTimeInit()) );

    d->indicators = new QModemIndicators( this );

    // Listen for suspend() and wake() messages on QPE/ModemSuspend.
    QtopiaIpcAdaptor *suspend =
        new QtopiaIpcAdaptor( "QPE/ModemSuspend", this );
    QtopiaIpcAdaptor::connect( suspend, MESSAGE(suspend()),
                               this, SLOT(suspend()) );
    QtopiaIpcAdaptor::connect( suspend, MESSAGE(wake()),
                               this, SLOT(wake()) );
    
    // Connect to missed calls counter for low level LED missed calls indicator
    connect(&phoneVs, SIGNAL(contentsChanged()), this, SLOT(phoneVsChanged()));
}

/*!
    Destroys this modem service and all of its interfaces.
*/
QModemService::~QModemService()
{
    delete d;
}

/*!
    \reimp
*/
void QModemService::initialize()
{
    // If the modem does not exist, then tell clients via QServiceChecker.
    if ( !supports<QServiceChecker>() ) {
        QSerialIODevice *primary = d->mux->channel( "primary" );
        addInterface( new QServiceCheckerServer
            ( service(), primary->isValid(), this ) );
    }

    if ( !supports<QSimInfo>() )
        addInterface( new QModemSimInfo( this ) );

    if ( !supports<QPinManager>() )
        addInterface( new QModemPinManager( this ) );

    if ( !supports<QNetworkRegistration>() )
        addInterface( new QModemNetworkRegistration( this ) );

    if ( !supports<QPreferredNetworkOperators>() )
        addInterface( new QModemPreferredNetworkOperators( this ) );

    if ( !supports<QCallBarring>() )
        addInterface( new QModemCallBarring( this ) );

    if ( !supports<QCallForwarding>() )
        addInterface( new QModemCallForwarding( this ) );

    if ( !supports<QCallSettings>() )
        addInterface( new QModemCallSettings( this ) );

    if ( !supports<QSMSReader>() )
        addInterface( new QModemSMSReader( this ) );

    if ( !supports<QSMSSender>() )
        addInterface( new QModemSMSSender( this ) );

    if ( !supports<QSimToolkit>() )
        addInterface( new QModemSimToolkit( this ) );

    if ( !supports<QSimFiles>() )
        addInterface( new QModemSimFiles( this ) );

    if ( !supports<QSimGenericAccess>() )
        addInterface( new QModemSimGenericAccess( this ) );

    if ( !supports<QPhoneBook>() )
        addInterface( new QModemPhoneBook( this ) );

    if ( !supports<QCellBroadcast>() )
        addInterface( new QModemCellBroadcast( this ) );

    if ( !supports<QServiceNumbers>() )
        addInterface( new QModemServiceNumbers( this ) );

    if ( !supports<QSupplementaryServices>() )
        addInterface( new QModemSupplementaryServices( this ) );

    if ( !supports<QTelephonyConfiguration>() )
        addInterface( new QModemConfiguration( this ) );

    if ( !supports<QPhoneRfFunctionality>() )
        addInterface( new QModemRfFunctionality( this ) );

    if ( !supports<QVibrateAccessory>() )
        addInterface( new QModemVibrateAccessory( this ) );

    if ( !supports<QGprsNetworkRegistration>() )
        addInterface( new QModemGprsNetworkRegistration( this ) );

    if ( !supports<QCallVolume>() )
        addInterface( new QModemCallVolume( this ) );

    // Create a default modem call provider if necessary.
    if ( !callProvider() )
        setCallProvider( new QModemCallProvider( this ) );

    QTelephonyService::initialize();
}

/*!
    Returns the serial device multiplexer that is being used to
    communicate with the modem.
*/
QSerialIODeviceMultiplexer *QModemService::multiplexer() const
{
    return d->mux;
}

/*!
    Returns the AT chat handler for the primary modem control channel.

    \sa secondaryAtChat()
*/
QAtChat *QModemService::primaryAtChat() const
{
    return d->primaryAtChat;
}

/*!
    Returns the AT chat handler for the secondary modem control channel.
    This will return the same as primaryAtChat() if the modem does not
    have a secondary modem control channel.

    \sa primaryAtChat()
*/
QAtChat *QModemService::secondaryAtChat() const
{
    return d->secondaryAtChat;
}

/*!
    Sends \a command to the modem on the primary AT chat channel.
    If the command fails, the caller will not be notified.

    \sa primaryAtChat(), retryChat()
*/
void QModemService::chat( const QString& command )
{
    d->primaryAtChat->chat( command );
}

/*!
    Sends \a command to the modem on the primary AT chat channel.
    When the command finishes, notify \a slot on \a target.  The slot
    has the signature \c{done(bool,QAtResult&)}.  The boolean parameter
    indicates if the command succeeded or not, and the QAtResult parameter
    contains the full result data.

    The optional \a data parameter can be used to pass extra user data
    that will be made available to the target slot in the QAtResult::userData()
    field.

    \sa primaryAtChat(), retryChat()
*/
void QModemService::chat( const QString& command, QObject *target,
                          const char *slot, QAtResult::UserData *data )
{
    d->primaryAtChat->chat( command, target, slot, data );
}

/*!
    Retry \a command up to 15 times, once per second, until it succeeds.
    This is typically used to send initialization commands that may need
    several tries before the modem will accept them during start up.

    \sa primaryAtChat(), chat()
*/
void QModemService::retryChat( const QString& command )
{
    new QRetryAtChat( d->primaryAtChat, command, 15 );
}

/*!
    Creates and returns a vendor-specific modem service handler called \a service
    (the default is \c modem) and attaches it to \a parent.

    The \a device parameter specifies the serial device and baud
    rate to use to communicate with the modem (for example,
    \c{/dev/ttyS0:115200}).

    If \a device is empty, the default serial device specified
    by the \c QTOPIA_PHONE_DEVICE environment variable is used.

    This function will load vendor-specific plug-ins to handle
    extended modem functionality as required.  If the
    \c QTOPIA_PHONE_VENDOR environment variable is set, then that
    vendor plug-in will be loaded.  Otherwise this function will
    issue the \c{AT+CGMI} command and ask each vendor plug-in if
    it supports the returned manufacturer value.

    \sa QModemServicePlugin
*/
QModemService *QModemService::createVendorSpecific
        ( const QString& service, const QString& device, QObject *parent )
{
    QSerialIODeviceMultiplexer *mux;
    QSerialIODevice *dev;
    char *env;

    // Create a multiplexer.  This will consult QTOPIA_PHONE_VENDOR
    // to load a vendor-specific multiplexer plug-in if appropriate.
    if ( device.isEmpty() ) {
        // Create a multiplexer for the default serial device.
        mux = QSerialIODeviceMultiplexer::create();
    } else {
        // Create a multiplexer for the specified serial device.
        dev = QSerialPort::create( device );
        if ( !dev )
            dev = new QNullSerialIODevice();
        mux = QSerialIODeviceMultiplexer::create( dev );
    }

    // Get the name of the vendor modem plug-in to load.
    env = getenv( "QTOPIA_PHONE_VENDOR" );
#ifdef QTOPIA_PHONE_VENDOR
    if ( !env || *env == '\0' )
        env = QTOPIA_PHONE_VENDOR;  // Allow custom.h to override.
#endif

    // Handle vendor-specific modem plug-ins.
    static QPluginManager *pluginLoader = 0;
    if (!pluginLoader)
        pluginLoader = new QPluginManager( "phonevendors" );
    QModemServicePluginInterface *plugin;
    QModemService *serv = 0;
    QString name;
    QObject *obj;
    if ( env && *env != '\0' ) {

        // Load the specified plug-in and call its creation function.
        name = QString(env) + "vendor";
        qLog(Modem) << "querying single modem plug-in" << name;
        obj = pluginLoader->instance( name );
        if( ( plugin = qobject_cast<QModemServicePluginInterface*>( obj ) )
                    != 0 ) {
            if( plugin->keys().contains( "QModemServicePluginInterface" ) ) {
                serv = plugin->create( service, mux, parent );
            }
        }

    } else {

        // Send AT+CGMI to the modem to determine its type.
        QSerialIODevice *primary = mux->channel( "primary" );
        if ( !primary->isOpen() )
            primary->open( QIODevice::ReadWrite );
        QString manufacturer =
            QSerialIODeviceMultiplexer::chatWithResponse( primary, "AT+CGMI" );
        if ( !manufacturer.isEmpty() ) {

            // Scan all modem plug-ins until one says it supports the value.
            QStringList list = pluginLoader->list();
            QStringList::Iterator it;
            for ( it = list.begin(); it != list.end(); ++it ) {
                name = *it;
                qLog(Modem) << "querying modem plug-in" << name;
                obj = pluginLoader->instance( name );
                if( ( plugin = qobject_cast
                        <QModemServicePluginInterface*>( obj ) ) != 0 ) {
                    if( plugin->keys().contains
                            ( "QModemServicePluginInterface" ) &&
                        plugin->supports( manufacturer ) ) {
                        serv = plugin->create( service, mux, parent );
                        break;
                    }
                }
            }

        }

    }

    // If we don't have a service yet, then create the default implementation.
    if ( !serv ) {
        qLog(Modem) << "No modem vendor plug-in found - using default";
        serv = new QModemService( service, mux, parent );
    }

    // Return the new service to the caller.
    return serv;
}

/*!
    Requests the SMS service from the modem and places it into PDU mode.
    The "smsready" item will be posted when it is ready.  If the SMS
    service is already known to be ready and in PDU mode, then the
    "smsready" item will be posted upon the next entry to the event loop.
    The object requesting SMS service should use connectToPost() to monitor
    for when "smsready" is posted.

    This implementation repeatedly sends \c{AT+CMGF=0} every second
    for fifteen seconds until the command succeeds or the retry count expires.
    Higher levels that call this function should timeout their requests
    after fifteen seconds if "smsready" has not been received in the meantime.

    \sa post(), connectToPost()
*/
void QModemService::needSms()
{
    // Update the final timeout if there is a ready check in progress,
    // but otherwise let it continue.
    if ( d->smsRetryCount > 0 ) {
        d->smsRetryCount = 15;
        return;
    }

    // Set the initial retry count and send the first check command.
    d->smsRetryCount = 15;
    sendNeedSms();
}

/*!
    Processes a request to suspend modem operations and to put the
    modem into a low-power state.  The modem vendor plug-in must call
    suspendDone() once the operation completes.  The default implementation
    calls suspendDone().

    This is intended for modems that need a special AT command or
    device \c ioctl operation to suspend the modem.  If the modem does
    not need any special help, this function can be ignored.

    This function is called in response to a \c{suspend()} command
    on the QCop channel \c{QPE/ModemSuspend}.

    \sa wake(), suspendDone()
*/
void QModemService::suspend()
{
    suspendDone();
}

/*!
    Processes a request to wake up the modem from a \c suspend() state.
    The modem vendor plug-in must call \c wakeDone() once the operation
    completes.  The default implementation calls \c wakeDone().

    This function is called in response to a \c{wake()} command
    on the QCop channel \c{QPE/ModemSuspend}.

    \sa wakeDone(), suspend()
*/
void QModemService::wake()
{
    wakeDone();
}

/*!
    Notifies the system that a suspend() operation has completed.
    The \c{suspendDone()} message is sent on the QCop channel
    \c{QPE/ModemSuspend}.

    \sa suspend(), wake()
*/
void QModemService::suspendDone()
{
    QtopiaIpcEnvelope env( "QPE/ModemSuspend", "suspendDone()" );
}

/*!
    Notifies the system that a wake() operation has completed.
    The \c{wakeDone()} message is sent on the QCop channel
    \c{QPE/ModemSuspend}.

    \sa wake(), suspend()
*/
void QModemService::wakeDone()
{
    QtopiaIpcEnvelope env( "QPE/ModemSuspend", "wakeDone()" );
}

void QModemService::cmgfDone( bool ok )
{
    if ( ok ) {
        d->smsRetryCount = 0;
        post( "smsready" );
    } else if ( --( d->smsRetryCount ) > 0 ) {
        QTimer::singleShot( 1000, this, SLOT(sendNeedSms()) );
    }
}

void QModemService::sendNeedSms()
{
    chat( "AT+CMGF=0", this, SLOT(cmgfDone(bool)) );
}

/*!
    Posts \a item to functionality providers that have expressed an
    interest via connectToPost() or posted().  The posted item
    will be delivered upon the next entry to the event loop.

    Posted events are used as a simple communication mechanism
    between functionality providers.  Providers that post items
    are completely decoupled from providers that consume items.

    For example, posting the item \c{sms:needed} will ask the SMS
    functionality provider to initialize the SMS subsystem.  Once SMS
    has been initialized, it will post the \c{sms:available} item.
    Other providers that depend upon SMS initialization can then
    proceed with using the SMS functionality.

    \sa connectToPost(), posted()
*/
void QModemService::post( const QString& item )
{
    d->pending += item;
    if ( d->pending.size() == 1 )
        QTimer::singleShot( 0, this, SLOT(postItems()) );
}

/*!
    Connects \a slot on \a target to receive posted \a item values.
    This can be used as an alternative to the posted() signal.
    Only those posted items that equal \a item will be delivered
    to the slot.

    \sa post(), posted()
*/
void QModemService::connectToPost
        ( const QString& item, QObject *target, const char *slot )
{
    QSlotInvoker *invoker = new QSlotInvoker( target, slot, this );
    d->invokers.insert( item, invoker );
}

/*!
    Returns the modem indicator object for this modem service.

    \sa QModemIndicators
*/
QModemIndicators *QModemService::indicators() const
{
    return d->indicators;
}

/*!
    \fn void QModemService::posted( const QString& item )

    Signal that is emitted whenever an \a item is posted using post().
    Slots that are connected to this signal should filter on \a item
    to determine if the posted item is relevant to them.  Alternatively,
    they can use connectToPost() to only receive notification of
    specific posted items.

    \sa post(), connectToPost()
*/

/*!
    \fn void QModemService::resetModem()

    Signal that is emitted when the modem has reset, either during start up
    or just after a \c{AT+CFUN} command.  Slots that connect to this signal
    should issue AT commands to put the modem into a known state and to
    enable unsolicited notifications of interest to the connected object.
*/

void QModemService::postItems()
{
    // Save the current list and then clear it.  New items could
    // be added to the list while we are posting the old ones.
    QStringList items = d->pending;
    d->pending = QStringList();

    // Post the queued items.
    QMultiMap< QString, QSlotInvoker * >::ConstIterator it;
    QList<QVariant> args;
    foreach ( QString item, items ) {
        emit posted( item );
        for ( it = d->invokers.find( item );
              it != d->invokers.end() && it.key() == item; ++it ) {
            it.value()->invoke( args );
        }
    }
}

void QModemService::firstTimeInit()
{
    if ( !d->firstInitDone ) {
        d->firstInitDone = true;

        // Force the SIM toolkit routines to initialize.
        // Initialization will then move on to other subsystems.
        QModemSimToolkit *sim =
            qobject_cast<QModemSimToolkit *>( interface<QSimToolkit>() );
        if ( sim )
            sim->initialize();
        else
            emit resetModem();

        // Preload the "SM" phone book, to try to speed up the apparent
        // startup time on SIM phone books.
        QTimer::singleShot(10000, this, SLOT(phoneBookPreload()));
    } else {
        emit resetModem();
    }
}

void QModemService::phoneBookPreload()
{
    QModemPhoneBook *pb;
    pb = qobject_cast<QModemPhoneBook *>( interface<QPhoneBook>() );
    if ( pb )
        pb->preload( "SM" );
}

void QModemService::stkInitDone()
{
    // STK init is done, so we can reset the modem now.
    emit resetModem();
}

bool QModemService::supportsAtCced()
{
    return true;
}

void QModemService::phoneVsChanged()
{
    int num = phoneVs.value("MissedCalls").toInt();
    QModemLLIndicators::MissedCallsChanged(num);
}