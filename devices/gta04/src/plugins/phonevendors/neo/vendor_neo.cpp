/****************************************************************************
**
** This file is part of the Qt Extended Opensource Package.
**
** Copyright (C) 2011 Radek Polak <psonek2@seznam.cz>
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
#include <qmodemllindicators.h>
#include <QProcess>
#include <QTimer>
#include <QFile>
#include <QTextStream>
#include <QSettings>

#include <stdio.h>
#include <stdlib.h>
#include <linux/input.h>
#include <alsa/asoundlib.h>

#include <qled.h>
#include <qmodemcallvolume.h>
#include <qmodemsiminfo.h>
#include <qmodemcellbroadcast.h>

#define CLCC_POLL_INTERVAL 1000

// Call provider for GTM601 modem. Currently there is no way how to detect
// remote party hangup so during calls we do CLCC polling to detect it.
NeoCallProvider::NeoCallProvider(NeoModemService * service)
:  QModemCallProvider(service)
    , clccTimer(this)
{
    modemService = service;
    setUseMissedTimer(false);
    setUseDetectTimer(false);

    clccTimer.setSingleShot(true);
    connect(&clccTimer, SIGNAL(timeout()), this, SLOT(doClcc()));
}

NeoCallProvider::~NeoCallProvider()
{
}

bool NeoCallProvider::hasRepeatingRings() const
{
    return true;
}

 // Use the AT+CHUP command to abort outgoing calls, instead of AT+CHLD=1. Also
 // use send() without waiting like chat() does.
 void NeoCallProvider::abortDial(uint id, QPhoneCall::Scope scope)
{
    Q_UNUSED(id);
    Q_UNUSED(scope);
    atchat()->send("AT+CHUP");
}

void NeoCallProvider::ringing(const QString &, const QString &, uint)
{
    llIndicatorsRinging();      // turn on green led
    doClcc();                   // start CLCC polling
}

void NeoCallProvider::clcc(bool, const QAtResult & result)
{
    QModemCall *ic = incomingCall();
    bool icMissing = true;

    int count = 0;

    // Find the current state of the call in the AT+CLCC results.
    QAtResultParser parser(result);
    while (parser.next("+CLCC:")) {
        uint id = parser.readNumeric();
        parser.readNumeric();   // dir
        uint state = parser.readNumeric();
        uint mode = parser.readNumeric();
        parser.readNumeric();   // mpty
        QString number = QAtUtils::decodeNumber(parser);
        QString callType = resolveCallMode(mode);
        //qDebug() << "=============== CLCC id=" << id << ", state =" << state <<
        //    ", mode=" << mode << ", number=" << number << "callType=" << callType;

        if (state == 4) {       // incoming
            if (ic && ic->number() == number) {
                icMissing = false;  // still ringing
            } else {            // new incoming call
                QModemCallProvider::ringing(number, callType, id);
                announceCall();
                clccTimer.start(CLCC_POLL_INTERVAL);
                return;
            }
        }
        count++;
    }

    // We still have call in incoming state
    if (ic) {
        if (icMissing) {
            qLog(Modem) << "Reporting missed call.";    // but it's missing in CLCC list
            ic->setState(QPhoneCall::Missed);   // make the call missed
            return;
        }
        clccTimer.start(CLCC_POLL_INTERVAL);    // continue with polling
        return;
    }
    // We still have some connected calls
    if (count > 0) {
        clccTimer.start(CLCC_POLL_INTERVAL);    // continue with polling
        return;
    }

    qLog(Modem) << "No more calls left";

    QList < QPhoneCallImpl * >list = calls();
    QList < QPhoneCallImpl * >::ConstIterator it;
    for (it = list.begin(); it != list.end(); ++it) {
        (*it)->setState(QPhoneCall::HangupRemote);
    }
}

void NeoCallProvider::doClcc()
{
    if (clccTimer.isActive()) {
        return;                 // CLCC is already running
    }
    atchat()->chat("AT+CLCC", this, SLOT(clcc(bool, QAtResult)));
}

NeoModemService::NeoModemService
    (const QString & service, QSerialIODeviceMultiplexer * mux,
     QObject * parent)
:  QModemService(service, mux, parent)
    , inputEvent("/dev/input/incoming")
    , inputNotifier(0)
{
    qDebug() << "Gta04ModemService::constructor";

    // Turn on dynamic signal quality notifications.
    // Register for "_OSIGQ" notifications to get signal quality updates.
    primaryAtChat()->registerNotificationType
        ("_OSIGQ:", this, SLOT(sigq(QString)));

    // Determine AT_OPSYS value - by default disable UMTS, use only GSM 0,2
    // this can help reduce modem reenumerations.
    // Some references:
    // see [1] at the end of this source file
    // http://lists.goldelico.com/pipermail/gta04-owner/2013-August/004891.html
    // http://lists.goldelico.com/pipermail/gta04-owner/2013-September/004939.html
    QSettings cfg("Trolltech", "Modem");
    cfg.beginGroup("OPSYS");
    QString opsys = cfg.value("Value", "AT_OPSYS=0,2").toString();
    qLog(Modem) << "OPSYS value:" << opsys;

    chat("AT+CSCS=\"GSM\"");    // GSM encoding
    chat("AT_OSQI=1");          // unsolicited reporting of antenna signal strength, e.g. "_OSIGQ: 3,0"
    chat("AT_OPCMENABLE=1");    // enable the PCM interface for voice calls
    chat(opsys);

    // Modem input device - reports keys when modem generates interrupt (e.g.
    // on incoming call or sms).
    if(inputEvent.open(QIODevice::ReadOnly)) {
        inputNotifier = new QSocketNotifier(inputEvent.handle(), QSocketNotifier::Read, this);
        connect(inputNotifier, SIGNAL(activated(int)), this, SLOT(handleInputEvent()));
    }
    else {
        qWarning() << "Gta04ModemService: failed to open " << inputEvent.fileName() << ": " << inputEvent.errorString();
    }
}

NeoModemService::~NeoModemService()
{
    if(inputNotifier) {
        delete inputNotifier;
        inputNotifier = 0;
    }
    inputEvent.close();
}

void NeoModemService::initialize()
{
    // Disable cell broadcast, GTA04 modem probably does not know AT+CSCB commands
    suppressInterface < QCellBroadcast > ();

    if (!callProvider()) {
        neoCallProvider = new NeoCallProvider(this);
        setCallProvider(neoCallProvider);
    }

    if (!supports < QVibrateAccessory > ())
        addInterface(new NeoVibrateAccessory(this));

    QModemService::initialize();

    // Disable signal and battery polling, just do initial signal check
    indicators()->setPolling(false, true, false);
}

void NeoModemService::sigq(const QString & msg)
{
    // Automatic signal quality update, in the range 0-31, e.g. _OSIGQ: 7,0
    uint posn = 8;
    uint rssi = QAtUtils::parseNumber(msg, posn);
    indicators()->setSignalQuality((int)rssi, 31);
}

void NeoModemService::reset()
{
    qLog(Modem) << "Gta04ModemService::reset()";
}

void NeoModemService::suspend()
{
    qLog(Modem) << " Gta04ModemService::suspend()";
    //chat("AT_OSQI=0");          // unsolicited reporting of antenna signal strength, e.g. "_OSIGQ: 3,0"

    primaryAtChat()->suspend();
    QSerialIODevice *port = multiplexer()->channel("primary");
    port->close();

    suspendDone();
}

void NeoModemService::wake()
{
    qLog(Modem) << " Gta04ModemService::wake()";

    QSerialIODevice *port = multiplexer()->channel("primary");
    port->open(QIODevice::ReadWrite);
    primaryAtChat()->resume();

    neoCallProvider->doClcc();
    post( "modemresumed" );

    //chat("AT_OSQI=1");          // unsolicited reporting of antenna signal strength, e.g. "_OSIGQ: 3,0"
    wakeDone();
}

void NeoModemService::handleInputEvent()
{
    qLog(Modem) << "NeoModemService::handleInputEvent()";
    
    // Read all input data from the file
    char buf[sizeof(input_event) * 32];
    struct input_event *ev = (struct input_event *) buf;
    int n = read(inputEvent.handle(), buf, sizeof(buf));
    
    while(n >= (int)(sizeof(input_event))) {
        if(ev->type == EV_KEY && ev->code == KEY_UNKNOWN) {
            // Set fast blinking on missed calls led
            qLedSetCall(qLedAttrBrightness(), qLedMaxBrightness());
            qLedSetCall(qLedAttrTrigger(), "timer");
            qLedSetCall(qLedAttrDelayOff(), "1024");
            qLedSetCall(qLedAttrDelayOn(), "64");
            break;
        }
        ev++;
        n -= sizeof(input_event);
    }
}

bool NeoModemService::supportsAtCced()
{
    return false;
}

// Open GTA04 vibrate device
static int openRumble()
{
    int fd = open("/dev/input/rumble", O_RDWR);
    if (fd > 0)
        return fd;

    perror("rumble open");
    return -1;
}

// Upload vibrate effect, returns effect id or -1 on error
static qint16 uploadEffect(int fd, quint16 strength, int timeoutMs)
{
    struct ff_effect e;

    // Define the new event
    e.type = FF_RUMBLE;
    e.id = -1;
    e.direction = 0;
    e.trigger.button = 0;
    e.trigger.interval = 0;
    e.replay.length = timeoutMs ? timeoutMs : 3000;
    e.replay.delay = 0;
    e.u.rumble.strong_magnitude = strength;
    e.u.rumble.weak_magnitude = 0;

    // Write the event
    if (ioctl(fd, EVIOCSFF, &e) == -1) {
        perror("rumble upload");
        return -1;
    }
    return e.id;
}

static void removeEffect(int &fd, qint16 & effectId, bool closeFd)
{
    if (fd < 0)
        return;

    if (effectId >= 0 && ioctl(fd, EVIOCRMFF, effectId) == -1)
        perror("rumble remove");
    effectId = -1;

    if (closeFd) {
        close(fd);
        fd = -1;
    }
}

NeoVibrateAccessory::NeoVibrateAccessory(QModemService * service)
:  QVibrateAccessoryProvider(service->service(), service)
    , rumbleFd(-1)
    , effectId(-1)
{
    setSupportsVibrateOnRing(true);
    setSupportsVibrateNow(true);
}

NeoVibrateAccessory::~NeoVibrateAccessory()
{
    removeEffect(rumbleFd, effectId, true);
}

void NeoVibrateAccessory::setVibrateOnRing(const bool value)
{
    qLog(Modem) << "setVibrateOnRing " << value;
    setVibrateNow(value);
}

void NeoVibrateAccessory::setVibrateNow(const bool value, int timeoutMs, int strength)
{
    struct input_event event;

    //qLog(Modem) << "setVibrateNow " << value << ", timeoutMs=" << timeoutMs << ", strength=" << strength;

    if (value && rumbleFd < 0)
        rumbleFd = openRumble();

    if (rumbleFd < 0)
        return;

    if (value && effectId < 0)
        effectId = uploadEffect(rumbleFd, (quint16)(strength), timeoutMs);

    if (effectId < 0)
        return;

    // Play/stop the effect
    event.type = EV_FF;
    event.code = effectId;
    event.value = (value ? 1 : 0);

    if (write(rumbleFd, (const void *)&event, sizeof(event)) == -1) {
        perror("rumble write");
    }

    if (!value)
        removeEffect(rumbleFd, effectId, true);

    QVibrateAccessoryProvider::setVibrateNow(value);
}

/* [1]
 
first number (mode):
0:  GSM only (2G only which IMHO includes GPRS and EDGE)
1:  WDMA only (3G only)
2:  GSM first (i.e. 2G over 3G)
3:  WCDMA first (i.e. 3G over 2G)
4:  no change
5:  auto

second number (service domain):
0:  circuit switched only (i.e. no GPRS/EDGE)
1:  packet switched only
2:  circuit and/or packed switched
3:  no preference
4:  no change
5:  demand PS attach
6:  demand PS detach
 
*/