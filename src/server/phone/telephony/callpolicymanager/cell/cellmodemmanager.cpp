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

#include "cellmodemmanager.h"
#include "servercontactmodel.h"

#include <QPhoneProfile>
#include <qtopialog.h>
#include <qtopiaapplication.h>
#include <qsiminfo.h>
#include <qcallsettings.h>
#include <qphonerffunctionality.h>
#include <QServiceChecker>
#include <QPinManager>
#include <QTimer>
#include <QTranslatableSettings>
#include <QContact>

static bool profilesControlModem = true;
static bool cellModemManagerInstance = false;

// declare CellModemManagerPrivate
class CellModemManagerPrivate
{
public:
    CellModemManagerPrivate()
    : m_state(CellModemManager::Initializing),
      m_netReg(NULL), m_aerialOn(false), m_aerialStable(false),
      m_pinManager(NULL), m_regState(QTelephony::RegistrationNone),
      m_status(NULL), m_autoRegisterTimer(NULL), m_profiles(NULL),
      m_profilesBlocked(false), m_callForwarding(NULL),
      m_callForwardingEnabled(false),
      m_rfFunc(NULL), m_phoneBook(NULL) {}

    CellModemManager::State m_state;
    QNetworkRegistration *m_netReg;
    bool m_aerialOn;
    bool m_aerialStable;
    QPinManager *m_pinManager;
    QString m_operator;
    QString m_operatorCountry;
    QTelephony::RegistrationState m_regState;
    QString m_location;
    QValueSpaceObject *m_status;
    QTimer *m_autoRegisterTimer;
    QPhoneProfileManager *m_profiles;
    bool m_profilesBlocked;
    QCallForwarding *m_callForwarding;
    bool m_callForwardingEnabled;
    QPhoneRfFunctionality *m_rfFunc;
    QPhoneBook *m_phoneBook;
};

/*!
    \class CellModemManager
    \inpublicgroup QtCellModule
    \brief The CellModemManager class simplifies the initialization and monitoring of a cellular phone modem.
    \ingroup QtopiaServer::Telephony

    The CellModeManager provides a Qt Extended Server Task.  Qt Extended Server Tasks are
    documented in full in the QtopiaServerApplication class documentation.

    \table
    \row \o Task Name \o CellModem
    \row \o Interfaces \o CellModemManager
    \row \o Services \o Suspend
    \endtable

    The CellModemManager class provides a simplified goal-oriented wrapper around
    the Telephony APIs.  While the telephony subsystem provides the ability
    to query and control every aspect of a cellular modem, the CellModemManager
    class focuses only the task of initializing the telephony subsystem and
    monitoring common attributes such as the current operator or cell location.

    To reduce the complexity of developing a system using CellModemManager, the
    class rigidly enforces a state transition model of startup and shutdown.
    Refer to the CellModemManager::State enumeration for this model and an exact
    description of each state.  By guarenteeing the transition flow, validating
    users of the API is greatly reduced.

    In addition to the C++ methods provided, the CellModemManager class also
    sets the following value space keys:
    \table
    \header \o Key \o Description
    \row \o \c {/Telephony/Status/RegistrationState} \o Set to the current registration state of the network.  Possible values are "None", "Home", "Searching", "Denied", "Unknown" or "Roaming" and correspond to the values of the QTelephony::RegistrationState enumeration.
    \row \o \c {/Telephony/Status/NetworkRegistered} \o Set to true if the network is registered (in the home or roaming states), otherwise false.
    \row \o \c {/Telephony/Status/Roaming} \o Set to true if the network is in the roaming statem otherwise false.
    \row \o \c {/Telephony/Status/CellLocation} \o Set to the current cell location.  This is equivalent to the value returned by the cellLocation() method.
    \row \o \c {/Telephony/Status/OperatorName} \o Set to the current network operator name.  This is equivalent to the value returned by the networkOperator() method.
    \row \o \c {/Telephony/Status/OperatorCountry} \o Set to the current network operator's country name, if available.  This is equivalent to the value returned by the networkOperatorCountry() method.
    \row \o \c {/Telephony/Status/CallDivert} \o True if voice calls will be unconditionally diverted, false otherwise.
    \row \o \c {/Telephony/Status/PlaneModeAvailable} \o True if the modem supports "plane mode".  Possible values are "Yes", "No" or "Unknown".  The "Unknown" value is set during modem initialization before Qt Extended has determined whether the modem can support plane mode.
    \row \o \c {/Telephony/Status/ModemStatus} \o Set to the string value of the current State enumeration value.
    \row \o \c {/Telephony/Status/SimToolkit/Available} \o Set to true if SIM toolkit support is available, otherwise false.
    \row \o \c {/Telephony/Status/SimToolkit/IdleModeText} \o Set to the value of the idle mode text string from the SIM toolkit application.
    \row \o \c {/Telephony/Status/SimToolkit/IdleModeIcon} \o Set to the QIcon to display in idle mode.
    \row \o \c {/Telephony/Status/SimToolkit/IdleModeIconSelfExplanatory} \o Set to true if the idle mode icon is self-explanatory without accompanying text.
    \row \o \c {/Telephony/Status/SimToolkit/MenuTitle} \o Set to the title of the SIM toolkit application's main menu.
    \endtable

    This class is part of the Qt Extended server and cannot be used by other Qt Extended applications.
*/

/*!
  \enum CellModemManager::State

  The State enumeration represents the current state of the CellModemManager
  class.  The allowable transitions between each of the 14 possible states is
  strictly defined and enforced.  The following diagram shows \i all of the
  state transitions possible.

  \image cellmodemmanager.png

  \value NoCellModem There is no cell modem present on the device.
  \value Initializing The modem is initializing, but has not yet determined
  whether a SIM is present or if a pin is required.
  \value Initializing2 The modem is initializing.  A SIM card is present and
  a pin number is either not required or has been correctly entered.
  \value Ready The modem is ready to connect to the network and service user
  requests.  Most status changed notifications and requests that the
  CellModemManager class supports will return default or empty values until it
  enters the Ready state.
  \value WaitingSIMPin The modem requires a SIM pin number to continue
  initializing.  The pin number should be requested from the user and entered
  through the setSimPin() method.
  \value VerifyingSIMPin The modem has received the SIM pin number and is
  verifying whether it is correct.  If incorrect, it will reenter the
  WaitingSIMPin state (or the WaitingSIMPuk state if the maximum number of SIM
  pin attempts has expired), otherwise it will proceed to the Initializing2
  state.
  \value WaitingSIMPuk The modem requires a SIM puk number to continue
  initializing.  The SIM puk is an unlock code for the SIM which is generally
  required if the SIM pin is incorrectly entered a number of times.  The
  specific number of attempts allowed is determined by either the SIM card or
  the modem itself.  The puk number, and a new pin number, should be entered
  through the setSimPuk() method.
  \value VerifyingSIMPuk The modem has received the SIM puk number and is
  verifying wither it is correct.
  \value SIMDead The user has attempted entry of the SIM puk more than the
  allowed number of times and the SIM has disabled itself.  The specific number
  of attempts allowed is determined by either the SIM card or the modem itself.
  Generally SIMDead means that a new SIM card will be required for the account.
  Emergency calls will still be available in this state.
  \value SIMMissing The SIM card is missing and needs to be inserted.
  Emergency calls will still be available in this state.
  \value AerialOff The modem is fully initialized, but is in plane mode with the
  aerial turned off.
  \value FailureReset The modem has encountered an error and is resetting.
  \value UnrecoverableFailure The modem has failed in such a way that it cannot
  be recovered.  Qt Extended or the device will need to be restarted.
 */

// define CellModemManager
/*!
  \internal

  Construct a new CellModemManager with the appropriate \a parent.  Only one
  instance of CellModemManager may be constructed.
 */
CellModemManager::CellModemManager(QObject *parent)
: QAbstractCallPolicyManager(parent), d(new CellModemManagerPrivate)
{
    Q_ASSERT(!cellModemManagerInstance);
    cellModemManagerInstance = true;

    d->m_status = new QValueSpaceObject("/Telephony/Status", this);
    d->m_status->setAttribute("ModemStatus", "Initializing");

    QValueSpaceItem *simToolkitAvailable;
    simToolkitAvailable = new QValueSpaceItem
            ("/Telephony/Status/SimToolkit/Available", this);
    connect(simToolkitAvailable, SIGNAL(contentsChanged()),
            this, SLOT(simToolkitAvailableChange()));

    // Check for modem
    QServiceChecker checker("modem");
    if(!checker.isValid()) {
        d->m_aerialOn = false;
        d->m_state = NoCellModem;
        updateStatus();
        return;
    }

    d->m_netReg = new QNetworkRegistration("modem", this);
    d->m_regState = d->m_netReg->registrationState();
    d->m_operator = d->m_netReg->currentOperatorName();

    QObject::connect(d->m_netReg, SIGNAL(registrationStateChanged()),
                     this, SLOT(registrationStateChanged()));
    QObject::connect(d->m_netReg, SIGNAL(currentOperatorChanged()),
                     this, SLOT(currentOperatorChanged()));

    // Rename signal for QAbstractCallPolicyManager.
    QObject::connect(this, SIGNAL(registrationStateChanged(QTelephony::RegistrationState)),
                     this, SIGNAL(registrationChanged(QTelephony::RegistrationState)));

    d->m_pinManager = new QPinManager("modem", this);
    QObject::connect(d->m_pinManager,
                     SIGNAL(pinStatus(QString,QPinManager::Status,QPinOptions)),
                     this,
                     SLOT(pinStatus(QString,QPinManager::Status,QPinOptions)) );

    d->m_rfFunc = new QPhoneRfFunctionality("modem", this);
    QObject::connect(d->m_rfFunc,
                     SIGNAL(levelChanged()), this, SLOT(rfLevelChanged()));

    d->m_callForwarding = new QCallForwarding("modem", this);
    QObject::connect(d->m_callForwarding,
                     SIGNAL(forwardingStatus(QCallForwarding::Reason,QList<QCallForwarding::Status>)),
                     this,
                     SLOT(forwardingStatus(QCallForwarding::Reason,QList<QCallForwarding::Status>)));

    QSimInfo *simInfo = new QSimInfo( "modem", this );
    connect( simInfo, SIGNAL(removed()), this, SLOT(simRemoved()) );
    connect( simInfo, SIGNAL(inserted()), this, SLOT(simInserted()) );

    if(::profilesControlModem) {
        d->m_profiles = new QPhoneProfileManager(this);
        d->m_aerialOn = !d->m_profiles->planeMode();
        QObject::connect(d->m_profiles, SIGNAL(planeModeChanged(bool)),
                         this, SLOT(planeModeChanged(bool)));
    }

    // If plane mode isn't an option, then we have to fully initialize the modem
    // each time
    if(!planeModeSupported())
        d->m_aerialOn = true;

    setAerialEnabled(d->m_aerialOn);
    updateStatus();

    doInitialize();
}

/*!
  Returns true if the modem supports plane mode.  Plane mode, a state in which
  the modem neither transmits or receives information, is suitable for use on
  an airplane.
 */
bool CellModemManager::planeModeSupported() const
{
    return NoCellModem != state() && d->m_rfFunc->available();
}

/*!
  Returns true if the modem is in or is transitioning into plane mode, false
  otherwise.  The call will return the last value set through
  setPlaneModeEnabled().

  As a modem takes time to respond to requests, planeModeEnabled() may not
  represent the true modem state.  Instead, it represents the intended state of
  the modem once all pending operations have been performed.

  The true state of the modem can be queried through the state() method.
  Ignoring potential abnormal conditions, the modem has reached a steady state
  when \c {state() == (planeModeEnabled()?AerialOff:Ready)}.
 */
bool CellModemManager::planeModeEnabled() const
{
    return !d->m_aerialOn;
}

/*!
  Sets the desired plane mode state to \a enabled.  If \a enabled is true, the
  CellModemManager will attempt to disable the modem aerial and put it into
  the AerialOff state.  If false, the aerial will be enabled and the manager
  will transition into the Ready state.

  If the modem does not support plane mode, this method will have no effect.
  Support for plane mode can be determined through the planeModeSupported()
  method.
 */
void CellModemManager::setPlaneModeEnabled(bool enabled)
{
    bool aerialEnabled = !enabled;
    if(d->m_aerialOn == aerialEnabled)
        return; // Already in progress
    if(!aerialEnabled && !planeModeSupported())
        return; // Plane mode is not supported

    d->m_aerialOn = aerialEnabled;

    if(Ready == state() || AerialOff == state()) {
        setAerialEnabled(aerialEnabled);
    }

    emit planeModeEnabledChanged(d->m_aerialOn);
    emit registrationStateChanged(d->m_regState);
}

void CellModemManager::setAerialEnabled(bool enabled)
{
    qLog(QtopiaServer) << "CellModemManager: Setting aerial enabled to"
                       << enabled;

    // We can only disable the modem if plane mode is supported
    Q_ASSERT(enabled || planeModeSupported());

    d->m_aerialOn = enabled;

    // Make change
    d->m_rfFunc->setLevel(d->m_aerialOn ? QPhoneRfFunctionality::Full
                          : QPhoneRfFunctionality::DisableTransmitAndReceive);
    d->m_aerialStable = false;

    // Force checking of the result
    d->m_rfFunc->forceLevelRequest();
}

void CellModemManager::registrationStateChanged()
{
    if(d->m_regState == d->m_netReg->registrationState())
        return;

    d->m_regState = d->m_netReg->registrationState();

    if(Ready == state()) {
        emit registrationStateChanged(d->m_regState);
        updateStatus();
    } else {
        tryDoReady();
    }

    doAutoRegister();
}

void CellModemManager::rfLevelChanged()
{
    QPhoneRfFunctionality::Level level = d->m_rfFunc->level();

    Q_ASSERT(level == QPhoneRfFunctionality::Full ||
             level == QPhoneRfFunctionality::DisableTransmitAndReceive);

    if(planeModeEnabled() && QPhoneRfFunctionality::Full == level) {
        // Need to disable aerial
        setAerialEnabled(false);

    } else if(!planeModeEnabled() &&
              QPhoneRfFunctionality::DisableTransmitAndReceive == level) {
        // Need to enable aerial
        setAerialEnabled(true);

    } else {
        d->m_aerialStable = true;
        tryDoAerialOff();
        tryDoReady();
    }
}

void CellModemManager::queryCallForwarding()
{
    d->m_callForwarding->requestForwardingStatus(QCallForwarding::Unconditional);
}

void CellModemManager::forwardingStatus(QCallForwarding::Reason reason,
        const QList<QCallForwarding::Status>& status)
{
    if(reason == QCallForwarding::Unconditional) {
        bool callFwdState = false;
        QList<QCallForwarding::Status>::ConstIterator it;
        for(it = status.begin(); it != status.end(); ++it) {
            if(((*it).cls & QTelephony::CallClassVoice) != 0) {
                callFwdState = true;
                break;
            }
        }

        if(callFwdState == d->m_callForwardingEnabled)
            return;

        d->m_callForwardingEnabled = callFwdState;
        if(Ready == state()) {
            emit callForwardingEnabledChanged(callFwdState);
            updateStatus();
        }
    }
}

/*!
  \fn void CellModemManager::registrationStateChanged(QTelephony::RegistrationState state)

  Emitted whenever the network registration \a state changes.
 */

/*!
  \fn void CellModemManager::networkOperatorChanged(const QString &networkOperator)

  Emitted whenever the network operator changes.  \a networkOperator will be set
  to the new value.
 */

/*!
  \fn void CellModemManager::stateChanged(CellModemManager::State newState,
                                          CellModemManager::State oldState)
  Emitted whenever the state of CellModemManager changes from \a oldState to
  \a newState.
 */

/*!
  Assuming profiles control has not been temporarily \l profilesBlocked()
  blocked, CellModemManager normally enables and disables the modem in response
  to changes in the plane mode value returned by QPhoneProfileManager.

  A call to this method disables this functionality and setModemEnabled() must
  be called manually to control the modem.  This method can only be called
  prior to CellModemManager's construction.

  \sa CellModemManager::profilesControlModem()
 */
void CellModemManager::disableProfilesControlModem()
{
    Q_ASSERT(!cellModemManagerInstance);
    ::profilesControlModem = false;
}

/*!
  Returns true if the plane mode value returned by QPhoneProfileManager will
  control whether the modem is enabled or disabled.

  \sa disableProfilesControlModem()
 */
bool CellModemManager::profilesControlModem()
{
    return ::profilesControlModem;
}

/*!
  If \a block is true, temporarily disable changes in phone profile from
  effecting whether or not the modem is enabled.  If \a block is false, the
  modem will be immediately set to the state dictated by the plane mode value
  returned by QPhoneProfileManager and any further changes to this value will
  result in changes to the modem state.

  \sa profilesBlocked()
  */
void CellModemManager::blockProfiles(bool block)
{
    d->m_profilesBlocked = block;
    if(d->m_profiles && !block &&
       d->m_profiles->planeMode() != planeModeEnabled()) {
        setPlaneModeEnabled(d->m_profiles->planeMode());
    }
}

/*!
  Return true if profiles have been temporarily blocked from effecting the
  modem state, false otherwise.

  \sa blockProfiles()
  */
bool CellModemManager::profilesBlocked()
{
    return d->m_profilesBlocked;
}

void CellModemManager::planeModeChanged(bool planeMode)
{
    if(!d->m_profilesBlocked)
        setPlaneModeEnabled(planeMode);
}

class EmergencyNumberList : public QStringList
{
public:
    EmergencyNumberList()
    {
        append("112");      // GSM 02.30, Europe
        append("911");      // GSM 02.30, US and Canada
        append("08");       // GSM 02.30, Mexico
        append("000");      // GSM 22.101, Australia
        append("999");      // GSM 22.101, United Kingdom
        append("110");      // GSM 22.101
        append("118");      // GSM 22.101
        append("119");      // GSM 22.101
    }
};
Q_GLOBAL_STATIC(EmergencyNumberList, emergencyNumbers);

/*!
  Return the list of emergency numbers.  This list is currently \c {112},
  \c {911}, \c {08}, \c {000}, \c {999}, \c {110}, \c {118}, and \c {119},
  but may change in the future.
 */
QStringList CellModemManager::emergencyNumbers()
{
    return *::emergencyNumbers();
}

/*!
  Destroy the CellModemManager instance.
 */
CellModemManager::~CellModemManager()
{
    delete d;
}

void CellModemManager::startAutoRegisterTimer()
{
    if(!d->m_autoRegisterTimer) {
        d->m_autoRegisterTimer = new QTimer(this);
        QObject::connect(d->m_autoRegisterTimer, SIGNAL(timeout()),
                         this, SLOT(autoRegisterTimeout()));
    }
    d->m_autoRegisterTimer->start(60000);
}

void CellModemManager::stopAutoRegisterTimer()
{
    if(d->m_autoRegisterTimer)
        d->m_autoRegisterTimer->stop();
}

/*!
  Return the current state of the CellModemManager instance.
 */
CellModemManager::State CellModemManager::state() const
{
    return d->m_state;
}

/*!
  Returns true if a network is registered, false otherwise.  A network is
  registered if registrationState() is either QTelephony::RegistrationHome or
  QTelephony::RegistrationRoaming.
 */
bool CellModemManager::networkRegistered() const
{
    return registrationState() == QTelephony::RegistrationHome ||
           registrationState() == QTelephony::RegistrationRoaming;
}

/*!
  \fn void CellModemManager::planeModeEnabledChanged(bool enabled)

  Emitted whenever the plane mode enabled value, as set by setPlaneModeEnabled()
  changes.  \a enabled will be set to the new value.
 */


/* Start the Initializing state */
void CellModemManager::doInitialize()
{
    Q_ASSERT(state() == Initializing);

    QSimInfo *simInfo = new QSimInfo( QString(), this );
    connect( simInfo, SIGNAL(notInserted()), this, SLOT(simNotInserted()) );

    if(d->m_pinManager->available()) {
        d->m_pinManager->querySimPinStatus();
    } else {
        // Pin support is not available
        doInitialize2();
    }
}

void CellModemManager::simNotInserted()
{
    // SIM missing or failure may be reported on some modems when
    // it is waiting for a PUK.
    if (WaitingSIMPuk == state())
        return;

    Q_ASSERT(Initializing == state() || SIMMissing == state());

    doStateChanged(SIMMissing);
}

void CellModemManager::fetchEmergencyNumbers()
{
    if ( d->m_phoneBook ) {
        connect
            ( d->m_phoneBook,
              SIGNAL(entries(QString,QList<QPhoneBookEntry>)),
              this,
              SLOT(emergencyNumbersFetched(QString,QList<QPhoneBookEntry>)) );
    }
    d->m_phoneBook->getEntries( "EN" );    // No tr
}

void CellModemManager::emergencyNumbersFetched
    ( const QString& store, const QList<QPhoneBookEntry>& list )
{
    if ( store == "EN" ) {          // No tr
        QList<QPhoneBookEntry>::ConstIterator it;
        QStringList *en = ::emergencyNumbers();
        for ( it = list.begin(); it != list.end(); ++it ) {
            QString number = (*it).number();
            if ( !number.isEmpty() && !en->contains( number ) )
                en->append( number );
        }
    }
}

void CellModemManager::pinStatus(const QString& type,
                                   QPinManager::Status status,
                                   const QPinOptions&)
{
    qLog(QtopiaServer) << "CellModemManager: Received request for pin"
                       << type << "status" << status << " state" << state();
    if(UnrecoverableFailure == state())
        return;

    if(type == "SIM PIN" || type == "SIM PUK" || type == "READY") {
        if(SIMMissing == state())
            doStateChanged(Initializing);
    }

    // We only expect SIM PIN in Initializing and Verifying states.
    // Can also see it in the Ready state when the user changes the pin
    // after entering a PUK.
    Q_ASSERT(type != "SIM PIN" || state() == Initializing || state() == Ready ||
             state() == VerifyingSIMPin || state() == VerifyingSIMPuk ||
             state() == Initializing2);
    // We only expect SIM PUK during Initializing, VerifyingSIMPin,
    // or VerifyingSIMPuk.  Note: it can also happen when the user
    // enters **05*PUK*PIN*PIN during the Ready state.
    Q_ASSERT(type != "SIM PUK" || state() == Initializing || state() == Ready ||
             state() == VerifyingSIMPin || state() == VerifyingSIMPuk);
    // In states Waiting and Verifying we only ever expect to
    // receive the SIM PIN or SIM_PUK pins
    Q_ASSERT((state() != WaitingSIMPuk && state() != VerifyingSIMPin &&
              state() != VerifyingSIMPuk && state() != WaitingSIMPin) ||
             "SIM PUK" == type || "SIM PIN" == type);
    // In state Initializing we only ever expect to receive the SIM PIN,
    // SIM_PUK or READY pins
    Q_ASSERT(state() != Initializing ||
             "SIM PUK" == type || "SIM PIN" == type || "READY" == type);
    // SIM PIN only supports NeedPin and Valid options
    Q_ASSERT("SIM PIN" != type || status == QPinManager::NeedPin ||
             status == QPinManager::Valid);
    // SIM PUK only supports NeedPuk, Valid and Locked options
    Q_ASSERT("SIM PUK" != type || status == QPinManager::NeedPuk ||
             status == QPinManager::Valid || status == QPinManager::Locked);

    if(type == "READY" && Initializing == state()) {
        // Jump straight to Initializing2
        doInitialize2();
    } else if(type == "SIM PIN") {
        // If its valid we continue to Initializing2, otherwise we enter
        // WaitingSIMPin
        if(QPinManager::Valid == status) {
            // Sweet - continuing time
            doInitialize2();
        } else if(QPinManager::NeedPin == status) {
            doStateChanged(WaitingSIMPin);
        }
    } else if(type == "SIM PUK") {
        // If locked the SIM's dead.  If valid we continue to Initializing2,
        // otherwise we enter WaitingSIMPuk
        if(QPinManager::Locked == status) {
            doStateChanged(SIMDead);
        } else if(QPinManager::NeedPuk == status) {
            doStateChanged(WaitingSIMPuk);
        } else if(QPinManager::Valid == status) {
            doInitialize2();
        }
    }
}

void CellModemManager::newlyRegistered()
{
    QTimer::singleShot(1000, this, SLOT(setCallerIdRestriction()));
    QTimer::singleShot(5000, this, SLOT(queryCallForwarding()));
}

void CellModemManager::setNotReadyStatus()
{
    stopAutoRegisterTimer();
    if(d->m_regState != QTelephony::RegistrationNone)
        emit registrationStateChanged(QTelephony::RegistrationNone);
    if(!d->m_operator.isEmpty())
        emit networkOperatorChanged(QString());
    if(d->m_callForwardingEnabled)
        emit callForwardingEnabledChanged(false);
    updateStatus();
}

void CellModemManager::simInserted()
{
    if ( !d->m_phoneBook ) {
        d->m_phoneBook = new QPhoneBook( "modem", this ); // No tr
        connect( d->m_phoneBook, SIGNAL(ready()),
                this, SLOT(readPhoneBooks()) );
    }
}

void CellModemManager::readPhoneBooks()
{
    // Arrange for the emergency number phone book to be read when SIM is ready.
    fetchEmergencyNumbers();
}

void CellModemManager::simRemoved()
{
    // Nothing to do here at present.
}

void CellModemManager::simToolkitAvailableChange()
{
    emit simToolkitAvailableChanged(simToolkitAvailable());
}

void CellModemManager::setReadyStatus()
{
    if(d->m_regState != QTelephony::RegistrationNone)
        emit registrationStateChanged(d->m_regState);
    if(!d->m_operator.isEmpty())
        emit networkOperatorChanged(d->m_operator);
    if(d->m_callForwardingEnabled)
        emit callForwardingEnabledChanged(d->m_callForwardingEnabled);

    updateStatus();
}

void CellModemManager::updateStatus()
{
    QString registrationText;
    switch(registrationState()) {
        case QTelephony::RegistrationNone:
            registrationText = "None";
            break;
        case QTelephony::RegistrationHome:
            registrationText = "Home";
            break;
        case QTelephony::RegistrationSearching:
            registrationText = "Searching";
            break;
        case QTelephony::RegistrationDenied:
            registrationText = "Denied";
            break;
        case QTelephony::RegistrationUnknown:
            registrationText = "Unknown";
            break;
        case QTelephony::RegistrationRoaming:
            registrationText = "Roaming";
            break;
    }
    d->m_status->setAttribute("RegistrationState", registrationText);
    d->m_status->setAttribute("NetworkRegistered",
                              registrationState() == QTelephony::RegistrationHome ||
                              registrationState() == QTelephony::RegistrationRoaming);
    d->m_status->setAttribute("Roaming",
                              registrationState() == QTelephony::RegistrationRoaming);
    d->m_status->setAttribute("OperatorName", networkOperator());
    d->m_status->setAttribute("OperatorCountry", networkOperatorCountry());
    d->m_status->setAttribute("CallDivert", callForwardingEnabled());
    d->m_status->setAttribute("PlaneModeAvailable", planeModeSupported()?"Yes":"No");
    d->m_status->setAttribute("CellModem/Available", cellModemAvailable());
    d->m_status->setAttribute("ModemStatus", stateToString(state()));
}

void CellModemManager::autoRegisterTimeout()
{
    Q_ASSERT(d->m_regState != QTelephony::RegistrationHome &&
             d->m_regState != QTelephony::RegistrationRoaming);

    d->m_netReg->setCurrentOperator(QTelephony::OperatorModeAutomatic);
}

/*!
  Set the SIM \a pin.  This method may only be called while CellModemManager is
  in the WaitingSIMPin state.
 */
void CellModemManager::setSimPin(const QString &pin)
{
    Q_ASSERT(WaitingSIMPin == state());
    d->m_pinManager->enterPin("SIM PIN", pin);
    doStateChanged(VerifyingSIMPin);
}

/*!
  Set the SIM \a puk and new \a pin.  This method may only be called while
  CellModemManager is in the WaitingSIMPuk state.
 */
void CellModemManager::setSimPuk(const QString &puk, const QString &pin)
{
    Q_ASSERT(WaitingSIMPuk == state());
    d->m_pinManager->enterPuk("SIM PUK", puk, pin);
    doStateChanged(VerifyingSIMPuk);
}

void CellModemManager::doInitialize2()
{
    doStateChanged(Initializing2);
    tryDoReady();
    tryDoAerialOff();
}

/* Moves to Ready state if it can. */
void CellModemManager::tryDoReady()
{
    if(Initializing2 != state() && AerialOff != state())
        return;

    if(QTelephony::RegistrationNone != d->m_regState) {
        if(QPhoneRfFunctionality::Full == d->m_rfFunc->level()) {
            doStateChanged(Ready);
            setReadyStatus();
            doAutoRegister();
        }
    }
}

/* Moves to AerialOff state if it can. */
void CellModemManager::tryDoAerialOff()
{
    if(Initializing2 != state() && Ready != state())
        return;

    if(planeModeEnabled() && d->m_aerialStable) {
        Q_ASSERT(QPhoneRfFunctionality::DisableTransmitAndReceive == d->m_rfFunc->level());
        doStateChanged(AerialOff);
        setNotReadyStatus();
        updateStatus();
    }
}

void CellModemManager::doAutoRegister()
{
    if(Ready == state()) {
        if(d->m_regState == QTelephony::RegistrationSearching) {
            // Don't initiate auto-registration if the device is already
            // searching for a network.
        } else if(d->m_regState != QTelephony::RegistrationHome &&
           d->m_regState != QTelephony::RegistrationRoaming) {
            d->m_netReg->setCurrentOperator(QTelephony::OperatorModeAutomatic);
            startAutoRegisterTimer();
        }  else {
            newlyRegistered();
            stopAutoRegisterTimer();
        }
    } else {
        stopAutoRegisterTimer();
    }
}

void CellModemManager::currentOperatorChanged()
{
    if(d->m_operator == d->m_netReg->currentOperatorName())
        return;

    d->m_operator = d->m_netReg->currentOperatorName();

    QString id = d->m_netReg->currentOperatorId();
    if ( !id.startsWith( QChar('2') ) ) {
        d->m_operatorCountry = QString();
    } else {
        QTranslatableSettings settings( "Trolltech", "GsmOperatorCountry" );
        settings.beginGroup( "Countries" );
        d->m_operatorCountry = settings.value( id.mid(1,3) ).toString();
    }

    if(Ready == state()) {
        emit networkOperatorChanged(d->m_operator);
        updateStatus();
    }
}

/*!
    \reimp
*/
QString CellModemManager::callType() const
{
    return "Voice";     // No tr
}

/*!
    \reimp
*/
QString CellModemManager::trCallType() const
{
    return tr("GSM");
}

/*!
    \reimp
*/
QString CellModemManager::callTypeIcon() const
{
    return "antenna";   // No tr
}

/*!
    \reimp
*/
QAbstractCallPolicyManager::CallHandling CellModemManager::handling
        (const QString& number)
{
    // Cannot handle if in plane mode.
    if (planeModeEnabled())
        return CannotHandle;

    // Emergency numbers must be handled by us, regardless of network reg.
    if (emergencyNumbers().contains(number))
        return MustHandle;

    // Cannot handle URI's that contain '@' or ':'.
    if (number.contains(QChar('@')) || number.contains(QChar(':')))
        return CannotHandle;

    // If no network registration, then cannot handle at this time.
    if (!networkRegistered())
        return CannotHandle;

    // Assume that this is a number that we can dial.
    return CanHandle;
}

/*!
    \reimp
*/
bool CellModemManager::isAvailable(const QString& number)
{
    // No way to detect presence, so just assume that all numbers
    // that got through handling() are available for dialing.
    Q_UNUSED(number);
    return true;
}

/*!
    \reimp
*/
QString CellModemManager::registrationMessage() const
{
    QString cellMsg;

    switch (registrationState()) {
        case QTelephony::RegistrationNone:
            if (!cellModemAvailable()) {
                cellMsg = tr("No modem");
            } else if (planeModeEnabled()) {
                cellMsg = tr("Airplane safe mode");
            } else if (state() == SIMMissing) {
                cellMsg = tr("Missing SIM");
            } else {
                cellMsg = tr("No network");
            }
            break;
        case QTelephony::RegistrationHome:
            // Nothing - this is the normal state
            break;
        case QTelephony::RegistrationSearching:
            cellMsg = tr("Searching for network");
            break;
        case QTelephony::RegistrationDenied:
            cellMsg = tr("Network registration denied");
            break;
        case QTelephony::RegistrationUnknown:
            //We can't do calls anyway
            cellMsg = tr("No network");
            break;
        case QTelephony::RegistrationRoaming:
            // ### probably want to beep/show message to let user know.
            break;
    }

    return cellMsg;
}

/*!
    \reimp
*/
QString CellModemManager::registrationIcon() const
{
    QString pix(":image/antenna");

    switch (registrationState()) {
        case QTelephony::RegistrationNone:
            if (cellModemAvailable() && planeModeEnabled())
                pix = ":image/aeroplane";
            break;
        default:
            break;
    }

    return pix;
}

/*!
  Returns the current network operator.  This call will always return an empty
  string unless CellModemManager is in the Ready state.
  */
QString CellModemManager::networkOperator() const
{
    if(Ready == state())
        return d->m_operator;
    else
        return QString();
}

/*!
  Returns the current network operator's country.  This call will always
  return an empty string unless CellModemManager is in the Ready state.
  */
QString CellModemManager::networkOperatorCountry() const
{
    if(Ready == state())
        return d->m_operatorCountry;
    else
        return QString();
}

/*!
  Returns the current network registration state.  This call will always return
  QTelephony::RegistrationNone unless CellModemManager is in the Ready state.
 */
QTelephony::RegistrationState CellModemManager::registrationState() const
{
    if(Ready == state())
        return d->m_regState;
    else
        return QTelephony::RegistrationNone;
}

/*!
  Updates the value in value space at \c {/Telephony/Status/CellLocation} to \a location.
*/
void CellModemManager::setCellLocation( const QString &location )
{
    d->m_status->setAttribute("CellLocation", location);
}

/*!
  Returns true if a sim supporting SIM toolkit applications is available,
  otherwise false.
 */
bool CellModemManager::simToolkitAvailable() const
{
    QValueSpaceItem item( "/Telephony/Status/SimToolkit" );
    return item.value( "Available" ).toBool();
}

/*!
  Returns true if a cell modem is available, otherwise false.
*/
bool CellModemManager::cellModemAvailable() const
{
    return d->m_state != NoCellModem;
}

/*!
  Returns the text to display when the system is in idle mode.
 */
QString CellModemManager::simToolkitIdleModeText() const
{
    QValueSpaceItem item( "/Telephony/Status/SimToolkit" );
    return item.value( "IdleModeText" ).toString();
}

/*!
  Returns the icon to display when the system is in idle mode.
 */
QIcon CellModemManager::simToolkitIdleModeIcon() const
{
    QValueSpaceItem item( "/Telephony/Status/SimToolkit" );
    return qVariantValue<QIcon>(item.value( "IdleModeIcon" ));
}

/*!
  Returns true if the idle mode icon is self-explanatory without
  accompanying text.
 */
bool CellModemManager::simToolkitIdleModeIconSelfExplanatory() const
{
    QValueSpaceItem item( "/Telephony/Status/SimToolkit" );
    return item.value( "IdleModeIconSelfExplanatory" ).toBool();
}

/*!
  Returns the title of the SIM toolkit application's main menu.
 */
QString CellModemManager::simToolkitMenuTitle() const
{
    QValueSpaceItem item( "/Telephony/Status/SimToolkit" );
    return item.value( "MenuTitle" ).toString();
}

/*!
  \fn void CellModemManager::simToolkitAvailableChanged(bool available)

  Emitted whenever the state of SIM toolkit availability changes.  \a available
  will be set to true if a sim supporting SIM toolkit applications is available,
  and false otherwise.

  \sa simToolkitAvailable()
 */

/*!
  Returns true if unconditional call forwarding is enabled.  This call will
  always return false unless CellModemManager is in the Ready state.
 */
bool CellModemManager::callForwardingEnabled() const
{
    if(Ready == state())
        return d->m_callForwardingEnabled;
    else
        return false;
}

/*!
  \fn void CellModemManager::callForwardingEnabledChanged(bool enabled)

  Emitted whenever the state of unconditional call forwarding changes.
  \a enabled will be true if unconditional call forwarding is on, and false
  otherwise.

  \sa callForwardingEnabled()
 */

/*!
  Returns \a state as a string.
 */
QString CellModemManager::stateToString(State state)
{
    switch(state) {
        case NoCellModem:
            return "NoCellModem";
        case Initializing:
            return "Initializing";
        case Initializing2:
            return "Initializing2";
        case Ready:
            return "Ready";
        case WaitingSIMPin:
            return "WaitingSIMPin";
        case VerifyingSIMPin:
            return "VerifyingSIMPin";
        case WaitingSIMPuk:
            return "WaitingSIMPuk";
        case VerifyingSIMPuk:
            return "VerifyingSIMPuk";
        case SIMDead:
            return "SIMDead";
        case SIMMissing:
            return "SIMMissing";
        case AerialOff:
            return "AerialOff";
        case FailureReset:
            return "FailureReset";
        case UnrecoverableFailure:
            return "UnrecoverableFailure";
        default:
            return QString();
    }
}

void CellModemManager::setCallerIdRestriction()
{
    QSettings cfg("Trolltech","Phone");
    cfg.beginGroup("CallerId");
    int choice = cfg.value("choice",-1).toInt();
    if (choice != -1) {
        // The Trolltech/Phone/CallerId/choice option has four possible values:
        //   0 = operator default QCallSettings::Subscription
        //   1 = Send my number to none QCallSettings::Invoked
        //   2 = Send my number to all QCallSettings::Suppressed
        //   4 = Send my number to Contacts
        // We disable caller id restriction if we're in mode 2.
        // XXX - Alternatively we could always set restriction to mode 1 and
        //       emulate mode 2 by making every number a caller id allowed dial.
        if (choice == 4) // when option 'To my contacts' is selected
            choice = 1;  // id restriction should be invoked, callerIdNeeded() will determine if id needed.
        QCallSettings callSettings;
        callSettings.setCallerIdRestriction( (QCallSettings::CallerIdRestriction)choice );
    }
    cfg.endGroup();
}

void CellModemManager::doStateChanged(State newState)
{
    qLog(QtopiaServer) << "CellModemManager: State changed from"
                       << stateToString(state()) << "to"
                       << stateToString(newState);

    // We assert on an impossible state transition
    Q_ASSERT(state() != Initializing ||
             newState == Initializing2 ||
             newState == WaitingSIMPin ||
             newState == WaitingSIMPuk ||
             newState == SIMDead ||
             newState == SIMMissing ||
             newState == FailureReset);

    Q_ASSERT(state() != NoCellModem);

    Q_ASSERT(state() != UnrecoverableFailure);

    Q_ASSERT(state() != WaitingSIMPuk ||
             newState == VerifyingSIMPuk ||
             newState == FailureReset);

    Q_ASSERT(state() != WaitingSIMPin ||
             newState == VerifyingSIMPin ||
             newState == FailureReset);

    Q_ASSERT(state() != FailureReset ||
             newState == UnrecoverableFailure ||
             newState == Initializing);

    Q_ASSERT(state() != VerifyingSIMPuk ||
             newState == WaitingSIMPuk ||
             newState == Initializing2 ||
             newState == FailureReset);

    Q_ASSERT(state() != VerifyingSIMPin ||
             newState == WaitingSIMPin ||
             newState == WaitingSIMPuk ||
             newState == Initializing2 ||
             newState == FailureReset);

    Q_ASSERT(state() != Initializing2 ||
             newState == Ready ||
             newState == Initializing2 ||
             newState == AerialOff ||
             newState == FailureReset);

    Q_ASSERT(state() != SIMMissing ||
             newState == SIMMissing ||
             newState == Initializing ||
             newState == FailureReset);

    Q_ASSERT(state() != SIMDead ||
             newState == FailureReset);

    Q_ASSERT(state() != AerialOff ||
             newState == Ready ||
             newState == FailureReset);

    Q_ASSERT(state() != Ready ||
             newState == WaitingSIMPuk ||
             newState == Initializing2 ||
             newState == AerialOff ||
             newState == FailureReset);

    State oldState = d->m_state;
    d->m_state = newState;
    d->m_status->setAttribute("ModemStatus", stateToString(state()));
    emit stateChanged(newState, oldState);
}

/*!
    \reimp
*/
bool CellModemManager::supportsPresence() const
{
    return false;
}

/*!
    \reimp
*/
bool CellModemManager::doDnd()
{
    return false;
}

/*!
    \reimp
*/
void CellModemManager::updateOnThePhonePresence( bool /*isOnThePhone*/ )
{
    //nothing to do
}

QTOPIA_TASK(CellModem, CellModemManager);
QTOPIA_TASK_PROVIDES(CellModem, QAbstractCallPolicyManager);

