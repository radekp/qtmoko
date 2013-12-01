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

#include "neobattery.h"
#include "qtopiaserverapplication.h"

#include <QPowerSourceProvider>
#include <QTimer>
#include <QFileMonitor>

#include <math.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <QValueSpaceItem>
#include <QtopiaIpcEnvelope>

NeoBattery::NeoBattery(QObject *parent)
: QObject(parent),
  ac(0),
  battery(0),
  vsUsbCable(0),
  cableEnabled(0),
  charging(0),
  isSmartBattery(0),
  percentCharge(0)
{
    bool apm = APMEnabled();
    qLog(PowerManagement)<<"NeoBattery::NeoBattery";

    QtopiaServerApplication::taskValueSpaceSetAttribute("NeoBattery",
                                                        "APMAvailable", apm);

    ac = new QPowerSourceProvider(QPowerSource::Wall, "PrimaryAC", this);
    battery = new QPowerSourceProvider(QPowerSource::Battery, "NeoBattery", this);


    vsUsbCable = new QValueSpaceItem("/Hardware/UsbGadget/cableConnected", this);
    connect( vsUsbCable, SIGNAL(contentsChanged()), SLOT(cableChanged()));

    cableChanged();

    startTimer(60 * 1000);

    if ( QFileInfo("/sys/class/power_supply/battery/status").exists()) {
        QTimer::singleShot( 10 * 1000, this, SLOT(updateSysStatus()));
        isSmartBattery = true;
    } else {
        // 1973 only has dumb battery and must use apm
        QTimer::singleShot( 10 * 1000, this, SLOT(updateDumbStatus()));
        isSmartBattery = false;
    }
}

/*! \internal */
bool NeoBattery::APMEnabled() const
{
    int apm_install_flags;
    FILE *f = fopen("/proc/apm", "r");
    if ( f ) {
        //I 1.13 1.2 0x02 0x00 0xff 0xff 49% 147 sec
        fscanf(f, "%*[^ ] %*d.%*d 0x%x 0x%*x 0x%*x 0x%*x %*d%% %*i %*c",
               &apm_install_flags);
        fclose(f);

        if (!(apm_install_flags & 0x08)) //!APM_BIOS_DISABLED
        {
            qLog(PowerManagement)<<"Neo APM Enabled";
            return true;
        }
    }
    qLog(PowerManagement)<<"Neo APM Disabled";
    return false;
}

void NeoBattery::apmFileChanged(const QString &/* file*/)
{
    updateDumbStatus();
}

void NeoBattery::updateSysStatus()
{
    charging = isCharging();
    qLog(PowerManagement) << __PRETTY_FUNCTION__ << charging;

    int capacity = getCapacity();

	// JM: Occasionally we get a glitch and capacity returns 0, which
	// is an unlikely value, so we ignore it, otherwise we get a
	// critical battery dialog
	if(capacity == 0){
		qLog(PowerManagement) << __PRETTY_FUNCTION__ << "ignoring 0 capacity";
		return;
	}
		
		
	//JM: now is charging returns the correct result, we just need to do this
    battery->setCharging(charging);
    battery->setCharge(capacity);
    battery->setTimeRemaining(getTimeRemaining());

}

void NeoBattery::updateDumbStatus()
{
    qLog(PowerManagement) << __PRETTY_FUNCTION__;
    int min = -1; // Remaining battery (minutes)

    // apm on freerunner is borked.

    bool isFull = batteryIsFull();

    battery->setCharge( percentCharge);

    qLog(PowerManagement) << __PRETTY_FUNCTION__ << cableEnabled << percentCharge;

    battery->setCharging( cableEnabled && !isFull);
    battery->setTimeRemaining(min);
}

int NeoBattery::getDumbCapacity()
{
    qLog(PowerManagement) << __PRETTY_FUNCTION__;
    
    int voltage = qReadSysfsInt("/sys/class/power_supply/battery/voltage_now");
    qLog(PowerManagement)<<"voltage"<< voltage;

    // lets use 3400 as empty, for all intensive purposes,
    // 2 minutes left of battery life till neo shuts off might
    // as well be empty

    voltage = voltage - 3400000;
    float perc = voltage  / 8;
    percentCharge = (int)round( perc + 0.5);
    percentCharge = qBound<quint16>(0, percentCharge, 100);
    qLog(PowerManagement)<<"Battery volt"<< voltage + 3400000 << percentCharge<<"%";
    return voltage;
}

/*! \internal */
bool NeoBattery::batteryIsFull()
{
    qLog(PowerManagement) << __PRETTY_FUNCTION__;
    if(getDumbCapacity() + 3400000 > 4170000)
        return true;
    return false;
}

/*!
  \internal */
void NeoBattery::timerEvent(QTimerEvent *)
{
    updateStatus();
}

/*!
  \internal */
void NeoBattery::updateStatus()
{
    if (isSmartBattery)
        QTimer::singleShot( 1000, this, SLOT(updateSysStatus()));
    else
        QTimer::singleShot( 1000, this, SLOT(updateDumbStatus()));
}

/*!
  \internal */
bool NeoBattery::isCharging()
{
    char buf[64];
    qLog(PowerManagement) << __PRETTY_FUNCTION__;
    
    qReadSysfsStr("/sys/class/power_supply/battery/status", buf);
    qLog(PowerManagement) << __PRETTY_FUNCTION__ << buf;
    // Charging  Discharging  Not charging
    // ac        battery      ac/full
	// JM: Fixed this as it can return Not charging too
    return (strcmp(buf, "Charging") == 0);
}

/*!
  \internal */
void NeoBattery::cableChanged()
{
    cableEnabled = vsUsbCable->value().toBool();
    qLog(PowerManagement) << __PRETTY_FUNCTION__ << cableEnabled;

    charging = cableEnabled;

    if(cableEnabled) {
        qLog(PowerManagement) << "charging";
        ac->setAvailability(QPowerSource::Available);
    } else {
        qLog(PowerManagement) << "not charging";
        ac->setAvailability(QPowerSource::NotAvailable);
    }
    updateStatus();
}

/*!
  \internal */
int NeoBattery::getCapacity()
{
    if (!isSmartBattery)
        return 0;

    qLog(PowerManagement) << __PRETTY_FUNCTION__;

    int capacity = qReadSysfsInt("/sys/class/power_supply/battery/capacity");

// JM: Removed this test as it will permanently switch to dumb battery which will always return 100%
// might be gta02 with dumb battery
//     if (capacity == 0) {
//         isSmartBattery = false;
//         updateDumbStatus();
//     }

	return capacity;
}

/*!
  \internal */
int NeoBattery::getTimeToFull()
{
    if (!isSmartBattery)
        return 0;
    qLog(PowerManagement) << __PRETTY_FUNCTION__;

    int time = qReadSysfsInt("/sys/class/power_supply/battery/time_to_full_now");
    return time/60;

}

/*!
  \internal */
int NeoBattery::getTimeRemaining()
{
    if (!isSmartBattery)
        return 0;
    qLog(PowerManagement) << __PRETTY_FUNCTION__;

    int time = Qtopia::readSysfsInt("/sys/class/power_supply/battery/time_to_empty_now", false, 0);
    return time/60;
}

QTOPIA_TASK(NeoBattery, NeoBattery);
