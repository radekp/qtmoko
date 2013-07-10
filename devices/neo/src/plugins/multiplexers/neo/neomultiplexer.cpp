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

#include "neomultiplexer.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QSettings>
#include <qtopialog.h>
#include <QValueSpaceItem>

#include <qgsm0710multiplexer.h>
#include <qserialport.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <termios.h>

QTOPIA_EXPORT_PLUGIN( NeoMultiplexerPlugin )

// Define this to disable GSM 07.10 multiplexing, but still do wakeup handling.
//#define NEO_NO_MUX

// Size of GSM 07.10 frames to use with the multiplexer.
#ifndef NEO_FRAME_SIZE
#define NEO_FRAME_SIZE 64
#endif

#define N_TIHTC 17

NeoMultiplexerPlugin::NeoMultiplexerPlugin( QObject* parent )
    : QSerialIODeviceMultiplexerPlugin( parent )
{
}

NeoMultiplexerPlugin::~NeoMultiplexerPlugin()
{
}

bool NeoMultiplexerPlugin::detect( QSerialIODevice *device )
{
    qLog(Hardware) << __PRETTY_FUNCTION__;

    // Power on modem via sysfs
    QFile f("/sys/bus/platform/devices/neo1973-pm-gsm.0/power_on");
    if(f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        f.write("0");
        f.close();
    }
    if(f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        f.write("1");
        f.close();
    } else {
        qWarning() << "Modem power on failed "<< f.errorString();
    }
    
    // The FIC needs a special line discipline set on the device.
    QSerialPort *port = qobject_cast<QSerialPort *>( device );
    if (port) {
        int discipline = N_TIHTC;
        ::ioctl(port->fd(), TIOCSETD, &discipline);
    }
    device->discard();
    int rc;

    struct termios t;
    rc = tcgetattr(port->fd(), &t);
    t.c_cflag |= CRTSCTS;
    rc = tcsetattr(port->fd(), TCSANOW, &t);

    QValueSpaceItem deviceString("/Hardware/Neo/Device");
    if ( deviceString.value().toString() == "GTA02") {
        qLog(Hardware) << __PRETTY_FUNCTION__ << "is gta02";
        muxEnabled = true;
    } else {
        qLog(Hardware) << __PRETTY_FUNCTION__ << "is gta01";
        muxEnabled = false;
    }

    QSettings cfg("Trolltech", "Modem");
    QString multiplexing = cfg.value("Multiplexing/Active", "yes").toString();
    muxEnabled &= (multiplexing != "no");
    qLog(Mux) << "Neo multiplexing " << (muxEnabled ? "enabled" : "disabled")
              << multiplexing;

    // Make the modem talk to us. It can be a bit rough to get
    // it initialized... So we will empty the current buffer
    // and then send ^Z\r\n and wait for an OK or AT from the modem. This is
    // mostly based on ideas from ogsmd

    device->readAll();
    int attempts = 0;
    for (; attempts < 2; ++attempts) {
        if (QSerialIODeviceMultiplexer::chat(device, QChar(0x1a))) {
            qLog(Modem) << "Attempts needed to initialize the modem" << attempts;
            break;
        }
    }

    if (attempts == 2) {
        qWarning() << "Initializing the modem failed.";
        abort();
    }

    // disable echoing of commands
    QSerialIODeviceMultiplexer::chat(device, "ATE0");
    device->readAll();

    if (muxEnabled) {
        // Issue the AT+CMUX command to determine if this device
        // uses GSM 07.10-style multiplexing.
        return QGsm0710Multiplexer::cmuxChat( device, NEO_FRAME_SIZE, true );
    }
    return true;
}

QSerialIODeviceMultiplexer *NeoMultiplexerPlugin::create( QSerialIODevice *device )
{
    qLog(Hardware) << __PRETTY_FUNCTION__;

    if (muxEnabled)
        return new QGsm0710Multiplexer( device, NEO_FRAME_SIZE, true );
    return new QNullSerialIODeviceMultiplexer( device );
}
