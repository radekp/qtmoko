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

#include "poweralerttask.h"
#include "qabstractmessagebox.h"
#include "alertservicetask.h"

#include <qled.h>
#include <QPowerStatus>
#include <QSettings>

/*!
  \class PowerAlertDialogTask
    \inpublicgroup QtMediaModule
  \brief The PowerAlertDialogTask class provides a alert dialog that notifies the user
  when the battery is running low.
  \ingroup QtopiaServer::GeneralUI

  The PowerAlertDialogTask provides a Qt Extended Server Task and cannot be used by other Qt Extended applications. It displays the power alert dialog whenever the device battery runs low.  The
  default implementation is a standard message box and an audible sound alerting the user to the
  low power situation.

  The dialog appears when the battery level is very low or critical.
  The dialog will be redisplayed every five minutes, when very low, or every
  minute, when critical, until the power state improves or the device is placed
  on a charger.  The default implementation uses the QPowerStatus to determine
  this information.

  The period at which the power alert dialog redisplays can be configured by
  setting the \c {PowerAlertDialog/DisplayPeriod} and
  \c {PowerAlertDialog/CriticalDisplayPeriod} values to the period in seconds
  in the \c {Trolltech/HardwareAccessories} configuration file.  If not
  specified the default values of 300 seconds (5 minutes) and 60 seconds are
  used for \c {PowerAlertDialog/DisplayPeriod} and
  \c {PowerAlertDialog/CriticalDisplayPeriod} respectively.  Setting
  \c {PowerAlertDialog/DisplayPeriod} to 0 disables the power alert dialog.
*/

/*!
  \internal
  */
PowerAlertDialogTask::PowerAlertDialogTask( QObject *parent )
    : QObject( parent ), powerstatus(0), box(0)
{
    QSettings cfg("Trolltech", "HardwareAccessories");
    int redisplay = 
        cfg.value("PowerAlertDialog/DisplayPeriod", 300).toInt();
    if(redisplay > 0) {
        powerstatus = new QPowerStatus(0);
        timer.setInterval(redisplay * 1000, 
                             QtopiaTimer::PauseWhenInactive);
        QObject::connect(&timer, SIGNAL(timeout()), this, SLOT(powerChanged()));
        QObject::connect(powerstatus, SIGNAL(batteryStatusChanged(QPowerStatus::BatteryStatus)), this, SLOT(powerChanged()));
        QObject::connect(powerstatus, SIGNAL(batteryChargingChanged(bool)),
                         this, SLOT(powerChanged()));
    }
}

/*!
  \internal
  */
PowerAlertDialogTask::~PowerAlertDialogTask()
{
    if (box) {
        delete box;
        box = 0;
    }
}

/*!
  \internal
  */
void PowerAlertDialogTask::powerChanged()
{
    if ( !powerstatus )
        return;

    QString str;
    
    // Set Orange Diode on battery chargeing
    if(powerstatus->batteryCharging()) {
        qLedSetPower(qLedAttrBrightness(), qLedMaxBrightness());
    }
    else {
        switch(powerstatus->batteryStatus()) {
            case QPowerStatus::VeryLow:
            {
                QSettings cfg("Trolltech", "HardwareAccessories");
                str = tr( "Battery is running very low." );
                timer.setInterval(cfg.value("PowerAlertDialog/DisplayPeriod", 300).toInt() * 1000,
                                  QtopiaTimer::PauseWhenInactive);
                break;
            }
            case QPowerStatus::Critical:
            {
                QSettings cfg("Trolltech", "HardwareAccessories");
                str = tr( "Battery level is critical!\n"
                          "Please recharge now!" );
                timer.setInterval(cfg.value("PowerAlertDialog/CriticalDisplayPeriod", 60).toInt() * 1000);
                break;
            }
            default:
                qLedIndicatorLowPowerOff();
                break;
        }
    }

    if (!box) {
        box = QAbstractMessageBox::messageBox(0, tr("Battery Status"), tr("Low Battery"),
                               QAbstractMessageBox::Critical );
        if (!box) return;
    }

    if(!str.isEmpty()) {
        box->setText("<qt>" + str + "</qt>");

        //Qtopia::soundAlarm();
        qLedIndicatorLowPowerOn();
        
        timer.start();
        QtopiaApplication::showDialog( box );
    } else {
        timer.stop();
    }
}

QTOPIA_TASK( PowerAlertDialogTask, PowerAlertDialogTask );
