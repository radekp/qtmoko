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
#include "dialstring.h"

/*
   DialUp::dialString() returns the dialstring for the internal
   modem. The dial string initializes the modem for any kind of
   dialup connection. It is likely that this string needs to be
   modified in order to get a connection running.

   The returned string must provide a QString parameter slot for
   the APN.
 */


QString GPRSDialString()
{
    QString dialstring;
//Generic -> tested with Wavecom FASTRACK and Ericsson T39m
    dialstring = "AT+CGDCONT=1,\"IP\",\"%1\" OK "
           "AT+CGATT=1 OK "
           "ATD*99***1#";
    return dialstring;
}

QString GPRSDisconnectString()
{
    QString result;
    result = "\"\" \\d+++\\d\\c OK\nAT+CGATT=0 OK";

    return result;

}

