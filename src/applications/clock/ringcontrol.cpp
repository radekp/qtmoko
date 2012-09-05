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
#include <qdebug.h>
#include <QtopiaApplication>
#include <version.h>
#include <QVibrateAccessory>

#include <QSound>
#include <QSoundControl>
#include <QTimerEvent>
#include <QFileInfo>

namespace clockalarmringcontrol {

class RingControlPrivate
{
public:
    int startNoiseTimer;
    int stopNoiseTimer;
    int startVibrateTimer;
    int stopVibrateTimer;
    QSound *alertSound;
    QSoundControl *alertSoundControl;
    int noiseOff;
    int vibrateOn;
    int vibrateOff;
    int vibrateStartDelay;
    bool active;
    int toRepeat;
    int atRepeat;
    QString soundFile;
};

};

using namespace clockalarmringcontrol;

RingControl::RingControl( QObject *parent )
    : QObject( parent )
{
    d = new RingControlPrivate;
    d->alertSound = 0;
    d->alertSoundControl = 0;
    setSound(":sound/alarm");
    d->startNoiseTimer = 0;
    d->stopNoiseTimer = 0;
    d->startVibrateTimer = 0;
    d->stopVibrateTimer = 0;
    d->noiseOff = 2000;
    d->vibrateOn = 500;
    d->vibrateOff = 2000;
    d->vibrateStartDelay = 0;
    d->active = false;
    d->toRepeat = 0;
    d->atRepeat = 0;
}

RingControl::~RingControl()
{
    delete d;
}

void RingControl::setSound( const QString &file )
{
    if ( d->alertSound ) {
        delete d->alertSound;
        d->alertSound = 0;
    }
    if ( d->alertSoundControl ) {
        delete d->alertSoundControl;
        d->alertSoundControl = 0;
    }
    d->alertSound = new QSound( file, this );
    d->alertSoundControl = new QSoundControl( d->alertSound, this );
    d->alertSoundControl->setPriority( QSoundControl::RingTone );
    d->soundFile = file;
}

void RingControl::enableVibrate( bool vibrate )
{
    if ( vibrate) {
    } else if ( !vibrate) {

    }
}

void RingControl::setSoundTimer( int off )
{
    d->noiseOff = off;
}

void RingControl::setVibrateTimers( int on, int off )
{
    d->vibrateOn = on;
    d->vibrateOff = off;
}

void RingControl::setVibrateStartDelay( int secs )
{
    d->vibrateStartDelay = secs * 1000;
}

void RingControl::start()
{
    d->active = true;

    if ( d->alertSound ) {
        // start the noise
        startNoise();
    }

    // start the vibration
    startVibrate();
}

void RingControl::stop()
{
    if ( d->alertSound ) {
        stopTimer( d->startNoiseTimer );
        stopTimer( d->stopNoiseTimer );
        stopNoise();
        setSound(d->soundFile);
    }

    stopTimer( d->startVibrateTimer );
    stopTimer( d->stopVibrateTimer );
    stopVibrate_notimer();
    // just in case ...
    stopVibrate_notimer();

    d->active = false;
}

void RingControl::startNoise()
{
    stopTimer( d->startNoiseTimer );
    // start the noise
    d->alertSound->play();
    // there's no async notification that a sound has finished so we need to poll
    startTimer( d->stopNoiseTimer, 50 );
}

void RingControl::pollNoise()
{
    if ( d->alertSound->isFinished() ) {
        stop();
        if ( d->toRepeat ) {
            if ( ++d->atRepeat == d->toRepeat ) {
                qWarning() << "repeated" << d->atRepeat << "times";
                emit finished();
                return;
            }
        }
        // start again in 2 seconds
        startTimer( d->startNoiseTimer, d->noiseOff );
    }
}

void RingControl::stopNoise()
{
    stopTimer( d->stopNoiseTimer );
    // stop the noise
    d->alertSound->stop();
}

void RingControl::startVibrate()
{
    if(d->vibrateStartDelay > 0) {
        d->vibrateStartDelay -= d->vibrateOn + d->vibrateOff;
    }
    else {
        QVibrateAccessory vib;
        vib.setVibrateNow( true);
    }
    // we stop vibrating before we stop making noise
    startTimer( d->stopVibrateTimer, d->vibrateOn );
}

void RingControl::stopVibrate()
{
    if(d->vibrateStartDelay <= 0) {
        QVibrateAccessory vib;
        vib.setVibrateNow( false);
    }

    // start vibrating in 2 seconds
    startTimer( d->startVibrateTimer, d->vibrateOff );
}

void RingControl::stopVibrate_notimer()
{
    QVibrateAccessory vib;
    vib.setVibrateNow( false);
}

void RingControl::timerEvent( QTimerEvent *e )
{
    int id = e->timerId();
    if ( id == d->startNoiseTimer ) {
        start();
    } else if ( id == d->stopNoiseTimer ) {
        pollNoise();
    } else if ( id == d->startVibrateTimer ) {
        startVibrate();
    } else if ( id == d->stopVibrateTimer ) {
        stopVibrate();
    }
}

void RingControl::startTimer( int &timer, int timeout )
{
    if ( !timer )
        timer = QObject::startTimer( timeout );
}

void RingControl::stopTimer( int &timer )
{
    if ( timer ) {
        QObject::killTimer( timer );
        timer = 0;
    }
}

bool RingControl::isActive()
{
    return d->active;
}

void RingControl::setRepeat( int repeat )
{
    d->toRepeat = repeat;
    d->atRepeat = 0;
}

