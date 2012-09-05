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

#ifndef RINGCONTROL_H
#define RINGCONTROL_H

#include <QObject>

// namespaced to avoid a conflict with a class in the server
namespace clockalarmringcontrol {

class RingControlPrivate;

class RingControl : public QObject
{
    Q_OBJECT
public:
    RingControl( QObject *parent = 0 );
    ~RingControl();

    void setRepeat( int repeat );

    void setSound( const QString &file );
    void enableVibrate( bool vibrate );

    void setSoundTimer( int off );
    void setVibrateTimers( int on, int off );
    void setVibrateStartDelay( int secs );

    bool isActive();

public slots:
    void start();
    void stop();

signals:
    void finished();

private:
    void startNoise();
    void pollNoise();
    void stopNoise();
    void startVibrate();
    void stopVibrate();
    void stopVibrate_notimer();
    void timerEvent( QTimerEvent *e );
    void startTimer( int &timer, int timeout );
    void stopTimer( int &timer );

    RingControlPrivate *d;
};

};

#endif
