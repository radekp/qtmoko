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

#include "applicationlauncher.h"
#include "callscreen.h"
#include "dialercontrol.h"
#include "servercontactmodel.h"
#include "qtopiaserverapplication.h"
#include "qabstractmessagebox.h"
#include "qtopiainputevents.h"
#include "themecontrol.h"
#include "uifactory.h"
#include "abstractaudiohandler.h"
#include "callaudiohandler.h"
#include "abstractdialfilter.h"
#include "taskmanagerentry.h"
#ifdef MEDIA_SERVER
#include "videoringtone.h"
#endif
#include <QAction>
#include <QDateTime>
#include <QDebug>
#include <QDialog>
//#include <QItemDelegate>
#include <QKeyEvent>
#include <QLayout>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QPainter>
#include <QSettings>
#include <QTimer>
#include <QAbstractListModel>
#include <QDesktopWidget>
#include <QTextDocument>
#include <QList>
#include <QLabel>
#include <QScreenInformation>


#include <qtopiaservices.h>
#include <qsoftmenubar.h>
#include <qcontactmodel.h>
#include <qcontact.h>
#include <qtopianamespace.h>
#include <themedview.h>
#include <qphonecallmanager.h>

static const int  MAX_JOINED_CALLS = 5;
static const uint SECS_PER_HOUR= 3600;
static const uint SECS_PER_MIN  = 60;

#define SELECT_KEY_TIMEOUT 2000

static ThemedCallScreenView *callScreen = 0;

static QDialog *waitDlg = 0;

class CallData {
public:
    CallData() {}
    CallData(const QPhoneCall &c) : call(c), callState(c.state()), havePhoto(false) {
        // Get the number or name to display in the text area.
        numberOrName = call.number();

        QContact cnt;
        QContactModel *m = ServerContactModel::instance();
        if (!call.contact().isNull()) {
            cnt = m->contact(call.contact());
        } else if (!numberOrName.isEmpty()) {
            cnt = m->matchPhoneNumber(numberOrName);
        }

        if (!cnt.uid().isNull()) {
            numberOrName = cnt.label();
            ringTone = cnt.customField( "tone" );
            QString pf = cnt.portraitFile();
            if( pf.isEmpty() ) {
                photo = ":image/addressbook/generic-contact.svg";
                havePhoto = false;
            } else {
                photo = Qtopia::applicationFileName( "addressbook", "contactimages/" ) + cnt.portraitFile();
                havePhoto = QFile::exists(photo);
            }
        }
    }

    bool isMulti() const {
        return ((callScreen->activeCallCount() > 1 && call.state() == QPhoneCall::Connected) ||
                (callScreen->heldCallCount() > 1 && call.onHold()));
    }

    QString durationString() {
        QString duration;
        if (!call.incoming() && !call.dialing()) {
            if (!connectTime.isNull()) {
                int elapsed;
                if (disconnectTime.isNull()) {
                    elapsed = connectTime.secsTo(QDateTime::currentDateTime());
                } else {
                    elapsed = connectTime.secsTo(disconnectTime);
                }
                int hour = elapsed/SECS_PER_HOUR;
                int minute = (elapsed % SECS_PER_HOUR)/SECS_PER_MIN;
                int second = elapsed % SECS_PER_MIN;
                QString buf;
                buf.sprintf( "%.2d:%.2d:%.2d", hour, minute, second );
                duration = buf;
            }
        }

        return duration;
    }

    QPhoneCall call;
    QPhoneCall::State callState;

    // QPixmap photo;
    // QString numberOrName;
    // elapsed time
    // conference
    // type

// what else should be exported in the vs

    QDateTime dialTime;
    QDateTime connectTime;
    QDateTime disconnectTime;
    QString ringTone;
    bool havePhoto;
    QString photo;
    QString numberOrName;
    QString state;
};

//===========================================================================

class SecondaryCallScreen : public QWidget
{
    Q_OBJECT
public:
    SecondaryCallScreen(QWidget *parent=0, Qt::WindowFlags f=0);

    void setCallData(const CallData &c) {
        callData = c;
        update();
    }

    const QPhoneCall &call() const { return callData.call; }

protected:
    void paintEvent(QPaintEvent *);

private:
    CallData callData;
};

SecondaryCallScreen::SecondaryCallScreen(QWidget *parent, Qt::WindowFlags f)
    : QWidget(parent, f)
{
}

void SecondaryCallScreen::paintEvent(QPaintEvent *)
{
    QPainter p(this);

    QRect r(rect());
    if (callData.havePhoto) {
        QPixmap photo(callData.photo);
        if (!photo.isNull()) {
            p.drawPixmap(0, 0, photo);
            r.adjust(photo.width(), 0, 0, 0);
        }
    }
    QString text(callData.numberOrName);
    QString subText;
    if (call().incoming() || call().dialing())
        subText = callData.state;
    else if (call().onHold())
        subText = callData.durationString() + QLatin1String(" ") + callData.state;
    else if (call().state() == QPhoneCall::Connected && DialerControl::instance()->hasIncomingCall())
        subText = callData.durationString();
    else if( call().dropped() )
        subText = callData.state;
    else
        subText = callData.durationString();

    if (!subText.isEmpty())
        text += QLatin1String("<br><font size=-1>") + subText;
    QTextDocument doc;
    doc.setHtml("<b>"+text+"</b>");
    doc.setTextWidth(r.width());
    p.translate(r.x(), 0);
    doc.drawContents(&p);
}

//===========================================================================

/* declare CallItemListView */
class CallItemListView : public QListView
{
    Q_OBJECT
public:
    CallItemListView(ThemeWidgetItem *ti, QWidget *parent=0);
protected:
    void keyPressEvent(QKeyEvent*);
protected slots:
    void currentChanged(const QModelIndex &cur, const QModelIndex &prev);
private:
    ThemeWidgetItem *thisThemeItem;
};

//-----------------------------------------------------------
/* define CallItemEntry */
class CallItemEntry : public ThemeListModelEntry
{
public:
    CallItemEntry(DialerControl *ctrl, const QPhoneCall &c, ThemeListModel* m);

    void setText(const QString & text) { display = text; }

    virtual QString type() const {
        QItemSelectionModel * selectModel = model()->listItem()->listView()->selectionModel();
        QModelIndex index = model()->entryIndex(this);
        if (selectModel->isSelected(index) && model()->listItem()->listView()->selectionMode() != QListWidget::NoSelection)
            return "selected";
        else if( call().incoming() )
            return "incoming";
        else if( call().dialing() )
            return "outgoing";
        else if( call().onHold() )
            return "onhold";
        else if( call().state() == QPhoneCall::Connected && !callScreen->incomingCall() )
            return "active";
        else if( call().dropped() )
            return "dropped";
        else
            return "active";
    }

    QPhoneCall &call() { return callData.call; }
    const QPhoneCall &call() const { return callData.call; }

    CallData callData;
    QString display;

private:
    DialerControl *control;
};

CallItemEntry::CallItemEntry(DialerControl *ctrl, const QPhoneCall &c, ThemeListModel* model)
    : ThemeListModelEntry(model),
            callData(c),
            control(ctrl)
{
}

//-----------------------------------------------------------
/* define CallItemModel */
typedef bool(*LessThan)(const ThemeListModelEntry *left, const ThemeListModelEntry *right);
class CallItemModel : public ThemeListModel
{
    Q_OBJECT
public:
    CallItemModel( QObject *parent, ThemeListItem* item, ThemedView* view) : ThemeListModel(parent, item, view) { };
    virtual ~CallItemModel() {}

    CallItemEntry* callItemEntry(const QModelIndex &index) const
    {
        QList<ThemeListModelEntry*> items = ThemeListModel::items();
        if (!index.isValid() || index.row() >= items.count())
            return 0;
        return static_cast<CallItemEntry*>(items.at(index.row()));
    }

    //sort items in list, reimplemented from QAbstractItemModel
    void sort(int column, Qt::SortOrder order)
    {
        if (column != 0)
            return;
        QList<ThemeListModelEntry*> items = ThemeListModel::items();
        LessThan compare = (order == Qt::AscendingOrder ? &itemLessThan : &itemGreaterThan);
        qSort(items.begin(), items.end(), compare);
        emit dataChanged(QAbstractListModel::index(0, 0), QAbstractListModel::index(items.count() - 1, 0));
    }

private:
    static bool itemLessThan(const ThemeListModelEntry *left, const ThemeListModelEntry* right)
    {
        return (static_cast<const CallItemEntry*>(left)->display) < (static_cast<const CallItemEntry*>(right)->display); //we use QString::compare - QString::localeAwareCompare not needed
    }

    static bool itemGreaterThan(const ThemeListModelEntry *left, const ThemeListModelEntry* right)
    {
        return !itemLessThan(left,right);
    }
};

//===================================================================

/* define CallItemListView */
CallItemListView::CallItemListView(ThemeWidgetItem *ti, QWidget *parent)
    : QListView(parent), thisThemeItem( ti )
{
    setResizeMode(QListView::Adjust);
}

void CallItemListView::keyPressEvent(QKeyEvent* ke)
{
    if ( ke->key() == Qt::Key_Flip ) {
        // QListWidget accepts too much
        ke->ignore();
    } else {
        QListView::keyPressEvent(ke);
    }
}

void CallItemListView::currentChanged(const QModelIndex &cur, const QModelIndex &prev)
{
    if ( selectionMode() != QAbstractItemView::SingleSelection )
        return;

    CallItemModel *m = qobject_cast<CallItemModel *>(model());
    CallItemEntry *item = m->callItemEntry(cur);
    if (item)
        item->setValue( "State", tr( " (Connect)",
                "describing an action to take on a call, make sure keeping the space in the beginning" ) );
    item = m->callItemEntry(prev);
    if (item)
        item->setValue( "State", tr( " (Hold)",
                "describing an action to take on a call, make sure keeping the space in the beginning" ) );
}


//===========================================================================

class MouseControlDialog : public QDialog
{
    Q_OBJECT
public:
    MouseControlDialog( QWidget* parent = 0, Qt::WFlags fl = 0 )
        : QDialog(parent, fl), m_tid(0), m_parent(parent), m_mouseUnlocked(false)
    {
        QColor c(Qt::black);
        c.setAlpha(180);

        setAttribute(Qt::WA_SetPalette, true);

        QPalette p = palette();
        p.setBrush(QPalette::Window, c);
        setPalette(p);

        QVBoxLayout *vBox = new QVBoxLayout(this);
        QHBoxLayout *hBox = new QHBoxLayout;

        QIcon icon(":icon/select");

        QLabel *l = new QLabel(this);
        l->setPixmap(icon.pixmap(44, 44));
        hBox->addStretch();
        hBox->addWidget(l);
        hBox->addStretch();

        int height = l->sizeHint().height();

        vBox->addLayout(hBox);

        l = new QLabel(this);
        l->setWordWrap(true);
        if (Qtopia::mousePreferred()) {
            l->setText(tr("Move the slider to activate the touch screen."));
        } else {
            l->setText(tr("Press key <b>Down</b> to activate the touch screen.", "translate DOWN to name of key with down arrow"));
        }
        vBox->addWidget(l);
        height += l->sizeHint().height();

        if (Qtopia::mousePreferred()) {
            m_slider = new QSlider(Qt::Horizontal, this);
            m_slider->installEventFilter(this);
            m_slider->setRange(0, 10);
            m_slider->setPageStep(1);
            connect(m_slider, SIGNAL(valueChanged(int)), this, SLOT(sliderMoved(int)));
            vBox->addWidget(m_slider);

            height += m_slider->sizeHint().height();
        }

        QRect d = QApplication::desktop()->screenGeometry();
        int dw = d.width();
        int dh = d.height();

        height += QApplication::style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing);
        height += QApplication::style()->pixelMetric(QStyle::PM_LayoutTopMargin);
        height += QApplication::style()->pixelMetric(QStyle::PM_LayoutBottomMargin);
        setGeometry(20*dw/100, (dh - height)/2, 60*dw/100, height);

        m_parent->installEventFilter(this);
    }

    static const int TIMEOUT = 2000;
signals:
    void releaseMouse();
    void grabMouse();

protected:
    void timerEvent( QTimerEvent *e )
    {
        Q_UNUSED(e)
        close();
    }

    void closeEvent( QCloseEvent *e )
    {
        // if failed to unlock the touch screen
        // hand over mouse grab to call screen
        if ( Qtopia::mousePreferred() )
            if ( m_slider->value() != m_slider->maximum() )
               emit grabMouse();
        QDialog::closeEvent( e );
    }

    void showEvent( QShowEvent *e )
    {
        m_mouseUnlocked = false;
        if (Qtopia::mousePreferred()) {
            m_slider->grabMouse();
            m_slider->setValue(0);
        }

        resetTimer();
        QDialog::showEvent( e );
    }

    void keyPressEvent( QKeyEvent *e )
    {
        if ( e->key() == Qt::Key_Down ) {
            m_mouseUnlocked = true;
            emit releaseMouse();
            close();
        }
    }

    bool eventFilter( QObject *o, QEvent *e )
    {
        if ( o == m_parent ) {
            if ( e->type() == QEvent::WindowActivate ) {
                if ( !m_mouseUnlocked )
                   emit grabMouse();
            } else if ( e->type() == QEvent::WindowDeactivate ) {
                m_mouseUnlocked = false;
                emit releaseMouse();
            }
        }

        if ( Qtopia::mousePreferred() && o == m_slider ) {
            // do not allow to move slider with key press
            if ( e->type() == QEvent::KeyPress
                    || e->type() == QEvent::KeyRelease )
                return true;
        }
        return false;
    }

private slots:
    void resetTimer()
    {
        killTimer( m_tid );
        m_tid = startTimer( TIMEOUT );
    }

    void sliderMoved( int value )
    {
        if ( value == m_slider->maximum() ) {
            m_slider->releaseMouse();
            close();
        } else {
            resetTimer();
        }
    }

private:
    int m_tid;
    QSlider *m_slider;
    QWidget *m_parent;
    bool m_mouseUnlocked;
};

class CallScreenKeyboardFilter : public QtopiaKeyboardFilter
{
public:
    CallScreenKeyboardFilter() {}
    ~CallScreenKeyboardFilter() {}
protected:
    virtual bool filter(int, int, int, bool, bool)
    {
        return true;
    }
};

//===========================================================================

/*!
    \class ThemedCallScreen
    \inpublicgroup QtTelephonyModule
    \brief The ThemedCallScreen class provides a phone call screen.
    \ingroup QtopiaServer::PhoneUI

    This widget can be \l{Theming}{themed}.
    An image of this call screen using the Qt Extended theme can be found in the \l{Server Widget Classes}{server widget gallery}.

    This class is a Qt Extended \l{QtopiaServerApplication#qt-extended-server-widgets}{server widget}.
    It is part of the Qt Extended server and cannot be used by other Qt Extended applications.
  */

/*!
    Constrcuts a new instance with the given \a parent and \a flags.
*/
ThemedCallScreen::ThemedCallScreen(QWidget *parent, Qt::WFlags flags)
    : QAbstractCallScreen(parent, flags)
{
    QVBoxLayout *vbl = new QVBoxLayout(this);
    vbl->setMargin(0);
    view = new ThemedCallScreenView(DialerControl::instance(), this);
    vbl->addWidget(view);

    connect(view, SIGNAL(acceptIncoming()), this, SIGNAL(acceptIncoming()));
    connect(view, SIGNAL(listEmpty()), this, SLOT(hide()));
    connect(view, SIGNAL(hangupCall()), this, SIGNAL(hangupCall()));
    connect(view, SIGNAL(raiseCallScreen()), this, SLOT(raiseCallScreen()));
    connect(view, SIGNAL(hideCallScreen()), this, SLOT(hide()));
    setWindowTitle(tr("Calls"));
}

/*!
    \reimp
*/
void ThemedCallScreen::stateChanged()
{
    view->stateChanged();
}

/*!
    \internal
*/
void ThemedCallScreen::closeEvent(QCloseEvent *e)
{
    if (view->tryClose())
        e->accept();
    else
        e->ignore();
}

/*!
    \internal
*/
void ThemedCallScreen::raiseCallScreen()
{
    showMaximized();
    raise();
    activateWindow();
}

//===========================================================================

/*!
  \class ThemedCallScreenView
    \inpublicgroup QtTelephonyModule
  \brief The ThemedCallScreenView class provides a phone call screen.
  \ingroup QtopiaServer::PhoneUI
  \internal

  This class is part of the Qt Extended server and cannot be used by other Qt Extended applications.
  */


/*!
  \fn void ThemedCallScreenView::acceptIncoming()
  \internal
  */

/*!
  \fn int ThemedCallScreenView::activeCallCount() const
  \internal
  */

/*!
  \fn int ThemedCallScreenView::heldCallCount() const
  \internal
  */


/*!
    \fn bool ThemedCallScreenView::incomingCall() const
    \internal
    */

/*!
    \fn bool ThemedCallScreenView::inMultiCall() const
    \internal
    */

/* define ThemedCallScreenView */
/*!
  \internal
  */
ThemedCallScreenView::ThemedCallScreenView(DialerControl *ctrl, QWidget *parent)
    : PhoneThemedView(parent), control(ctrl), digits(0), listView(0), actionGsm(0),
    activeCount(0), holdCount(0) , keypadVisible(false), mLayout( 0 ),
    updateTimer( 0 ), gsmActionTimer(0), secondaryCallScreen(0), m_model(0),
#ifdef QTOPIA_TELEPHONY
    m_callAudioHandler(0),m_callAudio(0),
#endif
    videoWidget(0), showWaitDlg(false), symbolTimer(0), m_mouseCtrlDlg(0)
{
    callScreen = this;
    setObjectName(QLatin1String("calls"));

    contextMenu = QSoftMenuBar::menuFor(this);

    actionAnswer = new QAction(QIcon(":icon/phone/answer"),tr("Answer", "answer call"), this);
    connect(actionAnswer, SIGNAL(triggered()), this, SIGNAL(acceptIncoming()));
    actionAnswer->setVisible(false);
    contextMenu->addAction(actionAnswer);

    actionSendBusy = new QAction(QIcon(":icon/phone/reject"), tr("Send Busy"), this);
    connect(actionSendBusy, SIGNAL(triggered()), control, SLOT(sendBusy()));
    actionSendBusy->setVisible(false);
    contextMenu->addAction(actionSendBusy);

    actionMute = new QAction(QIcon(":icon/mute"),tr("Mute"), this);
    connect(actionMute, SIGNAL(triggered()), this, SLOT(muteRingSelected()));
    actionMute->setVisible(false);
    contextMenu->addAction(actionMute);

    actionEnd = new QAction(QIcon(":icon/phone/hangup"),tr("End"),this);
    connect(actionEnd, SIGNAL(triggered()), this, SLOT(endCall()));
    actionEnd->setVisible(false);
    contextMenu->addAction(actionEnd);

    actionEndAll = new QAction(tr("End all calls"), this);
    connect(actionEndAll, SIGNAL(triggered()), control, SLOT(endAllCalls()));
    actionEndAll->setVisible(false);
    contextMenu->addAction(actionEndAll);

    actionHold = new QAction(QIcon(":icon/phone/hold"),tr("Hold"), this);
    connect(actionHold, SIGNAL(triggered()), control, SLOT(hold()));
    actionHold->setVisible(false);
    contextMenu->addAction(actionHold);

    actionResume = new QAction(QIcon(":icon/phone/resume"),tr("Resume"), this);
    connect(actionResume, SIGNAL(triggered()), control, SLOT(unhold()));
    actionResume->setVisible(false);
    contextMenu->addAction(actionResume);

    actionMerge = new QAction(QIcon(":icon/phone/conference"),tr("Join"), this);
    connect(actionMerge, SIGNAL(triggered()), control, SLOT(join()));
    actionMerge->setVisible(false);
    contextMenu->addAction(actionMerge);

    actionSplit = new QAction(tr("Split..."), this);
    connect(actionSplit, SIGNAL(triggered()), this, SLOT(splitCall()));
    actionSplit->setVisible(false);
    contextMenu->addAction(actionSplit);

    actionTransfer = new QAction(QIcon(":icon/phone/callforwarding"),tr("Transfer"),this);
    connect(actionTransfer, SIGNAL(triggered()), control, SLOT(transfer()));
    actionTransfer->setVisible(false);
    contextMenu->addAction(actionTransfer);

    connect(control, SIGNAL(callControlRequested()), this, SLOT(showProgressDlg()));
    connect(control, SIGNAL(callControlSucceeded()), this, SLOT(hideProgressDlg()));


#ifdef QTOPIA_TELEPHONY
    m_callAudioHandler = qtopiaTask<CallAudioHandler>();
    if (m_callAudioHandler)
        if (m_callAudioHandler->isInitialized())
            initializeAudioConf();
        else
            QObject::connect(m_callAudioHandler, SIGNAL(initialized()),
                         this, SLOT(initializeAudioConf()));
#endif

    QObject::connect(this, SIGNAL(itemReleased(ThemeItem*)),
                    this, SLOT(themeItemReleased(ThemeItem*)));

    setWindowTitle(tr("Calls"));

    if (QApplication::desktop()->numScreens() > 1) {
        // We might have a secondary screen.  Search for a normal
        // (i.e. non-television) screen to create the secondary call screen.
        int secondScreen = -1;
        QDesktopWidget *desktop = QApplication::desktop();
        for (int screen = 0; screen < desktop->numScreens(); ++screen) {
            QScreenInformation info(screen);
            if (screen != desktop->primaryScreen() &&
                info.type() == QScreenInformation::Normal) {
                secondScreen = screen;
                break;
            }
        }
        if (secondScreen != -1) {
            secondaryCallScreen = new SecondaryCallScreen;
            secondaryCallScreen->setGeometry(QApplication::desktop()->availableGeometry(secondScreen));
        }
    }

    QObject::connect(control,
                     SIGNAL(requestFailed(QPhoneCall,QPhoneCall::Request)),
                     this,
                     SLOT(requestFailed(QPhoneCall,QPhoneCall::Request)));

    QObject::connect(control, SIGNAL(callConnected(QPhoneCall)),
                     this, SLOT(callConnected(QPhoneCall)));

    QObject::connect(control, SIGNAL(callDropped(QPhoneCall)),
                     this, SLOT(callDropped(QPhoneCall)));

    QObject::connect(control, SIGNAL(callDialing(QPhoneCall)),
                     this, SLOT(callDialing(QPhoneCall)));

    // reject any dialogs when new call coming in
    QObject::connect(control, SIGNAL(callIncoming(QPhoneCall)),
                     this, SLOT(rejectModalDialog()));
    QObject::connect(control, SIGNAL(callIncoming(QPhoneCall)),
                     this, SLOT(callIncoming(QPhoneCall)));

#if defined(MEDIA_SERVER) && defined(QTOPIA_TELEPHONY)
    VideoRingtone *vrt = qtopiaTask<VideoRingtone>();
    if ( vrt ) {
        QObject::connect( vrt, SIGNAL(videoWidgetReady()),
                this, SLOT(setVideoWidget()) );
        QObject::connect( vrt, SIGNAL(videoRingtoneStopped()),
                this, SLOT(deleteVideoWidget()) );
    } else {
        qLog(Component) << "CallScreen: VideoRingtone not available";
    }
    // delete the video widget once call is answered
    connect( this, SIGNAL(acceptIncoming()),
            this, SLOT(deleteVideoWidget()) );
#endif

    // Due to delayed intialization of call screen
    // manual update on incoming call is required when call screen is created
    if ( control->hasIncomingCall() )
        callIncoming( control->incomingCall() );

    ThemeControl *tctrl = qtopiaTask<ThemeControl>();
    if ( tctrl )
        tctrl->registerThemedView(this, "CallScreen");
    else 
        qLog(Component) << "ThemedCallScreen: ThemeControl not available, theme will not work properly";

    m_taskManagerEntry = new TaskManagerEntry(tr("Calls"), "phone/calls", this);
    QObject::connect(m_taskManagerEntry, SIGNAL(activated()), this, SIGNAL(raiseCallScreen()));

}

/*!
  Sets the video player widget to the ThemedCallScreenView.
  */
void ThemedCallScreenView::setVideoWidget()
{
#if defined(MEDIA_SERVER) && defined(QTOPIA_TELEPHONY)
    if ( !m_model->rowCount() )
        return;

    VideoRingtone *vrt = qtopiaTask<VideoRingtone>();
    if ( !vrt )
        return;

    videoWidget = vrt->videoWidget();
#else
    return; //video ring tone requires qtopiamedia
#endif
    videoWidget->setParent( this );

    QRect availableGeometry = rect();
    QRect lastItemRect =
        listView->visualRect( m_model->index( m_model->rowCount() - 1 ) );

    availableGeometry.setTop(
            lastItemRect.top()
            + lastItemRect.height() );

    videoWidget->setGeometry( availableGeometry );

    // set menu.
    QMenu *menu = QSoftMenuBar::menuFor( videoWidget );
    menu = contextMenu;
    QSoftMenuBar::setLabel(videoWidget, Qt::Key_Select, "phone/answer", tr("Answer", "answer call"));
    QSoftMenuBar::setLabel(listView, Qt::Key_Back, ":icon/mute", tr("Mute"));

    qLog(Media) << "Displaying the video ringtone";
    videoWidget->show();
}

void ThemedCallScreenView::initializeAudioConf()
{
#ifdef QTOPIA_TELEPHONY
    // add speaker, bluetooth headset actions, etc
    m_callAudioHandler->addOptionsToMenu(contextMenu);
#endif
}

/*!
  Hides the video player widget.
*/
void ThemedCallScreenView::deleteVideoWidget()
{
    if (videoWidget != 0)
        delete videoWidget;

    videoWidget = 0;
}

/*!
  \internal
  */
bool ThemedCallScreenView::dialNumbers(const QString & numbers)
{
    // allow to enter '+' symbol by pressing '*' key twice quickly
    // required when an internationl number is entered
    if ( numbers == "*" ) {
        if ( dtmfDigits.endsWith( "*" )
                && symbolTimer->isActive() ) {
            dtmfDigits = dtmfDigits.left( dtmfDigits.length() - 1 );
            appendDtmfDigits( "+" );
            return true;
        } else {
            if ( !symbolTimer ) {
                symbolTimer = new QTimer( this );
                symbolTimer->setSingleShot( true );
            }
            symbolTimer->start( 500 );
        }
    }

    // do not send dtmf tones while dialing for now.
    // but need a way to queue dtmf tones while dialing.
    // e.g. a phone number followed by an extension.
    if (/*control->isDialing() || */control->hasActiveCalls()) {
        // Inject the specified digits into the display area.
        control->activeCalls().first().tone(numbers);
        appendDtmfDigits(numbers);
        return true;
    } else if ( control->hasIncomingCall() ) {
        appendDtmfDigits(numbers);
    } else if ( control->hasCallsOnHold() ) {
        appendDtmfDigits(numbers);
        return true;
    }
    return false;
}

/*!
  \internal
  */
void ThemedCallScreenView::themeLoaded( const QString & )
{
    ThemeWidgetItem *item = 0;
    item = (ThemeListItem *)findItem( "callscreen", ThemedView::List );
    delete mLayout;
    mLayout = 0;
    if( !item ) {
        qWarning("No callscreen element defined for ThemedCallScreenView theme.");
        mLayout = new QVBoxLayout( this );
        listView = new CallItemListView(0,this);
    } else {
        listView = qobject_cast<CallItemListView *>(item->widget());
        Q_ASSERT(listView != 0 );
        // active item
        // hold item
        // joined call indicator
        // contact image
        // phone number text
        // status text
    }
    listView->setFrameStyle(QFrame::NoFrame);
    listView->installEventFilter(this);
    listView->setSelectionMode(QAbstractItemView::NoSelection);
    listView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    connect(listView, SIGNAL(activated(QModelIndex)), this, SLOT(callSelected(QModelIndex)));
    QSoftMenuBar::setLabel(listView, Qt::Key_Select, QSoftMenuBar::NoLabel);

    item = (ThemeWidgetItem *)findItem( "callscreennumber", ThemedView::Widget );
    if( !item ) {
        qWarning("No callscreennumber input element defined for ThemedCallScreenView theme.");
        if( !mLayout )
            mLayout = new QVBoxLayout( this );
        digits = new QLineEdit( this );
    } else {
        digits = qobject_cast<QLineEdit*>(item->widget());
        Q_ASSERT(digits != 0);
    }
    digits->setFrame(false);
    digits->setReadOnly(true);
    digits->setFocusPolicy(Qt::NoFocus);
    digits->hide();
    connect( digits, SIGNAL(textChanged(QString)),
            this, SLOT(updateLabels()) );

    if( mLayout ) {
        mLayout->addWidget( listView );
        mLayout->addWidget( digits );
    } else {
        manualLayout();
    }

    stateChanged();
}

/*!
  \internal
  */
void ThemedCallScreenView::manualLayout()
{
    ThemeRectItem *keypaditem = (ThemeRectItem *)findItem( "keypad-box", ThemedView::Rect );
    ThemeRectItem *keypadbutton = (ThemeRectItem *)findItem( "keypad-show-container", ThemedView::Rect );
    if( keypaditem && keypadbutton ) {
        keypaditem->setActive( keypadVisible );
        keypadbutton->setActive( !keypadVisible );

    }
    update();
}

/*!
  \internal
  */
QString ThemedCallScreenView::ringTone()
{
    CallItemModel* m = qobject_cast<CallItemModel *>(listView->model());
    for (int i = m->rowCount()-1; i>=0; i--) {
        CallItemEntry* item = m->callItemEntry(m->index(i));
        if ( item && item->callData.callState == QPhoneCall::Incoming ) {
            return item->callData.ringTone;
        }
    }
    return QString();
}

/*!
  \internal
  */
void ThemedCallScreenView::clearDtmfDigits(bool clearOneChar)
{
    if(dtmfDigits.isEmpty())
        return;

    if (clearOneChar)
        dtmfDigits = dtmfDigits.left(dtmfDigits.length() - 1);
    else
        dtmfDigits.clear();
    if (digits)
        digits->setText(dtmfDigits);

    if (dtmfDigits.isEmpty()) {
        if (digits)
            digits->hide();
        updateLabels();
    } else if (gsmActionTimer) {
        gsmActionTimer->start();
    }

    manualLayout();
    CallItemModel* m = qobject_cast<CallItemModel *>(listView->model());
    m->triggerUpdate();

    // remove menu item
    setGsmMenuItem();
}

/*!
  \internal
  */
void ThemedCallScreenView::setGsmMenuItem()
{
#ifdef QTOPIA_TELEPHONY
    if (!actionGsm) {
        actionGsm = new QAction(QIcon(":icon/phone/answer"),QString(), this);
        connect(actionGsm, SIGNAL(triggered()), this, SLOT(actionGsmSelected()));
        QSoftMenuBar::menuFor(this)->addAction(actionGsm);
    }

    AbstractDialFilter::Action act = AbstractDialFilter::Continue;
    if ( AbstractDialFilter::defaultFilter() ) {
        act = AbstractDialFilter::defaultFilter()->filterInput( dtmfDigits, false, true );
    }

    actionGsm->setVisible(!dtmfDigits.isEmpty());

    // update menu text & lable for Key_Select
    if (!dtmfDigits.isEmpty() ) {
        if (act == AbstractDialFilter::ActionTaken) {
            actionGsm->setText(tr("Send %1").arg(dtmfDigits));
            QSoftMenuBar::setLabel(listView, Qt::Key_Select, "", tr("Send"));
        } else {
            actionGsm->setText(tr("Call %1", "%1=phone number").arg(dtmfDigits));
            QSoftMenuBar::setLabel(listView, Qt::Key_Select, "phone/answer", tr("Call"));
        }
    }
#endif
}

/*!
  \internal
  */
void ThemedCallScreenView::actionGsmSelected()
{
#ifdef QTOPIA_TELEPHONY
    AbstractDialFilter::Action action = AbstractDialFilter::Continue;
    if (AbstractDialFilter::defaultFilter()) {
        action = AbstractDialFilter::defaultFilter()->filterInput(dtmfDigits, true);
    }

    // if the digits are not filtered place a call
    if ( action != AbstractDialFilter::ActionTaken ) {
        // check if contact exists
        QContactModel *m = ServerContactModel::instance();
        QContact cnt = m->matchPhoneNumber(dtmfDigits);

        if ( cnt == QContact() ) { // no contact
            QtopiaServiceRequest service( "Dialer", "dial(QString,QString)" );
            service << QString() << dtmfDigits;
            service.send();
        } else {
            QtopiaServiceRequest service( "Dialer", "dial(QString,QUniqueId)" );
            service << dtmfDigits << cnt.uid();
            service.send();
        }
    }
    // clear digits wheather filtered or not
    clearDtmfDigits();
#endif
}

/*!
  \internal
  */
void ThemedCallScreenView::updateLabels()
{
    // update context label according to the current call count.
    if (control->allCalls().count() >= 2)
        QSoftMenuBar::setLabel(listView, Qt::Key_Select, "phone/swap", tr("Swap"));
    else if (control->activeCalls().count() == 1)
        QSoftMenuBar::setLabel(listView, Qt::Key_Select, "phone/hold", tr("Hold"));
    else
        QSoftMenuBar::setLabel(listView, Qt::Key_Select, QSoftMenuBar::NoLabel);

    // display clear icon when dtmf digits are entered.
    if (digits->text().isEmpty())
        QSoftMenuBar::setLabel(this, Qt::Key_Back, QSoftMenuBar::Back);
    else
        QSoftMenuBar::setLabel(this, Qt::Key_Back, QSoftMenuBar::BackSpace);
}

/*!
  \internal
  */
void ThemedCallScreenView::appendDtmfDigits(const QString &dtmf)
{
    dtmfDigits.append(dtmf);
    if(dtmfDigits.isEmpty())
        return;

    digits->setText(dtmfDigits);
    digits->setCursorPosition(digits->text().length());
    digits->show();

    // if video widget is shown reduce the size
    if ( videoWidget ) {

        QRect curGeometry = videoWidget->geometry();
        QRect digitsRect = digits->geometry();

        curGeometry.setBottom( digitsRect.top() );

        videoWidget->setGeometry( curGeometry );
    }

    manualLayout();
    CallItemModel* m = qobject_cast<CallItemModel *>(listView->model());
    m->triggerUpdate();

    // add menu item.
    setGsmMenuItem();

    if (!gsmActionTimer) {
        gsmActionTimer = new QTimer(this);
        gsmActionTimer->setInterval(SELECT_KEY_TIMEOUT);
        gsmActionTimer->setSingleShot(true);
        QObject::connect(gsmActionTimer, SIGNAL(timeout()), this, SLOT(updateLabels()));
    }
    gsmActionTimer->start();

#ifdef QTOPIA_TELEPHONY
    // filter immediate action
    if (AbstractDialFilter::defaultFilter()) {
        if ( AbstractDialFilter::ActionTaken == 
                AbstractDialFilter::defaultFilter()->filterInput(dtmfDigits)) {
            clearDtmfDigits();
        }
    }
#endif
}

/*!
  \internal
  */
void ThemedCallScreenView::stateChanged()
{
    if( !listView || !digits )
        return;
    const QList<QPhoneCall> &calls = control->allCalls();


    // see if any calls have ended.

    CallItemEntry *item = 0;
    CallItemModel *m = qobject_cast<CallItemModel *>(listView->model());
    for (int i = m->rowCount()-1; i>=0; i--) {
        item = m->callItemEntry(m->index(i));
        if (item && !calls.contains(item->call())) {
            if (item->callData.disconnectTime.isNull()) {
                item->callData.disconnectTime = QDateTime::currentDateTime();
                item->setText("f");
            }
        }
    }

    activeCount = 0;
    holdCount = 0;
    incoming = false;
    bool dialing = false;

    CallItemEntry *active = 0;
    CallItemEntry *primaryItem = 0;
    int primaryOrder = 9;

    // any calls added/changed state
    int idx = 1;
    QList<QPhoneCall>::ConstIterator it;
    QString name;
    bool itemStateChanged = false;
    QString state;

    for (it = calls.begin(); it != calls.end(); ++it, idx++) {
        int sortOrder = 9;
        const QPhoneCall &call(*it);
        item = findCall(call, m);
        if (!item) {
            item = new CallItemEntry(control, call, m);
            m->addEntry(item);
            manualLayout();
        }
        if (item->callData.connectTime.isNull() && call.established())
            item->callData.connectTime = QDateTime::currentDateTime();
        //m->triggerUpdate();

        name = item->callData.numberOrName;
        if (name.isEmpty())
            name = ThemedCallScreenView::tr("Unknown caller");

        if( call.state() != item->callData.callState ) {
            item->callData.callState = call.state();
            itemStateChanged = true;
        }
        if (call.state() == QPhoneCall::Connected) {
            activeCount++;
            if (!active)
                active = item;
            sortOrder = 2;
            state = ThemedCallScreenView::tr("Connected", "call state");
        } else if (call.state() == QPhoneCall::Hold) {
            holdCount++;
            sortOrder = 3;
            state = ThemedCallScreenView::tr("Hold", "call state");
        } else if (call.state() == QPhoneCall::Dialing ||
                   call.state() == QPhoneCall::Alerting) {
            dialing = true;
            if (!active)
                active = item;
            sortOrder = 1;
            item->callData.callState = QPhoneCall::Dialing;
            item->callData.dialTime = QDateTime::currentDateTime();
            state = ThemedCallScreenView::tr("Dialing", "call state");
        } else if (call.state() == QPhoneCall::Incoming) {
            sortOrder = 0;
            incoming = true;
            item->callData.callState = QPhoneCall::Incoming;
            state = ThemedCallScreenView::tr("Incoming", "call state");
        }
        item->setText(QChar('a'+sortOrder) + QString::number(idx));
        item->callData.state = state;

        bool isMulti = ((callScreen->activeCallCount() > 1 && call.state() == QPhoneCall::Connected) ||
                (callScreen->heldCallCount() > 1 && call.onHold()));
        item->setValue( "State", tr( " (%1)", "describing call state, make sure keeping the space in the beginning" ).arg(state) );
        item->setValue( "CallId", tr( "(%1) ", "describing call id, make sure keeping the space int the end" ).arg( call.modemIdentifier() ) );
        item->setValue( "Identifier", name );
        item->setValue( "Conference", isMulti );

        if (sortOrder < primaryOrder) {
            primaryItem = item;
            primaryOrder = sortOrder;
        }
    }

    for (int i = m->rowCount()-1; i>=0; i--) {
        if (!m->index(i).isValid())
            break;
        CallItemEntry* item = m->callItemEntry(m->index(i));
        if( !item )
            continue;
        if( item->call().dropped() )
            item->setValue("State", tr( " (Disconnected)", "describing call state, make sure keeping the space in the beginning") );
    }

    if (secondaryCallScreen && primaryItem)
        secondaryCallScreen->setCallData(primaryItem->callData);

    // update available actions.
    actionAnswer->setVisible(control->hasIncomingCall());
    actionSendBusy->setVisible(control->hasIncomingCall());
    actionMute->setVisible(control->hasIncomingCall());
    actionHold->setVisible(activeCount && !holdCount && !incoming && !dialing);
    actionResume->setVisible(holdCount && !incoming && !dialing);
    actionEnd->setVisible((activeCount || holdCount || dialing) && !incoming);
    actionEndAll->setVisible(activeCount && holdCount && !incoming);
    actionMerge->setVisible(activeCount && holdCount &&
                            activeCount < MAX_JOINED_CALLS &&
                            holdCount < MAX_JOINED_CALLS && !incoming);
    actionSplit->setVisible(activeCount > 1 && !holdCount && !incoming);
    actionTransfer->setVisible(activeCount == 1 && holdCount == 1 && !incoming);

    // update the speaker and bluetooth headset actions.
    bool nonActiveDialing = dialing && !activeCount;

#ifdef QTOPIA_TELEPHONY
    if (!m_callAudio)
        m_callAudio = AbstractAudioHandler::audioHandler("CallAudio");
    if (m_callAudio)
        m_callAudio->activateAudio(activeCount || holdCount || nonActiveDialing /* || incoming*/ );
#endif

    if (incoming) {
        QSoftMenuBar::setLabel(listView, Qt::Key_Select, "phone/answer", tr("Answer"));
    } else if (activeCount && holdCount) {
        actionResume->setText(tr("Swap", "change to 2nd open phoneline and put 1st on hold"));
        actionResume->setIcon(QPixmap(":icon/phone/swap"));
        QSoftMenuBar::setLabel(listView, Qt::Key_Select, "phone/swap", tr("Swap"));
    } else if (holdCount && !activeCount && !dialing && !incoming) {
        actionResume->setText(tr("Resume"));
        actionResume->setIcon(QIcon(":icon/phone/resume"));
        QSoftMenuBar::setLabel(listView, Qt::Key_Select, "phone/resume", tr("Resume"));
    } else if (activeCount && !holdCount && !dialing && !incoming) {
        QSoftMenuBar::setLabel(listView, Qt::Key_Select, "phone/hold", tr("Hold"));
    } else {
        QSoftMenuBar::setLabel(listView, Qt::Key_Select, QSoftMenuBar::NoLabel);
    }

    if (incoming && listView->selectionMode() != QAbstractItemView::SingleSelection)
        if ( incoming )
            QSoftMenuBar::setLabel(listView, Qt::Key_Back, ":icon/mute", tr("Mute"));
        else
            QSoftMenuBar::setLabel(listView, Qt::Key_Back, QSoftMenuBar::NoLabel);
    else
        QSoftMenuBar::clearLabel(listView, Qt::Key_Back);

    //layout()->activate();
    m->sort(0, Qt::AscendingOrder);
    if (m->rowCount() > 0)
        listView->scrollTo(m->index(0));

    if( itemStateChanged ) // any items state changed?
        updateAll();
    else
        m->triggerUpdate();

    // Check dtmf status
    if(!active || active->call().identifier() != dtmfActiveCall)
        clearDtmfDigits();
    if(active)
        dtmfActiveCall = active->call().identifier();
    else
        dtmfActiveCall = QString();
    update();
}

/*!
  \internal
  */
void ThemedCallScreenView::requestFailed(const QPhoneCall &,QPhoneCall::Request r)
{
    hideProgressDlg();

    QString title, text;
    title = tr("Failure");
    if(r == QPhoneCall::HoldFailed) {
        text = tr("Hold attempt failed");
    } else if(r == QPhoneCall::ActivateFailed) {
        text = tr("Activate attempt failed");
    } else {
        text = tr("Join/transfer attempt failed");
    }

    QAbstractMessageBox *box = QAbstractMessageBox::messageBox(0, title, text, QAbstractMessageBox::Warning);
    box->setTimeout(3000, QAbstractMessageBox::NoButton);
    QtopiaApplication::execDialog(box);
    delete box;
}

/*!
  \internal
  */
CallItemEntry *ThemedCallScreenView::findCall(const QPhoneCall &call, CallItemModel *m)
{
    CallItemEntry *item = 0;
    for (int i = m->rowCount()-1; i>=0; i--) {
        item = m->callItemEntry(m->index(i));
        if (item->call() == call)
            return item;
    }

    return 0;
}

/*!
  \internal
  */
void ThemedCallScreenView::updateAll()
{
    if( !listView || !digits )
        return;

    CallItemModel *m = qobject_cast<CallItemModel *>(listView->model());
    for (int i = m->rowCount()-1; i>=0; i--) {
        if (!m->index(i).isValid())
            break;
        CallItemEntry* item = m->callItemEntry(m->index(i));
        if (item->call().state() == QPhoneCall::ServiceHangup) {
            // USSD message is coming soon, so remove from the call screen.
            m->removeEntry(m->index(i)); // removeEntry will delete the item
            manualLayout();
            i--;
            continue;
        } else if (item->call().dropped()) {
            // Remove dropped calls after a short delay
            if (!item->callData.disconnectTime.isNull() &&
                item->callData.disconnectTime.time().elapsed() > 3000) {
                m->removeEntry(m->index(i)); // removeEntry will delete the item
                manualLayout();
                i--;
                continue;
            }
        }

        /* Set appropriate information in the CallItemModel item */
        item->setValue( "Duration", item->callData.durationString() );
        if( item->value("Photo").toString().isEmpty() )
            item->setValue( "Photo", item->callData.photo );
        if (secondaryCallScreen && item->call() == secondaryCallScreen->call()) {
            secondaryCallScreen->setCallData(item->callData);
            secondaryCallScreen->showMaximized();
        }
    }
    if (m->rowCount() == 0) {
        emit listEmpty();
        m_taskManagerEntry->hide();
    } else {
        m->triggerUpdate();
    }
}

/*!
  \internal
  */
void ThemedCallScreenView::splitCall()
{
    setWindowTitle(tr("Select Call...","split 2 phone lines after having joined them"));
    setSelectMode(true);
    actionHold->setVisible(false);
    actionResume->setVisible(false);
    actionEnd->setVisible(false);
    actionEndAll->setVisible(false);
    actionMerge->setVisible(false);
    actionSplit->setVisible(false);
    actionTransfer->setVisible(false);
}

/*!
  \internal
  */
void ThemedCallScreenView::callSelected(const QModelIndex& index)
{
    CallItemModel* m = qobject_cast<CallItemModel *>(listView->model());

    CallItemEntry *callItem = m->callItemEntry(index);
    if (!callItem )
        qWarning("ThemedCallScreenView::callSelected(): invalid index passed to CallItemModel");
    if (m->flags(index) & Qt::ItemIsSelectable) {
        if (callScreen->heldCallCount() || callScreen->activeCallCount() > 1) {
            setSelectMode(false);
            //XXX I could be used for more than just split
            callItem->call().activate(QPhoneCall::CallOnly);
            setWindowTitle(tr("Calls"));
        } else if (!Qtopia::mousePreferred()){ // this is the only call
            if (callItem->call().onHold())
                control->unhold();
            else if (callItem->call().connected())
                control->hold();
        }
    }
}


/*!
  \internal
  */
void ThemedCallScreenView::setItemActive(const QString &name, bool active)
{
    ThemeItem *item = (ThemeItem *)findItem(name);
    if (item)
        item->setActive(active);
}

/*!
  \internal
  */
void ThemedCallScreenView::themeItemReleased(ThemeItem *item)
{
    if (!item)
        return;

    // if the touch screen is locked to nothing
    if (QWidget::mouseGrabber() == this)
        return;

    if (item->itemName() == "answer")
    {
        actionAnswer->trigger();
    }
    else if (item->itemName() == "endcall")
    {
        actionEnd->trigger();
    }
    else if (item->itemName() == "hold")
    {
        if (!control->hasActiveCalls())
            return;
        item->setActive(false);
        setItemActive("resume", true);
        actionHold->trigger();
    }
    else if (item->itemName() == "resume")
    {
        if (!control->hasCallsOnHold())
            return;
        item->setActive(false);
        setItemActive("hold", true);
        actionResume->trigger();
    }
    else if (item->itemName() == "sendbusy")
    {
        actionSendBusy->trigger();
    }
    else if (item->itemName() == "show_keypad")
    {
        setItemActive("menu-box", false);
        setItemActive("keypad-box", true);
    }
    else if (item->itemName() == "hide_keypad")
    {
        setItemActive("keypad-box", false);
        setItemActive("menu-box", true);
    }
    else if (item->itemName().left( 11 ) == "keypad-show") {
        // themed touchscreen keypad
        keypadVisible = true;
        manualLayout();
    }
    else if ( item->itemName().left( 11 ) == "keypad-hide" )
    {
        keypadVisible = false;
        manualLayout();
        clearDtmfDigits();
    }
    else if ( item->itemName() == "zero" )
    {
        dialNumbers("0");
    }
    else if(  item->itemName() == "one" )
    {
        dialNumbers("1");
    }
    else if ( item->itemName() == "two" )
    {
        dialNumbers("2");
    }
    else if ( item->itemName() == "three" )
    {
        dialNumbers("3");
    }
    else if ( item->itemName() == "four" )
    {
        dialNumbers("4");
    }
    else if ( item->itemName() == "five" )
    {
        dialNumbers("5");
    }
    else if ( item->itemName() == "six" )
    {
        dialNumbers("6");
    }
    else if ( item->itemName() == "seven" )
    {
        dialNumbers("7");
    }
    else if ( item->itemName() == "eight" )
    {
        dialNumbers("8");
    }
    else if ( item->itemName() == "nine" )
    {
        dialNumbers("9");
    }
    else if ( item->itemName() == "star" )
    {
        dialNumbers("*");
    }
    else if ( item->itemName() == "hash" )
    {
        dialNumbers("#");
    }
}

/*!
  \internal
  */
void ThemedCallScreenView::keyPressEvent(QKeyEvent *k)
{
    if (k->key() == Qt::Key_Flip) {
        QSettings cfg("Trolltech","Phone");
        cfg.beginGroup("FlipFunction");
        if (cfg.value("hangup").toBool()) {
            control->endAllCalls();
            hide();
        }
    } else if (k->key() == Qt::Key_Hangup || k->key() == Qt::Key_No) {
        if (control->isConnected() || control->isDialing() || control->hasIncomingCall())
            endCall();
        else
            emit hideCallScreen();
    } else if ((k->key() == Qt::Key_F28) && control->isConnected() && !control->hasIncomingCall()) {
        endCall();
    } else if (k->key() == Qt::Key_Call || k->key() == Qt::Key_Yes || k->key() == Qt::Key_F28) {
        if (!dtmfDigits.isEmpty()) {
            actionGsmSelected();
        } else {
            if ( control->hasIncomingCall() )
                emit acceptIncoming();
        }
    } else {
        k->ignore();
    }
}

/*!
  \internal
  */
void ThemedCallScreenView::showEvent( QShowEvent *e )
{
    if (!sourceLoaded())
        loadSource();
    if ( !updateTimer ) {
        updateTimer = new QTimer(this);
        connect(updateTimer, SIGNAL(timeout()), this, SLOT(updateAll()));
    }
    updateTimer->start(1000);
    m_taskManagerEntry->show();

    ThemedView::showEvent( e );
    manualLayout();
    QTimer::singleShot(0, this, SLOT(initializeMouseControlDialog()));
}

/*!
  \internal
  */
void ThemedCallScreenView::keyReleaseEvent(QKeyEvent *k)
{
    if (k->key() == Qt::Key_Flip && control->hasIncomingCall()) {
        QSettings cfg("Trolltech","Phone");
        cfg.beginGroup("FlipFunction");
        if ( cfg.value("answer").toBool() )
            emit acceptIncoming();
    }
}

/*!
  \internal
  */
bool ThemedCallScreenView::tryClose()
{
    if (listView && listView->selectionMode() == QAbstractItemView::SingleSelection) {
        setWindowTitle(tr("Calls"));
        setSelectMode(false);
        stateChanged();
        return false;
    } else if (control->hasIncomingCall()) {
        return false;
    } else {
        m_taskManagerEntry->hide();
        return true;
    }
}

/*!
    \internal
*/
void ThemedCallScreenView::endCall()
{
    emit hangupCall();
}

/*!
 *   \internal
 */
void ThemedCallScreenView::showEndCall()
{
    setItemActive("endcall", true);
}

/*!
  \internal
  */
void ThemedCallScreenView::hideEvent( QHideEvent * )
{
    if ( updateTimer )
        updateTimer->stop();

    if (secondaryCallScreen)
        secondaryCallScreen->hide();
}

/*!
  \internal
  */
bool ThemedCallScreenView::eventFilter(QObject *o, QEvent *e)
{
    if (o == listView) {
        if (e->type() == QEvent::KeyPress) {
            QKeyEvent *ke = (QKeyEvent *)e;
            if (listView->selectionMode() == QAbstractItemView::NoSelection) {
                if (ke->key() == Qt::Key_Up || ke->key() == Qt::Key_Down) {
                    return true;
                } else if (ke->key() == Qt::Key_Select) {
                    // gsm key select
                    if (!dtmfDigits.isEmpty() && gsmActionTimer && gsmActionTimer->isActive()) {
                        actionGsmSelected();
                        return true;
                    }
                    if ( incoming ) {
                        if(control->incomingCall().startTime().secsTo(QDateTime::currentDateTime()) >= 1)
                            emit acceptIncoming();
                    } else if (!ke->isAutoRepeat()) {
                        if (holdCount) {
                            control->unhold();
                        } else {
                            control->hold();
                        }
                    }
                    return true;
                } else if(ke->key() == Qt::Key_Back) {
                    if (control->hasIncomingCall()
                            && control->incomingCall().startTime().secsTo(QDateTime::currentDateTime()) >= 1) {
                        if ( actionMute->isVisible() )
                            muteRingSelected();
                        else
                            endCall();
                        return true;
                    } else if (!dtmfDigits.isEmpty()) {
                        clearDtmfDigits(true);
                        return true;
                    }
                }

                if (!ke->isAutoRepeat()) {
                    QString text = ke->text();
                    if ( !text.isEmpty() ) {
                        char ch = text[0].toLatin1();
                        if ( ( ch >= 48 && ch <= 57 )
                                || ch == 'p' || ch == 'P' || ch == '+' || ch == 'w'
                                || ch == 'W' || ch == '#' || ch == '*' || ch == '@' ) {
                            if( !dialNumbers( text ) ) {
                                // Show dialer
                                QtopiaServiceRequest sd("Dialer", "showDialer(QString)");
                                sd << text;
                                sd.send();
                            }
                        }
                    }
                }
            }
        } else if (e->type() == QEvent::Show) {
            grabMouse();
        } else if (e->type() == QEvent::FocusIn) {
            // required to show from here to give waitDlg focus and show no labels
            if ( showWaitDlg && waitDlg ) {
                waitDlg->show();
                waitDlg->setFocus();
                showWaitDlg = false;
            }
        }
    }

    return false;
}

/*!
  \internal
  */
void ThemedCallScreenView::setSelectMode(bool s)
{
    if (s) {
        listView->setSelectionMode(QAbstractItemView::SingleSelection);
        QSoftMenuBar::setLabel(listView, Qt::Key_Select, QSoftMenuBar::Select);
        CallItemModel *m = qobject_cast<CallItemModel *>(listView->model());
        for( int i = m->rowCount()-1; i >= 0; i--)
        {
            CallItemEntry *item = m->callItemEntry(m->index(i));
            if (item && item->call().state() == QPhoneCall::Connected) {
                listView->setCurrentIndex(m->index(i));
                if ( activeCount == 2 )
                    break;
            }
        }
    } else {
        QSoftMenuBar::setLabel(listView, Qt::Key_Select, QSoftMenuBar::NoLabel);
        listView->setSelectionMode(QAbstractItemView::NoSelection);
        QTimer::singleShot(0, listView, SLOT(clearSelection()));
    }
}

/* Reimplemented from ThemedView */
/*!
  \internal
  */
QWidget *ThemedCallScreenView::newWidget(ThemeWidgetItem* input, const QString& name)
{
    if( name == "callscreen" )  {
        Q_ASSERT(input->rtti() == ThemedView::List);
        CallItemListView * lv = new CallItemListView( input, this );
        if(m_model != 0)
            delete m_model;
        m_model = new CallItemModel( this, static_cast<ThemeListItem*>(input), this );
        lv->setModel(m_model);
        return lv;
    } else if( name == "callscreennumber" ) {
        return new QLineEdit( this );
    }
    return 0;
}

/*! \internal */
void ThemedCallScreenView::grabMouse()
{
    // lock touch screen
    if (!Qtopia::mousePreferred())
        PhoneThemedView::grabMouse();
}

/*! \internal */
void ThemedCallScreenView::releaseMouse()
{
    // unlock touch screen
    if (!Qtopia::mousePreferred())
        PhoneThemedView::releaseMouse();
}

void ThemedCallScreenView::initializeMouseControlDialog()
{
    // Do not use screen lock if touch screen only
    if (Qtopia::mousePreferred())
        return;

    if ( !m_mouseCtrlDlg ) {
        m_mouseCtrlDlg = new MouseControlDialog(this, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        connect( m_mouseCtrlDlg, SIGNAL(releaseMouse()), this, SLOT(releaseMouse()) );
        connect( m_mouseCtrlDlg, SIGNAL(grabMouse()), this, SLOT(grabMouse()) );
    }
}

/*! \internal */
void ThemedCallScreenView::mousePressEvent(QMouseEvent *e)
{
    // if touch screen is not locked no need to show unlock dialog
    if ( !QWidget::mouseGrabber() ) {
        PhoneThemedView::mousePressEvent(e);
        return;
    }

    // if touch screen only phone, the mouse control dialog need to grab mouse
    // so release the mouse before show the dialog.
    if ( Qtopia::mousePreferred() )
        releaseMouse();

    if ( m_mouseCtrlDlg )
        m_mouseCtrlDlg->show();
}

/*! \internal */
void ThemedCallScreenView::muteRingSelected()
{
    actionMute->setVisible(false);
    QtopiaServiceRequest req("Ringtone", "muteRing()");
    req.send();
    QSoftMenuBar::setLabel(listView, Qt::Key_Back, "phone/reject", tr("Send Busy"));
}

/*! \internal */
void ThemedCallScreenView::callConnected(const QPhoneCall &)
{
    setItemActive("answer", false);
    QTimer::singleShot(2000, this, SLOT(showEndCall())); // setItemActive("endcall", true);
    setItemActive("resume", false);
    setItemActive("sendbusy", false);
    setItemActive("hold", true);
}

/*! \internal */
void ThemedCallScreenView::callDropped(const QPhoneCall &)
{
    if (control->hasActiveCalls()) {
        setItemActive("hold", true);
        setItemActive("endcall", true);
        setItemActive("resume", false);
        setItemActive("answer", false);
        setItemActive("sendbusy", false);
    } else if (control->hasCallsOnHold()) {
        setItemActive("resume", true);
        setItemActive("answer", false);
        setItemActive("hold", false);
        setItemActive("endcall", true);
        setItemActive("sendbusy", false);
    } else {
        setItemActive("menu-box", false);
    }
}

/*! \internal */
void ThemedCallScreenView::callDialing(const QPhoneCall &)
{
    setItemActive("menu-box", true);
    setItemActive("answer", false);
    setItemActive("endcall", true);
    setItemActive("resume", false);
    setItemActive("sendbusy", false);
    setItemActive("hold", true);
}

/*! \internal */
void ThemedCallScreenView::callIncoming(const QPhoneCall &)
{
    if ( !waitDlg )
        waitDlg = UIFactory::createDialog( "DelayedWaitDialog", this );
    if ( !waitDlg ) {
        qLog(Component) << "DelayedWaitDlg component not available";
        return;
    }
    QMetaObject::invokeMethod( waitDlg, "setText", Qt::DirectConnection, 
                        Q_ARG(QString, tr("Incoming Call...")) );
    QtopiaInputEvents::addKeyboardFilter( new CallScreenKeyboardFilter );
    QTimer::singleShot( 1000, this, SLOT(interactionDelayTimeout()) );

    QSoftMenuBar::setLabel(waitDlg, Qt::Key_Context1, QSoftMenuBar::NoLabel);
    QSoftMenuBar::setLabel(waitDlg, Qt::Key_Select, QSoftMenuBar::NoLabel);
    QSoftMenuBar::setLabel(waitDlg, Qt::Key_Back, QSoftMenuBar::NoLabel);

    if ( isVisible() )
        waitDlg->show();
    else
        showWaitDlg = true;

    setItemActive("menu-box", true);
    setItemActive("hold", false);
    setItemActive("endcall", false);
    setItemActive("resume", false);
    setItemActive("answer", true);
    setItemActive("sendbusy", true);
}

/*!
  \internal
  */
void ThemedCallScreenView::rejectModalDialog()
{
    // Last resort.  We shouldn't have modal dialogs in the server, but
    // just in case we need to get rid of them when a call arrives.  This
    // is a bad thing to do, but far less dangerous than missing a call.
    // XXX Known modals:
    //  - category edit dialog
    QWidgetList list = QApplication::topLevelWidgets();
    QList<QPointer<QDialog> > dlgsToDelete;

    foreach(QWidget *w, list)
        if (w->isVisible() && w->inherits("QDialog"))
            dlgsToDelete.append((QDialog*)w);

    foreach(QPointer<QDialog> d, dlgsToDelete) {
        if (!d)
            continue;

        if (d->testAttribute(Qt::WA_ShowModal)) {
            qWarning("Rejecting modal dialog: %s", d->metaObject()->className());
            d->reject();
        } else {
            qWarning("Hiding non-modal dialog: %s", d->metaObject()->className());
            d->hide();
        }
    }
}

/*!
  \internal
*/
void ThemedCallScreenView::interactionDelayTimeout()
{
    if ( !waitDlg )
        return;
    waitDlg->hide();
    QtopiaInputEvents::removeKeyboardFilter();

    stateChanged();
}

/*! \internal */
void ThemedCallScreenView::showProgressDlg()
{
    if ( !waitDlg )
        waitDlg = UIFactory::createDialog( "DelayedWaitDialog", this );
    if ( !waitDlg ) {
        qLog(Component) << "DelayedWaitDlg component not available";
        return;
    }
    QMetaObject::invokeMethod( waitDlg, "setText", Qt::DirectConnection, 
                        Q_ARG(QString, tr("Please wait...")) );
    QMetaObject::invokeMethod( waitDlg, "setDelay", Qt::DirectConnection,
                        Q_ARG(int, 500) );
    waitDlg->show();
}

/*! \internal */
void ThemedCallScreenView::hideProgressDlg()
{
    if ( waitDlg )
        waitDlg->hide();
}

QTOPIA_REPLACE_WIDGET(QAbstractCallScreen, ThemedCallScreen);

#include "callscreen.moc"
