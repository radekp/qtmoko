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
#include <qled.h>
#include <qtopialog.h>
#include <qmodemllindicators.h>

// Called when ringing() function is called
void QModemLLIndicators::Ringing()
{
    qLedIndicatorMissedCallsOn();          // consider the called missed now
}

// Called on QModemCall.hangup()
void QModemLLIndicators::MissedCallsChanged(int num)
{
    static int numMissed = 0;
    qLog(Modem) << "MissedCallsChanged " << numMissed << "->" << num;
    if (numMissed > 0 && num == 0) {
        qLedIndicatorMissedCallsOff();     // clear missed call led
    } else if (numMissed == 0 && num > 0) {
        qLedIndicatorMissedCallsOn();
    }
    numMissed = num;
}

// Called on QModemCall.accept()
void QModemLLIndicators::Hangup()
{
    qLedIndicatorMissedCallsOff();         // clear missed call led - user should have noticed it when he hangs up
}

// Called user checks missed calls
void QModemLLIndicators::Accept()
{
    qLedIndicatorMissedCallsOff();         // clear missed call led when users accepts the call
}
