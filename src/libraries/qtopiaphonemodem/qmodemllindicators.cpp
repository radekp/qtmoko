/****************************************************************************
**
** This file is part of the Qt Extended Opensource Package.
**
** Copyright (C) 2011 Radek Polak.
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

#include <QFile>
#include <QDebug>
#include <qtopialog.h>
#include <qmodemllindicators.h>

// Turn on/off led. Device can be "gta02" or "gta04".
// GTA04: color can be "green" or "red" type can be "power" or "aux"
// GTA02: supports "blue power" "orange power" and "red aux"
static bool setLed(const char *device, const char *color,
                   const char *type, const char *attr, const char *value)
{
    char filename[255];
    sprintf(filename, "/sys/class/leds/%s:%s:%s/%s", device, color, type, attr);    // e.g. /sys/class/leds/gta04:green:power/brightness
    QFile f(filename);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        qWarning() << "setLed failed" << f.errorString();
        return false;
    }
    f.write(value);
    f.close();
    return true;
}

#ifdef QT_QWS_GTA04

static void missedCallLedOn()
{
    setLed("gta04", "green", "power", "brightness", "255");
    setLed("gta04", "green", "power", "trigger", "timer");
    setLed("gta04", "green", "power", "delay_off", "1024");
    setLed("gta04", "green", "power", "delay_on", "1024");
}

static void missedCallLedOff()
{
    setLed("gta04", "green", "power", "brightness", "0");
    setLed("gta04", "green", "power", "trigger", "none");
}

#elif QT_QWS_NEO

static void missedCallLedOn()
{
    setLed("gta02", "blue", "power", "brightness", "255");
}

static void missedCallLedOff()
{
    setLed("gta02", "blue", "power", "brightness", "0");
}

#else

static void missedCallLedOn()
{
}

static void missedCallLedOff()
{
}

#endif

// Called when ringing() function is called
void QModemLLIndicators::Ringing()
{
    missedCallLedOn();          // consider the called missed now
}

// Called on QModemCall.hangup()
void QModemLLIndicators::MissedCallsChanged(int num)
{
    static int numMissed = 0;
    qLog(Modem) << "MissedCallsChanged " << numMissed << "->" << num;
    if (numMissed > 0 && num == 0) {
        missedCallLedOff();     // clear missed call led
    } else if (numMissed == 0 && num > 0) {
        missedCallLedOn();
    }
    numMissed = num;
}

// Called on QModemCall.accept()
void QModemLLIndicators::Hangup()
{
    missedCallLedOff();         // clear missed call led - user should have noticed it when he hangs up
}

// Called user checks missed calls
void QModemLLIndicators::Accept()
{
    missedCallLedOff();         // clear missed call led when users accepts the call
}
