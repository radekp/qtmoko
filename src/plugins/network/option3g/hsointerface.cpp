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

#include "hsointerface.h"
#include "config.h"
#include "dialstring.h"

#include <qtopialog.h>
#include <qtopianamespace.h>
#include <qtopiaipcadaptor.h>

#include <QDebug>
#include <QAtChat>
#include <QAtResultParser>

#include <errno.h>
#include <string.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

/* This is howto from NeilBrown
   (http://lists.goldelico.com/pipermail/gta04-owner/2012-January/001361.html)

0/ connect to /dev/ttyHS3  (others might work)
1/ make sure you are registered with network.
  e.g.
     AT+CFUN=1
     AT+COPS
     AT+COPS?

2/ establish data connection
     AT_OWANCALL=1,1,1

3/ collect status

   AT_OWANDATA?
  My response was
   _OWANDATA: 1, 49.179.102.244, 0.0.0.0, 211.29.132.12, 61.88.88.88, 0.0.0.0, 0.0.0.0,144000
                 ^IP address   ^          ^DNS-1------^  ^DNS-2----^

4/  configure network

    ifconfig hso0 49.179.102.244 up
    route add default dev hso0

    echo nameserver 211.29.132.12 > /etc/resolv.conf
    echo nameserver 61.88.88.88 >> /etc/resolv.conf

And you should be set to go.  If you want tethering via USB then add:
 on GTA04:
    echo 1 > /proc/sys/net/ipv4/ip_forward 
    iptables -t nat -A POSTROUTING -s 192.168.0.200 -j MASQUERADE

   (here 192.168.0.200 is the IP of my notebook on the USB interface.)

 on notebook/desktop/whatever

    route add default gw 192.168.0.202
    echo nameserver 211.29.132.12 > /etc/resolv.conf
    echo nameserver 61.88.88.88 >> /etc/resolv.conf

   (192.168.0.202 is IP of GTA04 of USB link).

To terminate data call

  AT_OWANCALL=1,0,1
  
*/

HsoInterface::HsoInterface(const QString & confFile)
:state(Uninitialized)
, configIface(0)
, port(0)
#ifndef QTOPIA_NO_FSO
, gsmDev("org.freesmartphone.ogsmd", "/org/freesmartphone/GSM/Device",
         QDBusConnection::systemBus(), this)
, gsmPdp("org.freesmartphone.ogsmd", "/org/freesmartphone/GSM/Device",
         QDBusConnection::systemBus(), this)
, fsoEnabled(false)
#endif
, netSpace(0)
, trigger(0)
{
    qLog(Network) << "Creating HsoInterface instance";
    configIface = new DialupConfig(confFile);

#ifndef QTOPIA_NO_FSO
    fsoEnabled = !strcmp("Fso", getenv("QTOPIA_PHONE"));    // are we using FSO as backend?
#endif

    setState(Uninitialized);
}

HsoInterface::~HsoInterface()
{
    qLog(Network) << "Deleting HsoInterface instance";
    if (configIface)
        delete configIface;
    configIface = 0;
}

static QtopiaNetworkInterface::Status stateToStatus(HsoInterface::State state)
{
    switch (state) {
    case HsoInterface::Down:
        return QtopiaNetworkInterface::Down;
    case HsoInterface::EnablingWan:
    case HsoInterface::GettingWanParams:
    case HsoInterface::SettingApn:
    case HsoInterface::DisablingWan:
        return QtopiaNetworkInterface::Pending;
    case HsoInterface::Up:
        return QtopiaNetworkInterface::Up;
    default:
        return QtopiaNetworkInterface::Unknown;
    }
}

void HsoInterface::setState(State newState)
{
    Status oldStatus = stateToStatus(state);
    Status newStatus = stateToStatus(newState);

    this->state = newState;

    if (netSpace && newStatus != oldStatus) {
        netSpace->setAttribute("State", (int)(newStatus));
        trigger = (++trigger) % 256;
        netSpace->setAttribute("UpdateTrigger", trigger);
    }
}

QtopiaNetworkInterface::Status HsoInterface::status()
{
    Status res = stateToStatus(state);
    return res;
}

void HsoInterface::initialize()
{
    if (!netSpace) {
        const uint ident = qHash(configIface->configFile());
        QString path = QString("/Network/Interfaces/%1").arg(ident);
        netSpace = new QValueSpaceObject(path, this);
        netSpace->setAttribute("Config", configIface->configFile());
        netSpace->setAttribute("State", QtopiaNetworkInterface::Unknown);
        netSpace->setAttribute("Error", QtopiaNetworkInterface::NotInitialized);
        netSpace->setAttribute("ErrorString",
                               tr("Interface hasn't been initialized yet."));
        netSpace->setAttribute("NetDevice", QVariant());
        netSpace->setAttribute("UpdateTrigger", 0);
    }
    setState(isActive()? HsoInterface::Up : HsoInterface::Down);
}

void HsoInterface::cleanup()
{
    setState(HsoInterface::Uninitialized);
}

bool HsoInterface::setDefaultGateway()
{
    return true;
}

bool HsoInterface::openPort()
{
    if (port)
        return true;

    port = new QSerialPort("/dev/ttyHS_Control", 115200);
    if (port->open(QIODevice::ReadWrite))
        return true;

    qWarning() << "Failed to open /dev/ttyHS_Control: " << port->errorString();
    delete port;
    port = 0;
    return false;
}

void HsoInterface::closePort()
{
    port->close();
    port->deleteLater();
    port = NULL;
}

static bool writeResolvConf(QString dns1, QString dns2)
{
    QFile resolvConf("/etc/resolv.conf");
    if (!resolvConf.open(QIODevice::ReadWrite | QIODevice::Truncate)) {
        qWarning() << "hso: failed to open /etc/resolv.conf";
        return false;
    }

    resolvConf.write(QString("nameserver %1\nnameserver %2\n").
                     arg(dns1).arg(dns2).toLatin1());
    resolvConf.close();
    return true;
}

bool HsoInterface::start(const QVariant /*options */ )
{
    if (!openPort())
        return false;

    QAtChat *chat = port->atchat();
    chat->registerNotificationType("_OWANCALL:", this,
                                   SLOT(wanCallNotification(QString)));
    chat->registerNotificationType("_OWANDATA:", this,
                                   SLOT(wanDataNotification(QString)));

    QString apn = configIface->property("Serial/APN").toString();
    chat->chat(QString("AT+CGDCONT=1,\"IP\",\"%1\"").arg(apn), this,
               SLOT(atFinished(bool, QAtResult)));

    setState(SettingApn);
    return true;
}

bool HsoInterface::stop()
{
    if (!openPort())
        return false;

    QAtChat *chat = port->atchat();
    chat->chat("AT_OWANCALL=1,0,1", this, SLOT(atFinished(bool, QAtResult)));
    setState(HsoInterface::DisablingWan);

    return true;
}

void HsoInterface::atFinished(bool ok, QAtResult result)
{
    if (!ok) {
        netSpace->setAttribute("Error", QtopiaNetworkInterface::UnknownError);
        netSpace->setAttribute("ErrorString", result.verboseResult());
        return;
    }

    switch (state) {
    case SettingApn:
        port->atchat()->chat("AT_OWANCALL=1,1,1", this,
                             SLOT(atFinished(bool, QAtResult)));
        setState(EnablingWan);
        break;
    case EnablingWan:
        port->atchat()->chat("AT_OWANDATA?", this,
                             SLOT(atFinished(bool, QAtResult)));
        setState(GettingWanParams);
        break;
    case GettingWanParams:
        break;
    case DisablingWan:
        closePort();
        QProcess::execute("ifconfig", QStringList() << "hso0" << "down");
        writeResolvConf("8.8.8.8", "208.67.222.222");
        setState(HsoInterface::Down);
        break;
    default:
        break;
    }
}

void HsoInterface::wanCallNotification(QString /*result */ )
{
    port->atchat()->chat("AT_OWANDATA?", this,
                         SLOT(atFinished(bool, QAtResult)));
}

static bool parseWanData(QString result, QString & ip, QString & dns1,
                         QString & dns2)
{
    QStringList parts = result.split(", ");
    if (parts.count() < 5) {
        qWarning() << "hso: unknown format of " << result;
        return false;
    }
    ip = parts[1];
    dns1 = parts[3];
    dns2 = parts[4];
    return true;
}

void HsoInterface::wanDataNotification(QString result)
{
    closePort();

    QString ip, dns1, dns2;
    if (!parseWanData(result, ip, dns1, dns2)) {
        setState(HsoInterface::Down);
        return;
    }

    qLog(Network) << "hso wan call ip=" << ip << ", dns1=" << dns1 << ", dns2="
        << dns2;

    writeResolvConf(dns1, dns2);

    int ret =
        QProcess::execute("ifconfig", QStringList() << "hso0" << ip << "up");
    if (ret) {
        qWarning() << "hso: ifconfig failed with " << ret;
        setState(HsoInterface::Down);
        return;
    }

    ret =
        QProcess::execute("route",
                          QStringList() << "add" << "default" << "dev" <<
                          "hso0");
    if (ret) {
        qWarning() << "hso: route failed with " << ret;
        setState(Down);
        return;
    }

    setState(HsoInterface::Up);
}

QString HsoInterface::device() const
{
    return "hso0";
}

QtopiaNetwork::Type HsoInterface::type()const
{
    return QtopiaNetwork::toType(configIface->configFile());
}

bool HsoInterface::isAvailable()
{
    return true;
}

bool HsoInterface::isActive()
{
    //TODO support for IPv4 only (PF_INET6)
    int inetfd = socket(PF_INET, SOCK_DGRAM, 0);
    if (inetfd == -1)
        return false;

    int flags = 0;
    struct ifreq ifreqst;
    strcpy(ifreqst.ifr_name, "hso0");
    int ret = ioctl(inetfd, SIOCGIFFLAGS, &ifreqst);
    if (ret == -1) {
        int error = errno;
        qLog(Network) << "HsoInterface: " << strerror(error);
        ::close(inetfd);
        return false;
    }

    flags = ifreqst.ifr_flags;
    if ((flags & IFF_UP) == IFF_UP) {
        qLog(Network) << "HsoInterface: hso0 is up and running";
        ::close(inetfd);
        return true;
    }

    qLog(Network) << "HsoInterface: device is offline";
    ::close(inetfd);
    return false;
}

QtopiaNetworkConfiguration *HsoInterface::configuration()
{
    return configIface;
}

void HsoInterface::setProperties(const QtopiaNetworkProperties & properties)
{
    configIface->writeProperties(properties);
}
