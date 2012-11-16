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

#include "ringcontrol.h"
#include "ringtoneservice.h"
#include <qtranslatablesettings.h>
#include "dialercontrol.h"
#include "servercontactmodel.h"
#include <qcontactmodel.h>
#include <qcontact.h>
#include <qtopialog.h>
#include <qvibrateaccessory.h>
#include <qtimer.h>
#include <qsound.h>
#include <QtopiaIpcEnvelope>

#ifdef MEDIA_SERVER
#include <qsoundcontrol.h>
#define SERVER_CHANNEL "QPE/MediaServer"
#else
#include <qsoundqss_qws.h>
#endif

#ifdef Q_WS_X11
#include <qcopchannel_x11.h>
#else
#include <qcopchannel_qws.h>
#endif

#include "systemsuspend.h"

static const int NotificationTimeout = 15000;

class RingtoneServiceProxy : public RingtoneService
{
Q_OBJECT
public:
    RingtoneServiceProxy(QObject * = 0);
    virtual ~RingtoneServiceProxy() {}

protected:
    virtual void startMessageRingtone();
    virtual void stopMessageRingtone();
    virtual void startRingtone(const QString& fileName);
    virtual void stopRingtone(const QString& fileName);
    virtual void muteRing();

signals:
    void doStartMessageRingtone();
    void doStopMessageRingtone();
    void doStartRingtone(const QString& fileName);
    void doStopRingtone(const QString& fileName);
    void doMuteRing();
};

// define RingtoneServiceProxy
RingtoneServiceProxy::RingtoneServiceProxy(QObject *parent)
    : RingtoneService(parent)
{
}

void RingtoneServiceProxy::startMessageRingtone()
{
    emit doStartMessageRingtone();
}

void RingtoneServiceProxy::stopMessageRingtone()
{
    emit doStopMessageRingtone();
}

void RingtoneServiceProxy::startRingtone( const QString& fileName )
{
    emit doStartRingtone(fileName);
}

void RingtoneServiceProxy::stopRingtone( const QString& fileName )
{
    emit doStopRingtone(fileName);
}

void RingtoneServiceProxy::muteRing()
{
    emit doMuteRing();
}


// declare RingControlPrivate
class RingControlPrivate
{
public:
    RingControlPrivate() :
        callEnabled(true),
        msgEnabled(true),
        msgTid(0),
        vrbTid(0),
        msgRingTime(10000),
        vibrateTime(1600),
        currentRingSource(RingControl::NotRinging),
        lastRingVolume(0),
        ringVolume(0),
        profileManager(0),
        priorityPlay(false),
        videoTone(false),
        videoToneFailed(false),
        messageWaiting(false)
#ifdef MEDIA_SERVER
        , soundcontrol(0)
#elif defined(Q_WS_QWS)
        , soundclient(0)
#endif
        , videoAdaptor(0)
    {}

    bool callEnabled;
    bool msgEnabled;

    int msgTid;
    int vrbTid;
    int msgRingTime;
    int vibrateTime;
    QString curRingTone;
    QPhoneProfile::AlertType curAlertType;
    RingControl::RingType currentRingSource;
    QTime ringtime;
    int lastRingVolume;
    int ringVolume;
    bool vibrateActive;
    QPhoneProfileManager *profileManager;
    bool priorityPlay;
    bool videoTone;
    bool videoToneFailed;
    bool messageWaiting;
#ifdef MEDIA_SERVER
    QSoundControl *soundcontrol;
#elif defined(Q_WS_QWS)
    QWSSoundClient *soundclient;
#endif
    QtopiaIpcAdaptor *videoAdaptor;
};

/*!
  \class RingControl
    \inpublicgroup QtTelephonyModule
  \brief The RingControl class controls the system ring for incoming calls and
         messages.
  \ingroup QtopiaServer::Telephony

  The RingControl provides a Qt Extended Server Task.  Qt Extended Server Tasks are
  documented in full in the QtopiaServerApplication class documentation.

  \table
  \row \o Task Name \o RingControl
  \row \o Interfaces \o RingControl
  \row \o Services \o None
  \endtable

  The RingControl class plays ring tones and enables vibration on incoming calls
  and messages.  The specifics of the tones and vibrations generated are
  controlled by the active QPhoneProfile.

  The RingControl class only supports a single simultaneous ring tone.  If both
  an incoming call and an incoming message are received together, the call ring
  takes priority and the message ring is discarded.  Likewise, if a message ring
  is in progress when an incoming call is received, the message ring is stopped
  and the phone ring commenced.

  The RingControl class utilizes video ringtones if the VideoRingtone task is deployed.
  The communications between the two classes is based on messages on the \c QPE/VideoRingtone
  QCop channel. For more details see VideoRingtone.

  This class is part of the Qt Extended server and cannot be used by other Qt Extended applications.

  \sa VideoRingtone
*/

/*!
  Sets whether call ring control is \a enabled.

  \sa callRingEnabled()
 */
void RingControl::setCallRingEnabled(bool enabled)
{
    d->callEnabled = enabled;
}

/*!
  Returns true if call ring control is enabled, otherwise false.  If enabled,
  the RingControl class will play ring tones and vibrate subject to the rules
  for call rings outlined above.  If disabled, the RingControl class will not
  perform any ring control for calls.

  By default, call ring control is enabled.
 */
bool RingControl::callRingEnabled() const
{
    return d->callEnabled;
}

/*!
  Sets whether message ring control is \a enabled.

  \sa messageRingEnabled()
 */
void RingControl::setMessageRingEnabled(bool enabled)
{
    d->msgEnabled = enabled;
}

/*!
  Returns true if message ring control is enabled, otherwise false.  If enabled,
  the RingControl class will play ring tones and vibrate subject to the rules
  for message rings outlined above.  If disabled, the RingControl class will not
  perform any ring control for messages.

  By default, message ring control is enabled.
 */
bool RingControl::messageRingEnabled() const
{
    return d->msgEnabled;
}

/*!
  Returns the in-progress ring tpe, or NotRinging if there is no in-progress
  ring.
 */
RingControl::RingType RingControl::ringType() const
{
    return d->currentRingSource;
}

/*!
  Sets the \a volume at which the rings will be played.  \a volume may be from
  0 (silent) to 5 (loudest).

  The ring volume only effects non-ascending rings.  Ascending rings always
  commence at the lowest volume and slowly increase to the maximum volume.
  Generally setVolume() should not be called directly as it will be overridden
  by any change to the active QPhoneProfile.
 */
void RingControl::setVolume(int volume)
{
    d->ringVolume = volume;
}

/*!
  \enum RingControl::RingType

  The RingType enumeration represents the current in-progress ring type.

  \value NotRinging There is no active ring.
  \value Call The active ring is for an incoming call.
  \value Msg The active ring is for a newly received message.
 */

int volmap[6] = {
    0,
    20,
    40,
    60,
    80,
    100,
};

/*!
  \internal

  Create a new RingControl instance with the specified \a parent.
 */
RingControl::RingControl(QObject *parent)
: QObject(parent)
{
    d = new RingControlPrivate;
    d->videoAdaptor = new QtopiaIpcAdaptor( "QPE/VideoRingtone", this );
    QtopiaIpcAdaptor::connect( d->videoAdaptor, MESSAGE(videoRingtoneFailed()),
                               this, SLOT(videoRingtoneFailed()) );

    d->profileManager = new QPhoneProfileManager(this);
    QPhoneProfile prof = d->profileManager->activeProfile();
    setVolume(prof.volume());
    setMsgRingTime(prof.msgAlertDuration());

    connect(DialerControl::instance(), SIGNAL(stateChanged()),
            this, SLOT(stateChanged()));

    QObject::connect(d->profileManager,
                     SIGNAL(activeProfileChanged(QPhoneProfile)),
                     this,
                     SLOT(profileChanged()));

    QObject::connect(qtopiaTask<SystemSuspend>(), SIGNAL(systemSuspending()),
            this, SLOT(stopMessageAlert()));

    // Implement ringtone service
    RingtoneServiceProxy *ringtoneServiceProxy = new RingtoneServiceProxy(this);
    connect(ringtoneServiceProxy, SIGNAL(doStartMessageRingtone()),
            this, SLOT(startMessageRingtone()));
    connect(ringtoneServiceProxy, SIGNAL(doStopMessageRingtone()),
            this, SLOT(stopMessageRingtone()));
    connect(ringtoneServiceProxy, SIGNAL(doStartRingtone(QString)),
            this, SLOT(startRingtone(QString)));
    connect(ringtoneServiceProxy, SIGNAL(doStopRingtone(QString)),
            this, SLOT(stopRingtone(QString)));
    connect(ringtoneServiceProxy, SIGNAL(doMuteRing()),
            this, SLOT(muteRing()));

    d->curAlertType = QPhoneProfile::Off;
}

/*!
  \internal
  Destroy the RingControl.
 */
RingControl::~RingControl()
{
    delete d;
    d = 0;
}

/*!
  Sets the \a duration in milliseconds of the vibration period.  When vibration
  is active, the vibration will toggle on for \a duration then off for
  \a duration milliseconds.
 */
void RingControl::setVibrateDuration(int duration)
{
    Q_ASSERT(duration >= 0);
    d->vibrateTime = duration;
}

/*!
  Return the duration of the vibration period in milliseconds.
 */
int RingControl::vibrateDuration() const
{
    return d->vibrateTime;
}

/*!
  Set the message ring time to \a time milliseconds.
 */
void RingControl::setMsgRingTime(int time)
{
    Q_ASSERT(time >= 0);
    d->msgRingTime = time;
}

/*!
  Return the message ring time, in milliseconds.
 */
int RingControl::msgRingTime() const
{
    return d->msgRingTime;
}

/*!
  Return the time, in milliseconds, the current ring has been in progress.  If
  there is no in-progress ring, returns 0.
 */
int RingControl::ringTime() const
{
    return d->currentRingSource == NotRinging ?0:d->ringtime.elapsed();
}

void RingControl::startRinging(RingType t)
{
    stopRing();

    if(t == Call && !d->callEnabled)
        return;
    else if(t == Msg && !d->msgEnabled)
        return;

    if(qLogEnabled(QtopiaServer)) {
        QString type;
        switch(t) {
            case NotRinging:
                type = "NotRinging";
                break;
            case Call:
                type = "Call";
                break;
            case Msg:
                type = "Message";
                break;
        }
        qLog(QtopiaServer) << "RingControl: Ringing" << type;
    }

    if(t == NotRinging)
        return;

    d->ringtime.start();

    QPhoneProfile profile = d->profileManager->activeProfile();
    QString ringToneDoc;

    // try contact ringtone

    if (t == Call) {

        // try personalized ring tone
        ringToneDoc = findRingTone();
        // try profile ring tone
        // try video ring tone first.
        if (ringToneDoc.isEmpty()) {
            ringToneDoc = profile.videoTone().fileName();

            if (!ringToneDoc.isEmpty()) {
                d->videoTone = true;
            }
        }

        // try profile ring tone
        if (ringToneDoc.isEmpty() || d->videoToneFailed) {
            ringToneDoc = profile.callTone().fileName();
            d->videoTone = false;
            d->videoToneFailed = false;
        }

        // last resort, fall back to system ring tone
        if (ringToneDoc.isEmpty())
            ringToneDoc = profile.systemCallTone().fileName();

        d->curRingTone = ringToneDoc;

        d->curAlertType = profile.callAlert();
    }
    else if (t == Msg)
    {
        d->curAlertType = profile.msgAlert();

        if (d->curAlertType == QPhoneProfile::Continuous ||
            d->curAlertType == QPhoneProfile::Ascending)
        {
            d->msgTid = startTimer(msgRingTime());
        }

        ringToneDoc = profile.messageTone().fileName();

        // fall back if above settings lead to non-existent ringtone.
        if (ringToneDoc.isEmpty())
            ringToneDoc = profile.systemMessageTone().fileName();

        d->curRingTone = ringToneDoc;
    }

    if (profile.vibrate())
    {
        QVibrateAccessory vib;
        vib.setVibrateNow( true );
        d->vibrateActive = true;
        d->vrbTid = startTimer(vibrateDuration());
    }

    if(d->curAlertType == QPhoneProfile::Ascending)
        d->lastRingVolume = 1;
    else
        d->lastRingVolume = d->ringVolume;

    d->currentRingSource = t;

    if(d->lastRingVolume && d->curAlertType != QPhoneProfile::Off) {
        initSound();
#ifdef MEDIA_SERVER
        if ( !d->videoTone ) {
            if(d->soundcontrol && d->soundcontrol->sound()->fileName() != ringToneDoc) {
                delete d->soundcontrol->sound();
                delete d->soundcontrol;
                d->soundcontrol = 0;
            }
            if (!d->soundcontrol) {
                d->soundcontrol = new QSoundControl(new QSound(d->curRingTone));
                connect(d->soundcontrol, SIGNAL(done()), this, SLOT(nextRing()) );
          }
            d->soundcontrol->setPriority(QSoundControl::RingTone);
            d->soundcontrol->setVolume(volmap[d->lastRingVolume]);

            d->soundcontrol->sound()->play();
        } else {
            qLog(Media) << "Video ringtone selected" << d->curRingTone;
            QtopiaIpcSendEnvelope env = d->videoAdaptor->send( MESSAGE(playVideo(QString)) );
            env << d->curRingTone;
        }
#elif defined(Q_WS_QWS)
        if(d->soundclient)
            d->soundclient->play(0, d->curRingTone,
                                 volmap[d->lastRingVolume],
                                 QWSSoundClient::Priority);
#endif
    }

    emit ringTypeChanged(d->currentRingSource);
}

QString RingControl::findRingTone()
{
    QString ringTone;
    if(DialerControl::instance()->hasIncomingCall()) {
        QPhoneCall call = DialerControl::instance()->incomingCall();
        QString numberOrName = call.number();
        QContact cnt;
        QContactModel *m = ServerContactModel::instance();
        if (!call.contact().isNull()) {
            cnt = m->contact(call.contact());
        } else if (!numberOrName.isEmpty()) {
            cnt = m->matchPhoneNumber(numberOrName);
        }

        if (!cnt.uid().isNull()) {
            numberOrName = cnt.label();

            // video ringtone
            ringTone = cnt.customField( "videotone" );
            if ( !ringTone.isEmpty() ) {
                d->videoTone = true;
            } else { // normal ringtone
                ringTone = cnt.customField( "tone" );
                d->videoTone = false;
            }

            if ( ringTone.isEmpty() ) {
                // check if the contacts category has a ringtone
                QList<QString> catList = cnt.categories();
                if ( catList.count() ) {
                    QCategoryManager catManager;
                    ringTone = catManager.ringTone( catList.at( 0 ) );
                }
                d->videoTone = false;
            }
        }
    }

    return ringTone;
}

/*!
  Stop all ringing and vibration.  Does nothing if no ring is in progress.
  */
void RingControl::stopRing( )
{
    if(d->msgTid) {
        killTimer(d->msgTid);
        d->msgTid = 0;
        setSoundPriority(false);
    }

    if(d->vrbTid) {
        killTimer(d->vrbTid);
        d->vrbTid = 0;
        d->vibrateActive = false;
        QVibrateAccessory vib;
        vib.setVibrateNow( false );
    }

    if (d->curAlertType != QPhoneProfile::Off)
    {
#ifdef MEDIA_SERVER
        if(d->soundcontrol) {
            d->soundcontrol->sound()->stop();
            delete d->soundcontrol->sound();
            delete d->soundcontrol;
            d->soundcontrol = 0;
        }
#elif defined(Q_WS_QWS)
        if (d->soundclient) d->soundclient->stop(0);
#endif
        if (d->curAlertType == QPhoneProfile::Once)
        {
            // stopping a once off ring
            setSoundPriority(false);
        }

        d->curAlertType = QPhoneProfile::Off;
    }

    if (d->currentRingSource != NotRinging)
    {
        d->currentRingSource = NotRinging;
        emit ringTypeChanged(NotRinging);
    }
}

/*!
    Stop the message alert sound.  Does nothing if no message alert
    is in progress.
*/
void RingControl::stopMessageAlert()
{
    if (d->currentRingSource == Msg) {
        qLog(PowerManagement) << "system suspended, stop message alert";
        stopRing();
    }
}

/*!
  \fn void RingControl::ringTypeChanged(RingControl::RingType type)

  Emitted whenever the current ring type changes.  \a type will be the new
  value.
 */

void RingControl::profileChanged()
{
    QPhoneProfile prof = d->profileManager->activeProfile();
    setVolume(prof.volume());
    setMsgRingTime(prof.msgAlertDuration());
}

// should eventually timeout.
void RingControl::nextRing()
{
    if (d->currentRingSource != NotRinging &&
        d->curAlertType != QPhoneProfile::Once)
    {
        if (d->curAlertType == QPhoneProfile::Ascending && d->lastRingVolume < 5)
            d->lastRingVolume++;

#ifdef MEDIA_SERVER
        d->soundcontrol->setVolume(volmap[d->lastRingVolume]);
        d->soundcontrol->sound()->stop();
        d->soundcontrol->sound()->play();
#elif defined(Q_WS_QWS)
        if (d->soundclient)
        {
            d->soundclient->play(0,
                                 d->curRingTone,
                                 volmap[d->lastRingVolume],
                                 QWSSoundClient::Priority);
        }
#endif
    }
#ifdef MEDIA_SERVER
    else {
        if (d->soundcontrol)
        {
            if(d->soundcontrol) {
                d->soundcontrol->sound()->stop();
                disconnect(d->soundcontrol, SIGNAL(done()), this,
                        SLOT(nextRing()) );
                delete d->soundcontrol->sound();
                delete d->soundcontrol;
                d->soundcontrol = 0;
            }
        }
    }
#endif

    // we need to stop the vibration timer when using ring once
    if (d->curAlertType == QPhoneProfile::Once)
    {
        stopRing();
    }
}

/*!
  Play the \a soundFile.  The sound file will be played at the current ring
  volume and terminated if another ring even occurs.  Sounds may only be played
  through this API if the ring type is NotRinging.
 */
void RingControl::playSound(const QString &soundFile)
{
    if(NotRinging != ringType())
        return;

#ifdef MEDIA_SERVER
    if(d->soundcontrol) {
        delete d->soundcontrol->sound();
        delete d->soundcontrol;
    }

    d->soundcontrol = new QSoundControl(new QSound(soundFile));

    connect(d->soundcontrol, SIGNAL(done()), this, SLOT(nextRing()) );

    d->soundcontrol->setPriority(QSoundControl::RingTone);
    d->soundcontrol->setVolume(volmap[d->ringVolume]);
    d->soundcontrol->sound()->play();
#elif defined(Q_WS_QWS)
    if(d->soundclient)
        d->soundclient->play(0, soundFile, volmap[d->ringVolume],
                             QWSSoundClient::Priority);
#endif
}

void RingControl::initSound()
{
#if !defined(MEDIA_SERVER) && defined(Q_WS_QWS)
    if (!d->soundclient)
        d->soundclient = new QWSSoundClient(this);

    // muteRing might have disconnected this signal,
    // connect here whenever init sound.
    connect(d->soundclient, SIGNAL(soundCompleted(int)),
            this, SLOT(nextRing()));
#endif
}

/*!
  Stops audible ringing, but vibration will continue if it was already in
  progress.  To stop both audible ringing and vibration, use stopRing().
*/
void RingControl::muteRing()
{
#ifdef MEDIA_SERVER
    if(d->soundcontrol) {
        d->soundcontrol->sound()->stop();
        disconnect(d->soundcontrol, SIGNAL(done()), this, SLOT(nextRing()) );
    }
#elif defined(Q_WS_QWS)
    if (d->soundclient) {
        d->soundclient->stop(0);
        disconnect(d->soundclient, SIGNAL(soundCompleted(int)),
                this, SLOT(nextRing()));
    }
#endif
}

void RingControl::timerEvent(QTimerEvent *e)
{
    if (e->timerId() == d->msgTid) {
        if(ringType() == RingControl::Msg)
            stopRing();
    } else if (e->timerId() == d->vrbTid) {
        QVibrateAccessory vib;
        d->vibrateActive = !d->vibrateActive;
        vib.setVibrateNow( d->vibrateActive );
    }
}

void RingControl::stateChanged()
{
    DialerControl*  dialerControl = DialerControl::instance();

    if (dialerControl->hasIncomingCall() &&
            !dialerControl->hasActiveCalls() &&
            !dialerControl->hasCallsOnHold())
    {
        if (ringType() != RingControl::Call)
            startRinging(Call);
    }
    else
    {
        if (ringType() == RingControl::Call) {
#ifdef MEDIA_SERVER
            if ( d->videoTone )
                QtopiaIpcSendEnvelope env = d->videoAdaptor->send(MESSAGE(stopVideo()));
#endif
            stopRing();
        }

        if (!dialerControl->allCalls().count() &&
                d->messageWaiting ) {
            d->messageWaiting = false;
            startRinging(Msg);
        }
    }

    initSound();

    if (dialerControl->isConnected() ||
        dialerControl->isDialing() ||
        ringType() != RingControl::NotRinging)
    {
        setSoundPriority(true);
    }
    else
    {
        setSoundPriority(false);
    }
}

void RingControl::setSoundPriority(bool priorityPlay)
{
    if (priorityPlay != d->priorityPlay)
    {
#ifndef MEDIA_SERVER
        if (d->soundclient)
            d->soundclient->playPriorityOnly(priorityPlay);
#endif
        d->priorityPlay = priorityPlay;
    }
}

void RingControl::videoRingtoneFailed()
{
#ifdef MEDIA_SERVER
    qLog(Media) << "Video ringtone failed, fall back to normal ringtone";
    d->videoToneFailed = true;
    // FIXME fall-back policy needed
    //startRinging(Call);
#endif
}

void RingControl::startMessageRingtone()
{
    // Doesn't apply if already ringing
    if (ringType() == RingControl::NotRinging &&
            !DialerControl::instance()->hasActiveCalls() &&
            !DialerControl::instance()->hasCallsOnHold()) {
        setSoundPriority(true);
        startRinging(Msg);

        // Ensure that we eventually stop the ring
        QTimer::singleShot(NotificationTimeout, this, SLOT(stopMessageAlert()));
    } else {
        d->messageWaiting = true;
    }
}

void RingControl::stopMessageRingtone()
{
    if (d->currentRingSource == Msg) {
        stopRing();
    }
}

void RingControl::startRingtone( const QString& fileName )
{
    playSound(fileName);
}

void RingControl::stopRingtone( const QString& fileName )
{
#ifdef MEDIA_SERVER
    if (d->soundcontrol) {
        // Check if the same ringtone is still the most recently played
        if (QSound* sound = d->soundcontrol->sound())
            if (sound->fileName() == fileName)
                sound->stop();
    }
#elif defined(Q_WS_QWS)
    Q_UNUSED(fileName);
    if (d->soundclient) {
        // No way to know that this is the same ringtone...
        d->soundclient->stop(0);
        disconnect(d->soundclient, SIGNAL(soundCompleted(int)),
                this, SLOT(nextRing()));
    }
#endif
}

QTOPIA_TASK(RingControl, RingControl);
QTOPIA_TASK_PROVIDES(RingControl, RingControl);

#include "ringcontrol.moc"
