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

#ifndef NEOHARDWARE_H
#define NEOHARDWARE_H

#include <QFile>
#include <QTimer>
#include <QObject>
#include <QProcess>
#include <QDateTime>
#include <QTcpSocket>
#include <QtopiaIpcAdaptor>
#include <QPowerSourceProvider>

#include <qvaluespace.h>
#include <linux/input.h>

class QBootSourceAccessoryProvider;
class QPowerSourceProvider;

class QSocketNotifier;
class QtopiaIpcAdaptor;
class QSpeakerPhoneAccessoryProvider;

// Initial charging current - we will raise it until charging voltage does not
// drop under 4.6V
#define INIT_CURRENT 100000

// Max charging current
#define MAX_CURRENT 600000

// Max value used to raise charging current. We cant raise with large numbers
// because charging voltage could drop below 4.4V and made charging unreliable
#define CURRENT_PLUS 50000

class NeoHardware : public QObject
{
    Q_OBJECT
public:
    NeoHardware();
    ~NeoHardware();

private:
    QPowerSourceProvider ac;
    QPowerSourceProvider battery;
    QValueSpaceObject batteryVso;
    QValueSpaceObject vsoPortableHandsfree;
    QFile chargeLog;                         // power supply log
    QDateTime lastLogDt;
    QTcpSocket ueventSocket;
    QTimer timer;
    int chargingLogInterval;
    int maxChargeCurrent;
    int oldChargeNow;
    int oldChargerVoltage;
    bool hasSmartBattery;
    QtopiaIpcAdaptor *adaptor;
    QtopiaIpcAdaptor *audioMgr;
    void setMaxChargeCurrent(int newValue);
    void logCharge(QDateTime now, int chargeNow);
    
private slots:
    void headphonesInserted(bool);
    void shutdownRequested();
    void uevent();
    void updateStatus();
};

#endif
