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

  AT_OWANDATA=1,0,1
  
*/

HsoInterface::HsoInterface( const QString& confFile)
    : state( Uninitialized )
    , configIface(0)
    , port(0)
    , ifaceStatus(Unknown)
#ifndef QTOPIA_NO_FSO
    , gsmDev("org.freesmartphone.ogsmd", "/org/freesmartphone/GSM/Device", QDBusConnection::systemBus(), this)
    , gsmPdp("org.freesmartphone.ogsmd", "/org/freesmartphone/GSM/Device", QDBusConnection::systemBus(), this)
    , fsoEnabled(false)
#endif    
    , netSpace( 0 )
{
    qLog(Network) << "Creating HsoInterface instance";
    configIface = new DialupConfig( confFile );

#ifndef QTOPIA_NO_FSO
    fsoEnabled = !strcmp("Fso", getenv( "QTOPIA_PHONE" ));      // are we using FSO as backend?
#endif
    
    qDebug() << "======= HsoInterface ctor";
}

HsoInterface::~HsoInterface()
{
    qLog(Network) << "Deleting HsoInterface instance";
    if (configIface)
        delete configIface;
    configIface = 0;
}

void HsoInterface::setState(State newState)
{
    qDebug() << "setState " << state << "->" << newState;
    this->state = newState;
    switch(state)
    {
        case SettingApn:
            ifaceStatus = QtopiaNetworkInterface::Pending;
            netSpace->setAttribute("State", ifaceStatus);
            break;
        default:
            break;
    }
}

QtopiaNetworkInterface::Status HsoInterface::status()
{
    qDebug() << "====== HsoInterface::status() ifaceStatus=" << ifaceStatus;
    
    if ( ifaceStatus == QtopiaNetworkInterface::Unknown) {
        return ifaceStatus;
    }

    QtopiaNetworkInterface::Status status = QtopiaNetworkInterface::Unavailable;
    if ( isAvailable() ) {
        status = QtopiaNetworkInterface::Down;

        if ( ifaceStatus == QtopiaNetworkInterface::Pending ||
                ifaceStatus == QtopiaNetworkInterface::Demand )
            // these states are updated by timerEvent
            status = ifaceStatus;
        else if ( isActive() )
            status = QtopiaNetworkInterface::Up;
    }
    
    qDebug() << "====== HsoInterface::status()=" <<  status;

    ifaceStatus = status;
    netSpace->setAttribute( "State", ifaceStatus );
    return ifaceStatus;
}

void HsoInterface::initialize()
{
    if ( !netSpace ) {
        const uint ident = qHash( configIface->configFile() );
        QString path = QString("/Network/Interfaces/%1").arg(ident);
        netSpace = new QValueSpaceObject( path, this );
        netSpace->setAttribute( "Config", configIface->configFile() );
        netSpace->setAttribute( "State", QtopiaNetworkInterface::Unknown );
        netSpace->setAttribute( "Error", QtopiaNetworkInterface::NotInitialized );
        netSpace->setAttribute( "ErrorString", tr("Interface hasn't been initialized yet.") );
        netSpace->setAttribute( "NetDevice", QVariant() );
        netSpace->setAttribute( "UpdateTrigger", 0 );
    }

    qDebug() << "HsoInterface::initialize()";
    if ( isAvailable() ) {
        qDebug() << "1";
        if ( isActive() )
            ifaceStatus = QtopiaNetworkInterface::Up;
        else
            ifaceStatus = QtopiaNetworkInterface::Down;
    } else {
        ifaceStatus = QtopiaNetworkInterface::Unavailable;
    }
    
    qDebug() << "ifaceStatus=" << ifaceStatus;

    netSpace->setAttribute( "State", ifaceStatus );
}

void HsoInterface::cleanup()
{
    if ( ifaceStatus != QtopiaNetworkInterface::Unknown ) {
        ifaceStatus = QtopiaNetworkInterface::Unknown;
        netSpace->setAttribute( "State", ifaceStatus );
    } else {
        return;
    }
}

bool HsoInterface::setDefaultGateway()
{
    QStringList params;
    params << "install";
    params << "dns";
    if ( configIface->property("Serial/UsePeerDNS").toString() == "n" ) {
        params << configIface->property("Properties/DNS_1").toString();
        params << configIface->property("Properties/DNS_2").toString();
    }
 
    return true;
}

bool HsoInterface::start( const QVariant /*options*/ )
{
    qDebug() << "================ HsoInterface::start()";
    
    port = new QSerialPort("/dev/ttyHS_Control", 115200);
    if(!port->open(QIODevice::ReadWrite)) {
        qWarning() << "Failed to open /dev/ttyHS_Control: " << port->errorString();
        delete port;
        port = 0;
        return false;
    }
    
    QAtChat *chat = port->atchat();
    chat->registerNotificationType("_OWANCALL:", this, SLOT(wanCallNotification(QString)));
    chat->registerNotificationType("_OWANDATA:", this, SLOT(wanDataNotification(QString)));
    chat->chat(QString("AT+CGDCONT=1,\"IP\",\"%1\"").arg("internet"), this, SLOT(atFinished(bool,QAtResult)));
    
    setState(SettingApn);
    return true;

    /*bool ok = true;

    ifaceStatus = ok ?
        QtopiaNetworkInterface::Up : QtopiaNetworkInterface::Down;
    
    netSpace->setAttribute("State", ifaceStatus);
    
    return true;*/
}

void HsoInterface::atFinished(bool ok, QAtResult result)
{
    if(!ok) {
        netSpace->setAttribute( "Error", QtopiaNetworkInterface::UnknownError);
        netSpace->setAttribute( "ErrorString", result.verboseResult());
        return;
    }
    
    switch(state)
    {
        case SettingApn:
            port->atchat()->chat("AT_OWANCALL=1,1,1", this, SLOT(atFinished(bool,QAtResult)));
            setState(EnablingWan);
            break;
        case EnablingWan:
            port->atchat()->chat("AT_OWANDATA?", this, SLOT(atFinished(bool,QAtResult)));
            setState(GettingWanParams);
            break;
        case GettingWanParams:
            break;
        default:
            break;
    }        
}

void HsoInterface::wanCallNotification(QString result)
{
    qDebug() << "========= wanCallNotification" << result;

    port->atchat()->chat("AT_OWANDATA?", this, SLOT(atFinished(bool,QAtResult)));
}

static bool parseWanData(QString result, QString & ip, QString & dns1, QString & dns2)
{
    QStringList parts = result.split(", ");
    if(parts.count() < 5) {
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
    qDebug() << "========= wanDataNotification" << result;

    port->close();
    port->deleteLater();
    port = NULL;
    
    QString ip, dns1, dns2;
    if(!parseWanData(result, ip, dns1, dns2)) {
        setState(Down);
        return;
    }

    qLog(Network) << "hso wan call ip=" << ip << ", dns1=" << dns1 << ", dns2=" << dns2;
    
    QFile resolvConf("/etc/resolv.conf");
    if(!resolvConf.open(QIODevice::ReadWrite)) {
        qWarning() << "hso: failed to open /etc/resolv.conf";
        setState(Down);
        return;
    }
    
    resolvConf.write("nameserver ");
    resolvConf.write(dns1.toLatin1());
    resolvConf.write("\nnameserver ");
    resolvConf.write(dns2.toLatin1());
    resolvConf.close();
    
    int ret = QProcess::execute("ifconfig", QStringList() << "hso0" << ip << "up");
    if(ret) {
        qWarning() << "hso: ifconfig failed with " << ret;
        setState(Down);
        return;
    }
    
    ret = QProcess::execute("route", QStringList() << "add" << "default" << "dev" << "hso0");
    if(ret) {
        qWarning() << "hso: route failed with " << ret;
        setState(Down);
        return;
    }
    
    setState(Up);
}


bool HsoInterface::stop()
{
    qDebug() << "================ HsoInterface::stop()";
    ifaceStatus = QtopiaNetworkInterface::Down;
    netSpace->setAttribute("State", ifaceStatus);
    return true;
}

QString HsoInterface::device() const
{
    return "hso0";
}

QtopiaNetwork::Type HsoInterface::type() const
{
    return QtopiaNetwork::toType( configIface->configFile() );
}

bool HsoInterface::isAvailable()
{
    return true;
}

bool HsoInterface::isActive()
{
    qDebug() << "=== HsoInterface::isActive()";
    
    //TODO support for IPv4 only (PF_INET6)
    int inetfd = socket( PF_INET, SOCK_DGRAM, 0 );
    if ( inetfd == -1 )
        return false;

    int flags = 0;
    struct ifreq ifreqst;
    strcpy( ifreqst.ifr_name, "hso0" );
    int ret = ioctl( inetfd, SIOCGIFFLAGS, &ifreqst );
    if ( ret == -1 ) {
        int error = errno;
        qLog(Network) << "HsoInterface: " << strerror( error );
        ::close( inetfd );
        return false;
    }


    flags = ifreqst.ifr_flags;
    qDebug() << "================== flags=" << flags;
    
    if ( ( flags & IFF_UP ) == IFF_UP) {
        qLog(Network) << "HsoInterface: hso0 is up and running";
        ::close( inetfd );
        return true;
    }

    qLog(Network) << "HsoInterface: device is offline" ;
    ::close( inetfd );
    return false;
}

QtopiaNetworkConfiguration * HsoInterface::configuration()
{
    return configIface;
}

void HsoInterface::setProperties( const QtopiaNetworkProperties& properties )
{
    configIface->writeProperties(properties);
}