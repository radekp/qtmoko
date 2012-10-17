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

#include "neohardware.h"

#include <QSocketNotifier>
#include <QTimer>
#include <QLabel>
#include <QDesktopWidget>
#include <QProcess>
#include <QPowerSourceProvider>

#include <qcontentset.h>
#include <qtopiaapplication.h>
#include <qtopialog.h>
#include <qtopiaipcadaptor.h>

#include <qbootsourceaccessory.h>
#include <qtopiaipcenvelope.h>

#include <qtopiaserverapplication.h>

#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <linux/input.h>

#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <linux/types.h>
#include <linux/netlink.h>
#include <QTcpSocket>
#include <QtDebug>

QTOPIA_TASK(NeoHardware, NeoHardware);

// Setup netlink socket for watching usb cable and battery events
static int openNetlink()
{
    struct sockaddr_nl snl;
    memset(&snl, 0x00, sizeof(struct sockaddr_nl));
    snl.nl_family = AF_NETLINK;
    snl.nl_pid = getpid();
    snl.nl_groups = 1;
    snl.nl_groups = 0x1;

    int fd = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);
    if (fd == -1) {
        qLog(Hardware) << "error getting uevent socket: " << strerror(errno);
        return -1;
    }
    if (bind(fd, (struct sockaddr *)&snl, sizeof(struct sockaddr_nl)) < 0) {
        qLog(Hardware) << "uevent bind failed: " << strerror(errno);
        return -1;
    }
    return fd;
}

NeoHardware::NeoHardware()
:
ac(QPowerSource::Wall, "PrimaryAC", this)
    , battery(QPowerSource::Battery, "NeoBattery", this)
    , batteryVso("/UI/Battery", this)
    , vsoPortableHandsfree("/Hardware/Accessories/PortableHandsfree")
    , ueventSocket(this)
    , timer(this)
{
    qLog(Hardware) << "gta04 hardware";

    int netlinkFd = openNetlink();
    if (netlinkFd >= 0) {
        ueventSocket.setSocketDescriptor(netlinkFd);
        connect(&ueventSocket, SIGNAL(readyRead()), this, SLOT(uevent()));
    }

    hasSmartBattery =
        QFile::exists("/sys/class/power_supply/bq27000-battery/status");

    connect(&timer, SIGNAL(timeout()), this, SLOT(updateStatus()));
    timer.start(30 * 1000);
    
    QTimer::singleShot(1, this, SLOT(updateStatus()));
    
    adaptor = new QtopiaIpcAdaptor("QPE/NeoHardware");
    audioMgr = new QtopiaIpcAdaptor("QPE/AudioStateManager", this);

    QtopiaIpcAdaptor::connect(adaptor, MESSAGE(headphonesInserted(bool)),
                              this, SLOT(headphonesInserted(bool)));
}

NeoHardware::~NeoHardware()
{
}

void NeoHardware::updateStatus()
{
    QByteArray twlVbus = qReadFile("/sys/bus/platform/devices/twl4030_usb/vbus");
    if (twlVbus.contains("on")) {
        ac.setAvailability(QPowerSource::Available);
    } else {
        ac.setAvailability(QPowerSource::NotAvailable);
    }

    QString chargingStr =
        qReadFile("/sys/class/power_supply/bq27000-battery/status");
    QString capacityStr =
        qReadFile("/sys/class/power_supply/bq27000-battery/capacity");
    int capacity = capacityStr.toInt();

    battery.setCharging(chargingStr.contains("Charging"));

    if (capacity > 0) {
        battery.setCharge(capacity);
    }

    if (chargingStr.contains("Discharging")) {
        QString time =
            qReadFile
            ("/sys/class/power_supply/bq27000-battery/time_to_empty_now");
        battery.setTimeRemaining(time.toInt() / 60);
    }
    
    QString currentNowStr =
        qReadFile("/sys/class/power_supply/bq27000-battery/current_now");
    int currentNow = currentNowStr.toInt() / 1000;
    batteryVso.setAttribute("current_now", QString::number(currentNow));
}

#define UEVENT_BUFFER_SIZE 1024
void NeoHardware::uevent()
{
    char buffer[1024];
    if (ueventSocket.read(buffer, UEVENT_BUFFER_SIZE) <= 0) {
        return;
    }
    if (strstr(buffer, "twl4030") || strstr(buffer, "bq27000-battery")) {
        QTimer::singleShot(1000, this, SLOT(updateStatus()));   // vbus updates < 1s
        QTimer::singleShot(2000, this, SLOT(updateStatus()));   // but sometimes needs 2s when changing on->off
        QTimer::singleShot(10000, this, SLOT(updateStatus()));  // battery charging needs 10s
        QTimer::singleShot(20000, this, SLOT(updateStatus()));  // try also after 20s in case status at 10s was not yet correct
    }
}

void NeoHardware::shutdownRequested()
{
    qLog(PowerManagement) << __PRETTY_FUNCTION__;
    QtopiaServerApplication::instance()->
        shutdown(QtopiaServerApplication::ShutdownSystem);
}

void NeoHardware::headphonesInserted(bool b)
{
    qLog(Hardware)<< __PRETTY_FUNCTION__ << b;
    vsoPortableHandsfree.setAttribute("Present", b);
    vsoPortableHandsfree.sync();
    if (b) {
        QByteArray mode("Headphone");
        audioMgr->send("setProfile(QByteArray)", mode);
    } else {
        QByteArray mode("MediaSpeaker");
        audioMgr->send("setProfile(QByteArray)", mode);
    }
}