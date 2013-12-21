/****************************************************************************
**
** This file is part of the Qt Extended Opensource Package.
**
** Copyright (C) 2009 Trolltech ASA.
** Copyright (C) 2012 Radek Polak.
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

#include <stdio.h>
#include <stdlib.h>

#include <QFile>
#include <QTimer>
#include <QProcess>
#include <QDateTime>
#include <QPowerSource>
#include <QDesktopWidget>
#include <QtopiaIpcAdaptor>
#include <QtopiaIpcEnvelope>
#include <QtopiaServiceRequest>

#include "systemsuspend.h"

#include <qvaluespace.h>

#ifdef Q_WS_QWS
#include <qwindowsystem_qws.h>
#endif

class NeoSuspend : public SystemSuspendHandler
{
public:
    NeoSuspend();
    virtual bool canSuspend() const;
    virtual bool suspend();
    virtual bool wake();
private:
     QProcess resumeScript;
    QValueSpaceObject batteryVso;
    QDateTime suspendTime;
    int chargeNowBeforeSuspend;
};

QByteArray wakeupCount;

QTOPIA_DEMAND_TASK(NeoSuspend, NeoSuspend);
QTOPIA_TASK_PROVIDES(NeoSuspend, SystemSuspendHandler);

NeoSuspend::NeoSuspend()
:  resumeScript(this)
    , batteryVso("/UI/Battery", this)
{
}

bool NeoSuspend::canSuspend() const
{
    wakeupCount = Qtopia::readFile("/sys/power/wakeup_count");

/*    QPowerSource src( QLatin1String("DefaultBattery") );
    return !src.charging();
*/
    /* Make sure resume script is finished */
    bool ok = (resumeScript.state() == QProcess::NotRunning);

    if (!ok) {
        qLog(PowerManagement) <<
            "Cant suspend, resume script is running, state=" <<
            resumeScript.state();
    }

    return ok;
}

static int readChargeNow()
{
    return qReadSysfsInt("/sys/class/power_supply/bq27000-battery/charge_now");
}

bool NeoSuspend::suspend()
{
    qLog(PowerManagement) << __PRETTY_FUNCTION__;

    QProcess::execute("before-suspend.sh");
    chargeNowBeforeSuspend = readChargeNow();
    suspendTime = QDateTime::currentDateTime();

    // Check if wakeup sources havent changed. If yes, then the write will fail
    // For more info see:
    // http://lists.goldelico.com/pipermail/gta04-owner/2012-July/002587.html
    if (Qtopia::
        writeFile("/sys/power/wakeup_count", wakeupCount.constData(),
                  wakeupCount.count(), false, 0, 1)) {
        qLog(PowerManagement) <<
            "suspend aborted because by kernel wakeup sources";
        return false;
    }

    Qtopia::writeFile("/sys/power/state", "mem", 3, false);
    return true;
}

bool NeoSuspend::wake()
{
    // Average current in suspend computed from charge_now and suspend time
    QDateTime now = QDateTime::currentDateTime();
    int chargeNow = readChargeNow();
    int secs = suspendTime.secsTo(now);
    if (secs == 0)
        secs++;
    int avgCurrent = ((chargeNowBeforeSuspend - chargeNow) * 36) / (10 * secs); // (first_number - last_number) * 3600 / (last_timestamp - first_timestamp), but we want it in mA so 1000x less
    batteryVso.setAttribute("avg_current_in_suspend",
                            QString::number(avgCurrent));

    // Read and update current_now. It should contain the current in suspend
    int currentNow = qReadSysfsInt("/sys/class/power_supply/bq27000-battery/current_now") / 1000;
    batteryVso.setAttribute("current_now_in_suspend",
                            QString::number(currentNow));

#ifdef Q_WS_QWS
    QWSServer::instance()->refresh();
#endif
    QtopiaIpcEnvelope("QPE/Card", "mtabChanged()"); // might have changed while asleep

    //screenSaverActivate uses QTimer which depends on hwclock update
    //when the device wakes up. The clock update is another event in the
    //Qt event loop (see qeventdispatcher_unix.cpp. We have to call
    //processEvents to give Qt a chance to sync its internal timer with
    //the hw clock
#ifdef Q_WS_QWS
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    QWSServer::screenSaverActivate(false);
    {
        QtopiaIpcEnvelope("QPE/Card", "mtabChanged()"); // might have changed while asleep
        QtopiaServiceRequest e("QtopiaPowerManager", "setBacklight(int)");
        e << -3;                // Force on
        e.send();
        QtopiaIpcEnvelope("QPE/NetworkState", "updateNetwork()");   //might have changed
    }
#endif

    /* No waitForFinished after start, because we want the command to complete
       in the background */
    resumeScript.start("after-resume.sh");
    return true;
}
