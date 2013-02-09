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
#include "qtopiaserverapplication.h"
#include <qtopiaipcenvelope.h>
#include <qtopiaservices.h>
#include <qsoftmenubar.h>
#include <qtopianamespace.h>
#include <qtopiaservices.h>
#include <qspeeddial.h>
#include <qtopialog.h>
#include <qtopianamespace.h>
#include <qdrmcontent.h>
#include <custom.h>
#include <QExportedBackground>
#ifndef QTOPIA_ENABLE_EXPORTED_BACKGROUNDS
#include <QGlobalPixmapCache>
#endif
#include <qlabel.h>
#include <qlayout.h>
#include <qevent.h>
#include <qpixmapcache.h>
#include <QTimer>
#include <QDesktopWidget>
#include <QDebug>
#include <QMenu>
#include <QScreenInformation>

#include "phonelauncher.h"
#include "windowmanagement.h"

#include "qtopiainputevents.h"
#include "uifactory.h"

#include "qabstractbrowserscreen.h"
#include "qabstractcontextlabel.h"
#include "qabstractheader.h"
#include "qabstracthomescreen.h"
#include "qabstractsecondarydisplay.h"
#include "qabstracttaskmanager.h"

#ifdef QTOPIA_TELEPHONY
#include "qabstractcallpolicymanager.h"
#include "qabstractcallhistory.h"
#include "qabstractdialerscreen.h"
#include "dialercontrol.h"
#include "qabstractcallscreen.h"
#include "dialproxy.h"
#include "abstractaudiohandler.h"
#endif


static const int NotificationVisualTimeout = 0;  // e.g. New message arrived, 0 == No timeout
static const int WarningTimeout = 5000;  // e.g. Cannot call

class RejectDlg : public QDialog
{
public:
    void rejectDlg() { reject(); }
    void hideDlg() { hide(); }
};


/*!
    \class PhoneLauncher
    \inpublicgroup QtUiModule
    \brief The PhoneLauncher class implments the main UI widget for the Qt Extended.
    \ingroup QtopiaServer::PhoneUI

    This class is a Qt Extended \l{QtopiaServerApplication#qt-extended-server-widgets}{server widget}.
    It is part of the Qt Extended server and cannot be used by other Qt Extended applications.

    \sa QAbstractServerInterface
*/

/*!
    \internal
*/
PhoneLauncher::PhoneLauncher(QWidget *parent, Qt::WFlags fl)
    : QAbstractServerInterface(parent, fl), updateTid(0), m_header(0),
        m_context(0), stack(0), m_homeScreen(0),
#ifdef QTOPIA_TELEPHONY
    activeCalls(0),
    mCallScreen(0), m_dialer(0),
#endif
    secondDisplay(0),
#ifdef QTOPIA_TELEPHONY
    mCallHistory(0),
    missedMsgBox(0),
    alertedMissed(0),
#endif
    speeddialfeedback(0)
#ifdef QTOPIA_TELEPHONY
    , dialerSpeedDialFeedbackActive(false)
#endif
{
    QDesktopWidget *desktop = QApplication::desktop();
    QRect desktopRect = desktop->screenGeometry(desktop->primaryScreen());
#ifdef QTOPIA_ENABLE_EXPORTED_BACKGROUNDS
    QSize desktopSize = QExportedBackground::exportedBackgroundSize(desktop->primaryScreen());
    QExportedBackground::initExportedBackground(desktopSize.width(),
                                                desktopSize.height(),
                                                desktop->primaryScreen());
#else
    {
        //Needed to initialise cache!
        QPixmap pm;
        if(!QGlobalPixmapCache::find("qtopia", pm)) {
            pm.load("qtopia.png");
            QGlobalPixmapCache::insert("qtopia", pm);
        }
    }

#endif

#ifndef QT_NO_TRANSLATION //load translation for profile names
    QtopiaApplication::loadTranslations("QtopiaDefaults");
#endif
#ifdef QTOPIA_HOMEUI
    setWindowState((windowState() & ~(Qt::WindowMinimized | Qt::WindowFullScreen))
                    | Qt::WindowMaximized);
#else
    setGeometry(desktopRect);
#endif

    QObject* ctrl = QtopiaServerApplication::qtopiaTask("ThemeControl");
    //ThemeControl *ctrl = qtopiaTask<ThemeControl>();
    if (ctrl)
        QObject::connect(ctrl, SIGNAL(themeChanged()),
                        this, SLOT(loadTheme()));
    else
        qLog(Component) << "PhoneLauncher: ThemeControl not available, themes will not work";

    // Create phone header
    header();

    // Create phone context bar
    createContext();

#ifdef QTOPIA_TELEPHONY
    DialProxy *dialerServiceProxy = qtopiaTask<DialProxy>();
#   ifdef QTOPIA_HOMEUI
    if (dialerServiceProxy)
        dialerServiceProxy->setCallScreenTriggers(DialProxy::CallDialing | DialProxy::CallAccepted);
#   endif
#endif

    // Create home screen
    QVBoxLayout *layout = new QVBoxLayout(this);
    m_homeScreen = qtopiaWidget<QAbstractHomeScreen>(this);
    if (!m_homeScreen)
        qLog(Component) << "Phone Launcher: No homescreen available";
    else
        setObjectName("HomeScreen");
    // Homescreen covers the entire screen
    layout->addWidget( m_homeScreen );
    layout->setSpacing(0);
    layout->setMargin(0);

    QObject::connect(m_homeScreen, SIGNAL(speedDial(QString)),
                    this, SLOT(activateSpeedDial(QString)));
    QObject::connect(m_homeScreen, SIGNAL(showPhoneBrowser()),
                    this, SLOT(showPhoneLauncher()));
    QObject::connect(m_homeScreen, SIGNAL(lockStateChanged(bool)),
                    this, SLOT(keyStateChanged(bool)));
#ifdef QTOPIA_TELEPHONY
    QObject::connect(m_homeScreen, SIGNAL(showCallScreen()),
                    this, SLOT(showCallScreen()));
    QObject::connect(m_homeScreen, SIGNAL(showMissedCalls()),
                    this, SLOT(showMissedCalls()));
    QObject::connect(m_homeScreen, SIGNAL(showCallHistory()),
                    this, SLOT(showCallHistory()));
    QObject::connect(m_homeScreen, SIGNAL(dialNumber(QString)),
                    this, SLOT(showSpeedDialer(QString)));
    QObject::connect(m_homeScreen, SIGNAL(hangupCall()),
                    this, SLOT(hangupPressed()));

    //Connect dial string processing/dialer service
    if (dialerServiceProxy) {
        QObject::connect(m_homeScreen, SIGNAL(callEmergency(QString)),
                dialerServiceProxy, SLOT(requestDial(QString)));
        QObject::connect(dialerServiceProxy, SIGNAL(doShowDialer(QString)),
                this, SLOT(showDialer(QString)));
        QObject::connect(dialerServiceProxy, SIGNAL(showCallScreen()),
                this, SLOT(showCallScreen()));
        QObject::connect(dialerServiceProxy, SIGNAL(resetScreen()),
                this, SLOT(resetView()));
    }
#endif

    // Create secondary display
    secondaryDisplay();

    // Create TaskManager
    QAbstractTaskManager *taskManager = qtopiaWidget<QAbstractTaskManager>();
    if (taskManager) {
        connect(taskManager, SIGNAL(multitaskRequested()),
            this, SLOT(multitaskPressed()));
        connect(taskManager, SIGNAL(showRunningTasks()),
            this, SLOT(showRunningTasks()));
    }

    // Listen to system channel
    QtopiaChannel* sysChannel = new QtopiaChannel( "QPE/System", this );
    connect(sysChannel, SIGNAL(received(QString,QByteArray)),
            this, SLOT(sysMessage(QString,QByteArray)) );

    phoneBrowser()->resetToView("Main"); // better to get initial icon load cost now, rather then when user clicks.

    showHomeScreen(0);

#ifdef QTOPIA_TELEPHONY
    connect(DialerControl::instance(), SIGNAL(missedCount(int)),
            this, SLOT(missedCount(int)));
    connect(DialerControl::instance(), SIGNAL(activeCount(int)),
            this, SLOT(activeCallCount(int)));
    connect(DialerControl::instance(), SIGNAL(stateChanged()),
            this, SLOT(stateChanged()));
    connect(DialerControl::instance(), SIGNAL(callDropped(QPhoneCall)),
            this, SLOT(resetDialer()));

    // Don't alert user until count changes.
    alertedMissed = DialerControl::instance()->missedCallCount();
#endif
    loadTheme();
}

/*!
    \internal
*/
PhoneLauncher::~PhoneLauncher()
{
    delete stack;
    delete m_header;
    delete m_context;
#ifdef QTOPIA_TELEPHONY
    delete mCallScreen;
    if(mCallHistory)
        delete mCallHistory;
    if( m_dialer )
        delete m_dialer;
    delete speeddialfeedback;
#endif
    if ( secondDisplay )
        delete secondDisplay;
}

/*!
    \internal
*/
void PhoneLauncher::showEvent(QShowEvent *e)
{
    QTimer::singleShot(0, m_homeScreen, SLOT(show()));
#ifdef QTOPIA_TELEPHONY
    QTimer::singleShot(0, this, SLOT(initializeCallHistory()));
    QTimer::singleShot(0, this, SLOT(initializeDialerScreen()));
#endif
    if(m_header) {
        m_header->raise();
        m_header->show();
    }

    if (m_context && QSoftMenuBar::keys().count()) {
        m_context->raise();
        m_context->show();
    }
    QWidget::showEvent(e);
}

/*!
    \internal
*/
void PhoneLauncher::callPressed()
{
#ifdef QTOPIA_TELEPHONY
    // Called if server windows are not on top
    showCallHistory();
#endif
}


void PhoneLauncher::resetView()
{
    raiseMe();
    QtopiaIpcEnvelope e("QPE/System","close()");
    hideAll();
    showHomeScreen(0);
}

/*!
    \internal
*/
void PhoneLauncher::hangupPressed()
{
    // Called if server windows are not on top
    bool showHomeScreen = true;
#ifdef QTOPIA_TELEPHONY
    DialProxy *dialerServiceProxy = qtopiaTask<DialProxy>();
    if (dialerServiceProxy)
        showHomeScreen = !dialerServiceProxy->hangupPressed();
#endif

    if (showHomeScreen)
        resetView();
}

/*!
    \internal
*/
void PhoneLauncher::multitaskPressed()
{
    // When pressed, home screen is shown (without quiting apps).
    // When pressed again soon after (X msec), next running app is shown.

    QStringList runningApps = appMon.runningApplications();

    QStringList sortedapps = runningApps;
    sortedapps.removeAll("sipagent"); // workaround
    sortedapps.sort();

    int to_run=-1;
    if ( m_homeScreen && m_homeScreen->isActiveWindow() ) {
        // first application
        if ( sortedapps.count() == 0 ) {
            showRunningTasks(); // (gives error message)
            return;
        }
        to_run=multitaskingcursor=0;
    } else if ( multitaskingMultipressTimer.isActive() ) {
        // next application...
        if ( ++multitaskingcursor >= sortedapps.count() ) {
            to_run = -1;
        } else {
            to_run=multitaskingcursor;
        }
    }
    if ( to_run < 0 ) {
        multitaskingcursor=0;
        showHomeScreen(0);
    } else {
        QStringList sortedapps = runningApps;
        sortedapps.sort();
        QContent app(sortedapps[to_run], false);
        multitaskingMultipressTimer.start(2500,this);
        if ( app.isValid() )
            app.execute();
    }
}

/*!
    \internal
*/
void PhoneLauncher::showRunningTasks()
{
    // HACK: force backlight on - it now sometimes fails on GTA04 after
    // suspend.
    QtopiaServiceRequest e("QtopiaPowerManager", "setBacklight(int)");
    e << -3;                // Force on
    e.send();
    
    // XXX Should:
    // XXX  - Go above StaysOnTop windows (eg. menu popups, QCalendarWidget popups)

    //callhistory & callscreen must not close as they count as applications and must
    //appear in the task manager
#ifdef QTOPIA_TELEPHONY
    if (dialer(false))
        dialer(false)->close();
#endif

    QAbstractTaskManager *tm = qtopiaWidget<QAbstractTaskManager>();
    if (tm) {
        tm->showMaximized();
        tm->raise();
        tm->activateWindow();
    }
}

/*!
    \internal
*/
void PhoneLauncher::showContentSet()
{
    if ( !UIFactory::isAvailable("ContentSetLauncherView") )
        return;
    hideAll();
    phoneBrowser()->moveToView("Folder/ContentSet");
    phoneBrowser()->showMaximized();
    phoneBrowser()->raise();
    phoneBrowser()->activateWindow();
}

/*!
    \internal
*/
void PhoneLauncher::loadTheme()
{
    qLog(UI) << "Load theme";
    if (m_homeScreen) m_homeScreen->hide();
    layoutViews();
    initInfo();
}

/*!
    \internal
*/
void PhoneLauncher::layoutViews()
{
    bool v = isVisible();

    QDesktopWidget *desktop = QApplication::desktop();
    QRect desktopRect = desktop->screenGeometry(desktop->primaryScreen());

    QSize titleSize = m_header ? m_header->reservedSize() : QSize(0,0);
    QSize contextSize = m_context ? m_context->reservedSize() : QSize(0,0);

    // header - not lazy
    if (m_header) {
        WindowManagement::dockWindow(m_header, WindowManagement::Top, titleSize);
        if (v)
            m_header->show();
    }
    // context bar - not lazy
    if (m_context) {
        WindowManagement::dockWindow(m_context, 
#ifdef QTOPIA_HOMEUI_WIDE
    WindowManagement::Left
#else
    WindowManagement::Bottom 
#endif

    ,contextSize);
        if (v && QSoftMenuBar::keys().count())
            m_context->show();
    }

#ifdef QTOPIA_TELEPHONY
    // force call screen to be created immediately, because lazily creating it
    // when a call comes in will result in poor user perception of performance.
    QAbstractCallScreen *cs = callScreen(true);
    if (cs) {
        cs->setGeometry(desktopRect.x(), desktopRect.y() + titleSize.height(),
                        desktopRect.width(),
                        desktopRect.height() - titleSize.height() - contextSize.height() );
    }
#endif

    // update position of launcher stack - lazy
    if (phoneBrowser(false)) {
        if (!phoneBrowser()->isHidden())
            phoneBrowser()->showMaximized();
        else
            phoneBrowser()->setGeometry(desktopRect.x(), desktopRect.y() + titleSize.height(),
                                        desktopRect.width(),
                                        desktopRect.height() - titleSize.height() - contextSize.height());
    }
}

#ifdef QTOPIA_TELEPHONY
/*!
    Displays the call history window. The missed calls tab will be on top.
*/
void PhoneLauncher::showMissedCalls()
{
    showCallHistory(true);
}

/*!
    Displays the call screen.
*/
void PhoneLauncher::showCallScreen()
{
    if ( !callScreen() ) {
        qLog(Component) << "Missing callscreen component -> cannot show callscreen";
        return;
    }
    callScreen()->showMaximized();
    callScreen()->raise();
    callScreen()->activateWindow();
}
#endif

/*!
    \internal
*/
void PhoneLauncher::initInfo()
{
    update();

    timerEvent(0);

#ifdef QTOPIA_TELEPHONY
    int missCalls = DialerControl::instance()->missedCallCount();
    if ( missCalls != 0 )
        missedCount(missCalls);
#endif
}

/*!
    \internal
*/
void PhoneLauncher::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    layoutViews();
}

/*!
    \internal
*/
void PhoneLauncher::sysMessage(const QString& message, const QByteArray &data)
{
    QDataStream stream( data );
    if ( message == "showHomeScreen()" ) {
        hideAll();
        showHomeScreen(0);
#ifdef QTOPIA_HOMEUI
        setObjectName("HomeScreen");
        setWindowTitle("HomeScreen");
    } else if ( message == QLatin1String("setPhoneMode()")) {
        hideAll();
        showHomeScreen(0);
        setObjectName("HomeScreen_Phone");
        setWindowTitle("Phone");
    } else if ( message == QLatin1String("showContextBar()")) {
        if (m_context)
            m_context->raise();
#endif
#ifdef QTOPIA_TELEPHONY
    } else if ( message == QLatin1String("showCallScreen()")) {
        showCallScreen();
#endif
    } else if ( message == QLatin1String("showPhoneLauncher()")) {
        showPhoneLauncher();
    } else if ( message == QLatin1String("showHomeScreenAndToggleKeylock()") ) {
        showHomeScreen(2);
    } else if ( message == QLatin1String("showHomeScreenAndKeylock()") ) {
        showHomeScreen(3);
    } else if ( message == "serverKey(int,int)" ) {
        int key,press;
        stream >> key >> press;
        if ( key == Qt::Key_Call && press ) {
            callPressed();
        } else if ( (key == Qt::Key_Hangup || key == Qt::Key_Flip) && press ) {
            hangupPressed();
        }
    } else if ( message == "showContentSetView()" ) {
        showContentSet();
    }
}

/*!
    \internal
*/
void PhoneLauncher::raiseMe()
{
    topLevelWidget()->raise();
    if (m_header)
        QTimer::singleShot(0, m_header, SLOT(raise()));
    if (m_context && QSoftMenuBar::keys().count())
        QTimer::singleShot(0, m_context, SLOT(raise()));
}

/*!
    \internal
*/
void PhoneLauncher::showHomeScreen(int state)
{
    // state: 0 -> no screensaver calls
    //        1 -> showHomeScreen called by screensaver
    //        2 -> showHomeScreen called when lock key is toggled
    //        3 -> showHomeScreen called to key lock

    if (!m_homeScreen)
        return;
#ifdef QTOPIA_TELEPHONY
    if (state != 0 && activeCalls) {
        return;
    }
#endif

    bool hsWasLocked = m_homeScreen->locked();

    if (m_context)
        m_context->raise();
    rejectModalDialog();
    m_homeScreen->raise();
    m_homeScreen->show();
    topLevelWidget()->raise();
    m_homeScreen->setFocus();
    topLevelWidget()->activateWindow();

    if (phoneBrowser(false))
        phoneBrowser()->hide();

    if (state == 1) {
        if (!Qtopia::mousePreferred()) {
            QSettings c("Trolltech","qpe");
            c.beginGroup("HomeScreen");
            QString lockType = c.value( "AutoKeyLock", "Disabled" ).toString();
            if (lockType == "Enabled")
                m_homeScreen->setLocked(true);
        }
    } else if (state == 2) {
        //Touchscreen uses a modal dialog to block screen
        //the dlg was just rejected (see above) hence we need to
        //check the previous state.
        if (Qtopia::mousePreferred())
            m_homeScreen->setLocked(!hsWasLocked);
        else
            m_homeScreen->setLocked(!m_homeScreen->locked());
    } else if (state == 3) {
        if (!m_homeScreen->locked())
            m_homeScreen->setLocked(true);
    }
}

/*!
    \internal
*/
void PhoneLauncher::rejectModalDialog()
{
    // Last resort.  We shouldn't have modal dialogs in the server, but
    // just in case we need to get rid of them when a call arrives.  This
    // is a bad thing to do, but far less dangerous than missing a call.
    // XXX Known modals:
    //  - category edit dialog
    QWidgetList list = QApplication::topLevelWidgets();
    QList<QPointer<RejectDlg> > dlgsToDelete;

    foreach(QWidget *w, list)
        if (w->isVisible() && w->inherits("QDialog"))
            dlgsToDelete.append((RejectDlg*)w);

    foreach(QPointer<RejectDlg> d, dlgsToDelete) {
        if (!d)
            continue;

        if (d->testAttribute(Qt::WA_ShowModal)) {
            qWarning("Rejecting modal dialog: %s", d->metaObject()->className());
            d->rejectDlg();
        } else {
            qWarning("Hiding non-modal dialog: %s", d->metaObject()->className());
            d->hideDlg();
        }
    }
}

/*!
    \internal
*/
void PhoneLauncher::showPhoneLauncher()
{
    phoneBrowser()->resetToView("Main");

    phoneBrowser()->showMaximized();
    phoneBrowser()->raise();
    phoneBrowser()->activateWindow();
}

#ifdef QTOPIA_TELEPHONY
/*!
    \internal
*/
void PhoneLauncher::missedCount(int /*count*/)
{
    if(m_homeScreen && m_homeScreen->locked()
#ifdef QTOPIA_CELL
        || m_homeScreen->simLocked()
#endif
    ){
        // do not show alert this time
        // but reset missed count so when the phone is unlocked the alert is shown.
        resetMissedCalls();
    } else {
        showAlertDialogs();
    }
}

/*!
    \internal
*/
void PhoneLauncher::activeCallCount(int count)
{
    activeCalls = count;
}

#endif

/*!
    \internal
*/
void PhoneLauncher::timerEvent(QTimerEvent * tev)
{
    if(tev && tev->timerId() == updateTid) {
        update();
        killTimer(updateTid);
        updateTid = 0;
        return;
    }

    if ( tev && multitaskingMultipressTimer.timerId() == tev->timerId() ) {
        multitaskingMultipressTimer.stop();
        return;
    }
}

#ifdef QTOPIA_TELEPHONY
/*!
    \internal
    Shows the Missed Calls dialog if there are any missed calls.
*/
void PhoneLauncher::showAlertDialogs()
{
    if (isVisible()) {
        if(DialerControl::instance()->missedCallCount() &&
                DialerControl::instance()->missedCallCount() != alertedMissed) {
            alertedMissed = DialerControl::instance()->missedCallCount();
            if (!missedMsgBox) {
                QString missedMsg = tr("Do you wish to view missed calls?");
                missedMsgBox = QAbstractMessageBox::messageBox(m_homeScreen, tr("Missed Call"),
                        "<qt>"+missedMsg+"</qt>", QAbstractMessageBox::Information,
                        QAbstractMessageBox::Yes, QAbstractMessageBox::No);
                connect(missedMsgBox, SIGNAL(finished(int)), this, SLOT(messageBoxDone(int)));
            }
            missedMsgBox->setTimeout(NotificationVisualTimeout, QAbstractMessageBox::No);
            QtopiaApplication::showDialog(missedMsgBox);
        }
    }
}
#endif

/*!
    \internal
*/
void PhoneLauncher::keyStateChanged(bool locked)
{
#ifdef QTOPIA_TELEPHONY
    if(!locked)
        showAlertDialogs();
#else
    Q_UNUSED(locked);
#endif
}

// Creation methods
/*!
    \internal
*/
QAbstractHeader *PhoneLauncher::header()
{
    if(!m_header) {
        m_header = qtopiaWidget<QAbstractHeader>();
        if(!m_header) {
            qLog(Component) << "Phone Launcher: QAbstractHeader not available";
        } else {
            WindowManagement::protectWindow(m_header);
        }
    }
    static bool initializeIM = true;
    // instantiate InputMethods (usually belong to Phoneheader, but no
    // phoneheader on some devices such as deskphones).
    if ( initializeIM ) {
        initializeIM = false;
        if ( !m_header ) {
            QWidget *imw = UIFactory::createWidget( "InputMethods", this );
            if ( !imw )
                qLog(Component) << "PhoneLauncher: Cannot load Inputmethod component";
        }
    }
    return m_header;
}

/*!
    \internal
*/
void PhoneLauncher::createContext()
{
    if(!m_context) {
        m_context = qtopiaWidget<QAbstractContextLabel>();
        if(!m_context) {
            qLog(Component) << "Phone Launcher: QAbstractContextLabel not available";
        } else {
            WindowManagement::protectWindow(m_context);
        }
    }
}


/*!
    \internal
*/
QAbstractContextLabel *PhoneLauncher::context()
{
    return m_context;
}

#ifdef QTOPIA_TELEPHONY
/*!
    The speed dialer is displayed, preloaded with \a digits, when this slot is invoked.
*/
void PhoneLauncher::showSpeedDialer(const QString &digits)
{
    showDialer(digits, true);
}

/*!
    Displays the dialer, preloaded with \a digits.  If \a speedDial
    is true, the digits will be considered for use in speed dial (press and
    hold).
*/
void PhoneLauncher::showDialer(const QString &digits, bool speedDial)
{
    if (!dialer())
        return;

    if(speedDial) {
        dialer()->reset();
        dialer()->appendDigits(digits);
    } else {
        dialer()->setDigits(digits);
    }

    dialer()->showMaximized();
    dialer()->raise();
    dialer()->activateWindow();
}

/*!
    \internal
    Resets the audio handlers.
*/
void PhoneLauncher::resetDialer()
{
    dialer()->reset();
}

/*!
    \fn QPointer<QAbstractCallhistory> PhoneLauncher::callHistory() const

    Returns the associated call history or 0 if it hasn't been instanciated yet.
*/

/*!
    Displays the call history window.  If \a missed is true, the missed
    calls tab will be on top.
*/
void PhoneLauncher::showCallHistory(bool missed)
{
    if ( !callHistory() )
        initializeCallHistory();
    if( callHistory() ) {
        callHistory()->reset();
        if (missed || DialerControl::instance()->missedCallCount() > 0)
            callHistory()->showMissedCalls();

        callHistory()->refresh();

        if( !callHistory()->isHidden() )
        {
            callHistory()->raise();
        }
        else
        {
            callHistory()->showMaximized();
        }
    }
    callHistory()->activateWindow();
    callHistory()->raise();
}

/*!
    \internal
    Initialize call history window.
    This delayed initialization will shorten the first launch time.
*/
void PhoneLauncher::initializeCallHistory()
{
    if ( !mCallHistory ) {
        mCallHistory = qtopiaWidget<QAbstractCallHistory>();
        if( !mCallHistory) {
            qLog(Component) << "Phone Launcher: No Call History Available";
        } else {
            connect(callHistory(), SIGNAL(viewedMissedCalls()),
                this, SLOT(resetMissedCalls()) );
            connect(callHistory(), SIGNAL(viewedMissedCalls()),
                DialerControl::instance(), SLOT(resetMissedCalls()) );

            DialProxy *dialerServiceProxy = qtopiaTask<DialProxy>();
            if (dialerServiceProxy)
                connect(callHistory(), SIGNAL(requestedDial(QString,QUniqueId)),
                    dialerServiceProxy, SLOT(requestDial(QString,QUniqueId)));
        }
    }
}

/*!
    \internal
    Initialize dialer screen.
*/
void PhoneLauncher::initializeDialerScreen() const
{
    m_dialer = qtopiaWidget<QAbstractDialerScreen>(0);
    if (!m_dialer) {
        qLog(UI) << "Unable to create the Dialer Screen";
        return;
    }
    m_dialer->move(QApplication::desktop()->screenGeometry().topLeft());

    DialProxy *dialerServiceProxy = qtopiaTask<DialProxy>();
    if (dialerServiceProxy) {
        connect(m_dialer, SIGNAL(requestDial(QString,QUniqueId)),
                dialerServiceProxy, SLOT(requestDial(QString,QUniqueId)));
        connect(dialerServiceProxy, SIGNAL(onHookGesture()),
                m_dialer, SLOT(doOnHook()));
        connect(dialerServiceProxy, SIGNAL(offHookGesture()),
                m_dialer, SLOT(doOffHook()));
    }

    connect(m_dialer, SIGNAL(speedDial(QString)),
            this, SLOT(speedDial(QString)) );

}

#endif

/*!
    \internal
    Hides the Call History, Dialer and Call Screen.
*/
void PhoneLauncher::hideAll()
{
#ifdef QTOPIA_TELEPHONY
    if (callScreen(false))
        callScreen(false)->close();
    if (callHistory())
        callHistory()->close();
    if (dialer(false))
        dialer(false)->close();
#endif
}

/*!
    Returns the associated homescreen or 0 if it hasn't been instanciated yet.
*/
QAbstractHomeScreen *PhoneLauncher::homeScreen() const
{
    return m_homeScreen;
}

#ifdef QTOPIA_TELEPHONY
/*!
    \internal
*/
void PhoneLauncher::stateChanged()
{
    callScreen()->stateChanged(); // We must know that this is updated before we continue
}

/*!
    \internal
    Accepts an incoming call.  Has no affect if an incoming call is not
    currently available.
*/
void PhoneLauncher::messageBoxDone(int r)
{
    QAbstractMessageBox *box = (QAbstractMessageBox*)sender();
    if (box == missedMsgBox && r == QAbstractMessageBox::Yes) {
        showCallHistory(true);
    }
}

/*!
    \internal
*/
void PhoneLauncher::resetMissedCalls()
{
    alertedMissed = 0;
}

/*!
    Returns the associated dialer screen or 0 if it hasn't been instanciated yet.

    If \a create is true, the call screen will be created if it doesn't exist yet.
    This allows the delayed initialization of subcomponents.
*/
QAbstractDialerScreen *PhoneLauncher::dialer(bool create) const
{
    if(create && !m_dialer)
        initializeDialerScreen();

    return m_dialer;
}

/*!
    Returns the associated call screen or 0 if it hasn't been instanciated yet.

    If \a create is true, the call screen will be created if it doesn't exist yet.
    This allows the delayed initialization of subcomponents.
*/
QAbstractCallScreen *PhoneLauncher::callScreen(bool create) const
{
    if(create && !mCallScreen) {
        mCallScreen = qtopiaWidget<QAbstractCallScreen>(0);
        if (!mCallScreen) {
            qLog(Component) << "PhoneLauncher: Callscreen not available";
            return 0;
        }
        mCallScreen->move(QApplication::desktop()->screenGeometry().topLeft());
        QObject::connect(mCallScreen, SIGNAL(hangupCall()),
                this, SLOT(hangupPressed()));
        DialProxy *dialerServiceProxy = qtopiaTask<DialProxy>();
        if (dialerServiceProxy)
            connect(mCallScreen, SIGNAL(acceptIncoming()),
                    dialerServiceProxy, SLOT(acceptIncoming()));
    }
    return mCallScreen;

}

#endif

void PhoneLauncher::speedDialActivated()
{
#ifdef QTOPIA_TELEPHONY
    if (dialerSpeedDialFeedbackActive && dialer(false)) {
        dialer(false)->reset();
        dialer(false)->hide();
    }
#endif
    dialerSpeedDialFeedbackActive = false;
}


/*!
    \internal
*/
void PhoneLauncher::speedDial( const QString& input )
{
    if (activateSpeedDial(input)) {
        dialerSpeedDialFeedbackActive = true;
    }
}

/*!
    Activates speed dial associated with \a input and returns true if a matching
    speeddial action was found.
*/
bool PhoneLauncher::activateSpeedDial( const QString& input )
{
    if ( !speeddialfeedback ) {
        speeddialfeedback = new QSpeedDialFeedback;
        connect(speeddialfeedback, SIGNAL(requestSent()), this, SLOT(speedDialActivated()));
    }

    QString sel = input;
    if ( input.isEmpty() ) {
        speeddialfeedback->setBlindFeedback(false);
        sel = QSpeedDial::selectWithDialog(this);
    } else {
        speeddialfeedback->setBlindFeedback(true);
    }

    QDesktopWidget *desktop = QApplication::desktop();
    QtopiaServiceDescription* r = QSpeedDial::find(sel);
    if(r)
    {
        speeddialfeedback->show(desktop->screen(desktop->primaryScreen()),sel,*r);
        return !r->request().isNull();
    }
    else
    {
        if (!input.isEmpty())
            speeddialfeedback->show(desktop->screen(desktop->primaryScreen()),sel,QtopiaServiceDescription());
        return false;
    }
}


/*!
    Returns the associated browser screen or 0 if it hasn't been instanciated yet.

    If \a create is true the browser screen will be created if it doesn't exist yet.
    This allows the delayed initialization of subcomponents.
*/
QAbstractBrowserScreen *PhoneLauncher::phoneBrowser(bool create) const
{
    if(!stack && create) {
        stack = qtopiaWidget<QAbstractBrowserScreen>();
        if (stack)
            stack->move(QApplication::desktop()->screenGeometry().topLeft());
        else
            qFatal("Phone launcher requires the presence of a browser screen");
    }

    return stack;
}

/*!
    Returns the associated secondary display or 0 if it hasn't been instanciated yet.

    If \a create is true the secondary display will be created if it doesn't exist yet
    and if the device supports a secondary display. The \a create parameter allows the 
    delayed initialization of subcomponents.
*/
QAbstractSecondaryDisplay *PhoneLauncher::secondaryDisplay(bool create) const
{
    QDesktopWidget *desktop = QApplication::desktop();
    if (!secondDisplay && create && desktop->numScreens() > 1) {
        // Search for a normal (i.e. non-television) screen to create
        // the secondary display.
        int secondScreen = -1;
        for (int screen = 0; screen < desktop->numScreens(); ++screen) {
            QScreenInformation info(screen);
            if (screen != desktop->primaryScreen() &&
                info.type() == QScreenInformation::Normal) {
                secondScreen = screen;
                break;
            }
        }
        if (secondScreen != -1) {
            secondDisplay = qtopiaWidget<QAbstractSecondaryDisplay>(0,
                                                Qt::FramelessWindowHint | Qt::Tool);
            if (secondDisplay){
                secondDisplay->setGeometry(desktop->screenGeometry(secondScreen));
                secondDisplay->show();
            }else{
                qLog(UI) << "Unable to create the Secondary Display";
            }
        }
    }

    return secondDisplay;
}

// define QSpeedDialFeedback
QSpeedDialFeedback::QSpeedDialFeedback() :
    QFrame(0, (Qt::Tool | Qt::FramelessWindowHint)),
    timerId(0), blind(true)
{
    setFrameStyle(QFrame::WinPanel|QFrame::Raised);
    QVBoxLayout *vb = new QVBoxLayout(this);
    vb->setMargin(8);
    icon = new QLabel(this);
    vb->addWidget(icon);
    label = new QLabel(this);
    label->setWordWrap(true);
    vb->addWidget(label);
    icon->setAlignment(Qt::AlignCenter);
    label->setAlignment(Qt::AlignCenter);
}

// is user "dialing-blind", or picking from screen?
void QSpeedDialFeedback::setBlindFeedback(bool on)
{
    blind = on;
}

void QSpeedDialFeedback::show(QWidget* center, const QString& input, const QtopiaServiceDescription& r)
{
    req = r.request();
#ifdef QTOPIA_TELEPHONY
    if ( blind ) {
        QtopiaServiceRequest sRequest("Ringtone", "startRingtone(QString)");
        if ( req.isNull() ) {
            sRequest << QString(":sound/speeddial/nak");
        } else {
            sRequest << QString(":sound/speeddial/ack");
        }
        sRequest.send();
    }
#endif
    if ( req.isNull() ) {
        QIcon p(":icon/cancel");
        icon->setPixmap(p.pixmap(style()->pixelMetric(QStyle::PM_LargeIconSize)));
        label->setText(tr("No speed dial %1").arg(input));
    } else {
        // Often times these are actually icons, not images...
        QIcon p(":icon/" + r.iconName());
        if (p.isNull())
            p = QIcon(":image/" + r.iconName());
        icon->setPixmap(p.pixmap(style()->pixelMetric(QStyle::PM_LargeIconSize)));
        label->setText(r.label());
    }
    QtopiaApplication::sendPostedEvents(this, QEvent::LayoutRequest);
    QRect w = center->topLevelWidget()->geometry();
    // Make sure the label wraps (and leave a small amount around the edges)
    // (which is 2xlayout margin + a little bit)
    int labelmargins = layout()->margin() * 2 + layout()->spacing() * 2;
    label->setMaximumSize(w.width() - labelmargins, w.height() - labelmargins);
    QSize sh = sizeHint();
    //We have to set the minimumsize before we change the geometry
    //because setGeometry is based on it and for some weired reason
    //Minimumsize is set to the size of the previous geometry.
    //This is a problem when changing from a bigger to a smaller geometry
    //and seems to be a bug in Qt.
    setMinimumSize(sh.width(),sh.height());
    setGeometry(w.x()+(w.width()-sh.width())/2,
                w.y()+(w.height()-sh.height())/2,sh.width(),sh.height());
    QFrame::show();
    activateWindow();
    setFocus();
    if( Qtopia::mousePreferred() || !blind ) {
        if ( !req.isNull() ) {
            req.send();
            emit requestSent();
        }
        timerId = startTimer(1000);
    }
}

void QSpeedDialFeedback::timerEvent(QTimerEvent*)
{
    killTimer(timerId);
    close();
}

void QSpeedDialFeedback::keyReleaseEvent(QKeyEvent* ke)
{
    if ( !ke->isAutoRepeat() ) {
        if ( !req.isNull() ) {
            req.send();
            emit requestSent();
        }
        close();
    }
}

void QSpeedDialFeedback::mouseReleaseEvent(QMouseEvent*)
{
    if ( !req.isNull() ) {
        req.send();
        emit requestSent();
    }
    close();
}

QTOPIA_REPLACE_WIDGET(QAbstractServerInterface, PhoneLauncher);

#include "phonelauncher.moc"
