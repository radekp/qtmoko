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

#include "lan.h"
#include "config.h"
#include "roamingmonitor.h"
#include "wlanregistrationprovider.h"

#include <errno.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <custom.h>
#include <qtopialog.h>
#include <qtopianamespace.h>
#include <QDebug>

static const QString lanScript = Qtopia::qtopiaDir()+"/bin/lan-network";
static QMap<QString,QString>* devToConfig = 0;

static void map_cleanup()
{
    if ( devToConfig ) {
        devToConfig->clear();
        delete devToConfig;
        devToConfig = 0;
    }
};

LanImpl::LanImpl( const QString& confFile)
    : configIface(0), ifaceStatus(Unknown)
#ifndef NO_WIRELESS_LAN
    , roaming( 0 ), wlanRegProvider( 0 )
#endif
    , netSpace( 0 ), wifiSpace(0), delayedGatewayInstall( false )
{
    if ( !devToConfig ) {
        devToConfig = new QMap<QString,QString>();
        qAddPostRoutine(map_cleanup);
    }

    qLog(Network) << "Creating LanImpl instance";
    configIface = new LANConfig( confFile );

    //update state of this interface after each script execution
    connect( &thread, SIGNAL(scriptDone()), this, SLOT(updateState()));
}

LanImpl::~LanImpl()
{
    if (configIface)
        delete configIface;
    configIface = 0;
    qLog(Network) << "Deleting LanImpl instance";
}

void LanImpl::updateNetSpaceStatus()
{
    netSpace->setAttribute( "State", (int)ifaceStatus );
    if(wifiSpace)
        wifiSpace->setAttribute( "State", (int)ifaceStatus );
}

QtopiaNetworkInterface::Status LanImpl::status()
{
    //don't change status - initialize has not been called yet
    if ( ifaceStatus == QtopiaNetworkInterface::Unknown) {
        return ifaceStatus;
    }

    QtopiaNetworkInterface::Status status = QtopiaNetworkInterface::Unavailable;

    if ( isAvailable() ) {
        status = QtopiaNetworkInterface::Down;

        if ( thread.remainingTasks() > 0 ) {
            //still some jobs to do -> don't do anything until they are done
            status = ifaceStatus;
        } else {
            switch( ifaceStatus ) {
                case QtopiaNetworkInterface::Demand:
                    status = ifaceStatus;
                    break;
                case QtopiaNetworkInterface::Pending:
                default:
                    if ( isActive() )
                        status = QtopiaNetworkInterface::Up;
                    break;
            }
        }
    }

    ifaceStatus = status;
    updateNetSpaceStatus();
    updateTrigger();
    return ifaceStatus;
}

void LanImpl::initialize()
{
    if ( !netSpace ) {
        QString  path = QString("/Network/Interfaces/%1").arg( qHash( configIface->configFile() ) );
        netSpace = new QValueSpaceObject( path, this );
        netSpace->setAttribute( "Config", configIface->configFile() );
        updateNetSpaceStatus();
        netSpace->setAttribute( "ErrorString", tr("Interface hasn't been initialized yet.") );
        netSpace->setAttribute( "Error", QtopiaNetworkInterface::NotInitialized );
        netSpace->setAttribute( "NetDevice", QVariant() );
        netSpace->setAttribute( "UpdateTrigger" , 0 );
    }

    if ( isAvailable() ) {
        qLog(Network) << "LanImpl: Using network interface: " <<deviceName;
        if ( isActive() )
            ifaceStatus = QtopiaNetworkInterface::Up;
        else
            ifaceStatus = QtopiaNetworkInterface::Down;
    } else {
        ifaceStatus = QtopiaNetworkInterface::Unavailable;
        qLog(Network) << "LanImpl: interface not available";
    }

#ifndef NO_WIRELESS_LAN
    QtopiaNetwork::Type t = type();
    if ( t & QtopiaNetwork::WirelessLAN ) {
        roaming = new RoamingMonitor( configIface, this );
        connect( roaming, SIGNAL(changeNetwork()), this, SLOT(reconnectWLAN()) );

        wlanRegProvider = new WlanRegistrationProvider( QString::number(qHash(configIface->configFile())), this );
        wlanRegProvider->initialize();
        
        if (!wifiSpace )
            wifiSpace = new QValueSpaceObject( "/Network/Interfaces/Wifi", this );
    }
#endif

    updateNetSpaceStatus();
    updateTrigger();
}

void LanImpl::cleanup()
{
    if ( ifaceStatus != QtopiaNetworkInterface::Unknown ) {
        ifaceStatus = QtopiaNetworkInterface::Unknown;
        updateNetSpaceStatus();
        updateTrigger();
    } else {
        return;
    }

    QStringList params;
    params << "cleanup"; //no tr
    thread.addScriptToRun( lanScript, params );

    //Wait for lanScript to finish before removing network device
    while ( thread.remainingTasks() > 0 ) {
        qWarning("Network: lanScript still running");
    }

    qWarning("Network: Removing network device");
    QString key = devToConfig->key( configIface->configFile() );
    if ( !key.isEmpty() )
        devToConfig->remove( key );
    qWarning("Network: Network device removed");
}

/*
   lan-network route <iface> [-gw <gateway-IP>]
*/
bool LanImpl::setDefaultGateway()
{
    if ( deviceName.isEmpty() ) {
        updateTrigger( QtopiaNetworkInterface::UnknownError,
                tr("Cannot set default gateway.") );
        qLog(Network) << "Cannot set default gateway";
        return false;
    }

    QString prefix = "Properties/";
#ifndef NO_WIRELESS_LAN
    if ( type() & QtopiaNetwork::WirelessLAN ) {
        prefix = QString("WirelessNetworks/%1/").arg(netIndex);
    }
#endif

    qLog(Network) << "Settings default gateway to" <<configIface->configFile();
    QStringList args;
    args << "route";
    args << deviceName;

    const bool dhcp  = configIface->property(prefix+"DHCP").toString() != "n";
    if ( !dhcp ) {
        QString gateway = configIface->property(prefix+"GATEWAY").toString();
        args << "-gw";
        args << gateway;
    }
    thread.addScriptToRun( lanScript, args );

    //new gateway may require new dns
    installDNS( dhcp );
    return true;
}

void LanImpl::installDNS(bool dhcp)
{
    if ( deviceName.isEmpty() )
        return;

    // if dns server is passed use them otherwise assume dns info via dhcp
    // ### install eth0 dns [<dns1> <dns2>]
    QStringList list;
    list << "install";
    list << deviceName;
    list << "dns";
    if ( !dhcp ) {
        QString prefix = "Properties/";
#ifndef NO_WIRELESS_LAN
        if ( type() & QtopiaNetwork::WirelessLAN ) {
            prefix = QString("WirelessNetworks/%1/").arg(netIndex);
        }
#endif
        list << configIface->property(prefix+"DNS_1").toString();
        list << configIface->property(prefix+"DNS_2").toString();
    }

    //write dns info
    thread.addScriptToRun( lanScript, list );
}

bool LanImpl::start( const QVariant options )
{
    const QtopiaNetworkProperties prop = configIface->getProperties();

    bool writeToSystem = prop.value("Info/WriteToSystem").toBool();
#ifndef NO_WIRELESS_LAN
    netIndex = 0;
    QtopiaNetwork::Type t = type();
    if ( t & QtopiaNetwork::WirelessLAN ) {
        QString essid = options.toString();
        netIndex = roaming->selectWLAN( essid );

        if (netIndex <= 0) {
            updateTrigger( QtopiaNetworkInterface::NotConnected,
                    tr("No WLAN found in sourrounding area") );
            qLog(Network) << "Invalid WLAN selected";
            return false;
        }

        writeToSystem = true; //we always have to update the config since we change it very often
        roaming->activeNotification( true );
    }
#else
    Q_UNUSED( options );
#endif
    if ( ifaceStatus != QtopiaNetworkInterface::Down || deviceName.isEmpty() ) {
        switch ( ifaceStatus )
        {
            case QtopiaNetworkInterface::Unknown:
                updateTrigger( QtopiaNetworkInterface::NotInitialized,
                        tr("Interface hasn't been initialized yet.") );
                break;
            case QtopiaNetworkInterface::Unavailable:
                updateTrigger( QtopiaNetworkInterface::NotAvailable,
                        tr("Interface is not available.") );
                break;
            case QtopiaNetworkInterface::Up:
            case QtopiaNetworkInterface::Pending:
            case QtopiaNetworkInterface::Demand:
                updateTrigger( QtopiaNetworkInterface::NotConnected,
                        tr("Interface already started/active.") );
                break;
            default:
                break;
        }
        qLog(Network) << "LAN interface cannot be started "
            << configIface->configFile();
        return false;
    }


    if ( writeToSystem ) {
        qLog(Network) << "Installing new set of configuration for " << deviceName;

        QStringList params;
        params << "install";
        params << deviceName;

        QString prefix = "Properties/";
#ifndef NO_WIRELESS_LAN
        if ( t & QtopiaNetwork::WirelessLAN ) {
            prefix = QString("WirelessNetworks/%1/").arg(netIndex);
        }
#endif

        const bool dhcp = prop.value(prefix+"DHCP").toString() != "n";
        if ( dhcp ) {
            // ### install <iface> dhcp
            // dhcp takes care of everything
            params << "dhcp";
        } else {
            // ### install <iface> static <ip> <netmask> <broadcast> <gateway>
            params << "static";

            bool missingOption = false;


            QString temp = prop.value(prefix+"IPADDR").toString();
            if ( temp.isEmpty() ) {
                updateTrigger( QtopiaNetworkInterface::NotConnected,
                        tr("IP address missing.") );
                qLog(Network) << "IP address missing";
                missingOption = true;
            } else {
                params << temp;
            }

            if ( missingOption )
                return false;

            temp = prop.value(prefix+"SUBNET").toString();
            if ( temp.isEmpty() ) {
                qLog(Network) << "Subnet mask missing";
                updateTrigger( QtopiaNetworkInterface::NotConnected,
                        tr("Subnet mask missing.") );
                missingOption = true;
            } else {
                params << temp;
            }

            if ( missingOption )
                return false;

            QString ip = temp; //save ip in case we need it as gateway address

            temp = prop.value(prefix+"BROADCAST").toString();
            if ( temp.isEmpty() ) {
                updateTrigger( QtopiaNetworkInterface::NotConnected,
                        tr("Broadcast address missing.") );
                qLog(Network) << "Broadcast address missing";
                missingOption = true;
            } else {
                params << temp;
            }
            
            if ( missingOption )
                return false;

            temp = prop.value(prefix+"GATEWAY").toString();
            if ( temp.isEmpty() ) {
                qLog(Network) << "Gateway address missing. Using IP address.";
                params << ip;
            } else {
                params << temp;
            }
        }

        thread.addScriptToRun( lanScript, params );

#ifndef NO_WIRELESS_LAN
        if ( t & QtopiaNetwork::WirelessLAN ) {
            // ### install <iface> wireless
            // ###    [-essid <ESSID>] [-mode <Master|Managed|Ad-Hoc>]
            // ###    [-ap <AP>] [-bitrate <value>] [-nick <nickname>] [-channel <CHANNEL>]
            // ###    [-authmode <open|shared> -multikey <defaultKey> <key1> <key2> <key3> <key4>]
            // ###    [-authmode <open|shared> -phrase <passphrase> ]
            // ###    [-authmode <none> -nokey ]
            // ###    [-authmode <WPA-PSK> <password> <AES|TKIP>
            // ###    [-authmode <WPA-EAP> <TTLS|PEAP> <client cert> <server cert> ]
            // ###    [-authmode <WPA-EAP> <TLS> <identity> <password> ]
            // ###    [-keylength <128|64> ]

            const QString keyPrefix = QString("WirelessNetworks/%1/").arg(netIndex);
            params.clear();
            params << "install";
            params << deviceName;
            params << "wireless";

            QString temp = prop.value(keyPrefix+"WirelessMode").toString();
            if ( !temp.isEmpty() ) {
                params << "-mode";
                params << temp;
            }

            temp = prop.value(keyPrefix+"ESSID").toString();
            if ( !temp.isEmpty() ) {
                params << "-essid";
                params << Qtopia::shellQuote(temp);
                if ( t & QtopiaNetwork::WirelessLAN ) {
                    wlanRegProvider->setAccessPoint( temp );
                }
            }

            params << "-ap";
            params << Qtopia::shellQuote( prop.value(keyPrefix+"AccessPoint").toString() );

            temp = prop.value(keyPrefix+"BitRate").toString();
            if ( !temp.isEmpty() ) {
                params << "-bitrate";
                params << temp;
            }

            temp = prop.value(keyPrefix+"Nickname").toString();
            if ( !temp.isEmpty() ) {
                params << "-nick";
                params << Qtopia::shellQuote(temp);
            }

            temp = prop.value(keyPrefix+"CHANNEL").toString();
            if ( !temp.isEmpty() ) {
                params << "-channel";
                params << temp;
            }

            temp = prop.value(keyPrefix+"KeyLength", QString("128")).toString();
            params << "-keylength";
            params << temp;

            QString encryptMode = prop.value(keyPrefix+"Encryption", QString("open")).toString();
            params << "-authmode";
            params << encryptMode;

            if ( encryptMode == "open" || encryptMode == "shared" ) {
                const QString defaultKey = prop.value(keyPrefix+"SelectedKey").toString();
                const QString pw = prop.value(keyPrefix+"PRIV_GENSTR").toString();
                const bool isPassPhrase = defaultKey == "PP";
                if ( isPassPhrase ) {
                    params << "-phrase";
                    params << Qtopia::shellQuote(pw);
                } else {
                    params << "-multikey";
                    params << defaultKey.right(1);
                    for ( int i = 1; i<5; i++ ) {
                        params << Qtopia::shellQuote(
                            prop.value(keyPrefix+"WirelessKey_"+QString::number(i)).toString());
                    }
                }
            } else if ( encryptMode == QLatin1String("none") ) {
                params << "-nokey";
            } else if ( encryptMode == QLatin1String("WPA-PSK") ) {
                params << Qtopia::shellQuote(prop.value(keyPrefix+QLatin1String("PRIV_GENSTR"), QLatin1String("")).toString());
                params << prop.value(keyPrefix+QLatin1String("PSKAlgorithm"), QLatin1String("TKIP")).toString();
            } else if ( encryptMode == QLatin1String("WPA-EAP") ) {
                temp = prop.value(keyPrefix+QLatin1String("WPAEnterprise"), QLatin1String("TLS")).toString();
                params << temp;
                if ( temp == QLatin1String("TLS") ) {
                    params << prop.value(keyPrefix+QLatin1String("EAPIdentity")).toString();
                    params << prop.value(keyPrefix+QLatin1String("EAPClientKeyPassword")).toString();
                    params << prop.value(keyPrefix+QLatin1String("EAPClientKey")).toString();
                    params << prop.value(keyPrefix+QLatin1String("EAPClientCert")).toString();
                    params << prop.value(keyPrefix+QLatin1String("EAPServerCert")).toString();
               } else if ( temp == QLatin1String("TTLS") || temp == QLatin1String("PEAP") ) {
                    params << prop.value(keyPrefix+QLatin1String("EAPIdentity")).toString();
                    params << prop.value(keyPrefix+QLatin1String("EAPIdentityPassword")).toString();
                    params << prop.value(keyPrefix+QLatin1String("EAPServerCert")).toString();
                    params << prop.value(keyPrefix+QLatin1String("EAPAuthentication")).toString();
                    params << prop.value(keyPrefix+QLatin1String("EAPAnonIdentity")).toString();
               } else {
                    qLog(Network) << QLatin1String("Unknown encryption algorithm for WPA Enterprise");
                    return false;
               }
            } else {
                qLog(Network) << QLatin1String("Invalid encryption for WLAN:") << encryptMode;
                return false;
            }

            thread.addScriptToRun( lanScript, params );
        }
#endif

        //Remove WriteToSystem value
        QtopiaNetworkProperties p;
        p.insert("Info/WriteToSystem", false);
        configIface->writeProperties( p );
    }

    QStringList args;
    // ### start <iface>
    args << "start";
    args << deviceName;
    thread.addScriptToRun( lanScript, args );
    //we have to wait a bit until this interface is actually online
    //->then it can become the default gateway ->installs dns details as well
    ifaceStatus = QtopiaNetworkInterface::Pending;
    updateNetSpaceStatus();
    updateTrigger();
    delayedGatewayInstall = true;
    return true;
}

bool LanImpl::stop()
{
#ifndef NO_WIRELESS_LAN
    if ( type() & QtopiaNetwork::WirelessLAN )
        roaming->activeNotification( false );
#endif
    switch (ifaceStatus ) {
        case QtopiaNetworkInterface::Pending:
        case QtopiaNetworkInterface::Demand:
        case QtopiaNetworkInterface::Up:
            break;
        case QtopiaNetworkInterface::Unknown:
        case QtopiaNetworkInterface::Unavailable:
        case QtopiaNetworkInterface::Down:
        default:
            updateTrigger( QtopiaNetworkInterface::UnknownError,
                    tr("Interface is not running.") );
            return true;
    }

#ifndef NO_WIRELESS_LAN
    if ( type() & QtopiaNetwork::WirelessLAN ) {
        wlanRegProvider->setAccessPoint();
        wlanRegProvider->notifyClients();
    }
#endif
    // ### stop eth0
    QStringList args;
    args << "stop";
    args << deviceName;
    thread.addScriptToRun( lanScript, args );
    updateTrigger();
    return true;
}

QString LanImpl::device() const
{
    return deviceName;
}

QtopiaNetwork::Type LanImpl::type() const
{
    return QtopiaNetwork::toType( configIface->configFile() );
}

/*!
  \internal

  Returns true if \a dev is a PCMCIA device.
*/
bool LanImpl::isPCMCIADevice( const QString& dev ) const
{
    FILE* f = fopen("/var/run/stab", "r");
    if (!f) f = fopen("/var/state/pcmcia/stab", "r");
    if (!f) f = fopen("/var/lib/pcmcia/stab", "r");

    if ( f ) {
        char buffer[1024];
        while ( fgets( buffer, 1024, f ) ) {
            if ( strstr( buffer, "network") && strstr( buffer, dev.toAscii().constData() ) ) {
                fclose( f );
                return true;
            }
        }
        fclose( f );
    }

    return false;
}

/*!
  Returns \c true if \a device is specifically assigned to the current configuration
  or nobody else holds a lock on it; otherwise \c false. This removes the indeterministic
  assignment of Linux network interface to Qt Extended network configurations.
  */
bool LanImpl::isAvailableDevice(const QString& device) const
{
    const QString assignedDevice = configIface->property("Properties/DeviceName" ).toString();
    if ( !assignedDevice.isEmpty() )
    {
        if ( assignedDevice == device ) {
            qLog(Network) << "Testing assigned device only:" << device;
            return true;
        }
    }
    else
    {
        QtopiaNetwork::Type staticNetworkTypes[] = {
             QtopiaNetwork::LAN, QtopiaNetwork::WirelessLAN };
        for (uint i=0; i< sizeof( staticNetworkTypes )/sizeof(QtopiaNetwork::Type); i++ )
        {
            QStringList ifaces =
                QtopiaNetwork::availableNetworkConfigs( staticNetworkTypes[i] );
            //exclude the current config
            ifaces.removeAll( configIface->configFile() );
            for( int j = 0; j< ifaces.size(); j++ ) {
                QSettings cfg( ifaces[j], QSettings::IniFormat );
                QString devName = cfg.value( "Properties/DeviceName", QString() ).toString();
                if ( devName.isEmpty() ) {
                    continue;
                }
                if ( devName == device ) {
                    return false; // ifaces[j] holds a lock on \a device
                }
            }
        }
        //nobody holds a lock on this device
        qLog(Network) << "Testing potential device" << device << "for" << configIface->configFile();
        return true;
    }
    return false;
}

bool LanImpl::isAvailable() const
{
    const QtopiaNetwork::Type t = type();
    const bool wireless = (t & QtopiaNetwork::WirelessLAN);
    //const bool scannerRequested = configIface->property("WirelessNetworks/Timeout").toInt() > 0;
    int sock = -1;
    if ( (sock = socket( AF_INET, SOCK_DGRAM, 0)) < 0 ) {
        qLog(Network) << "Cannot open INET socket";
        return false;
    }

    FILE* f = fopen("/proc/net/dev", "r");
    if ( f ) {
        char line[1024];
        qLog(Network) << "LanImpl: Searching for (W)LAN network interfaces for " << configIface->configFile();
        while ( fgets( line, 1024, f ) ) {
            QString buffer(line);
            int index = buffer.indexOf( QChar(':') );
            if ( index >= 0 ) {
                QString dev = buffer.left(index).trimmed();
                if ( dev.startsWith( "eth" ) || dev.startsWith( "wlan" ) || dev.startsWith("usb") ) {
                    //it is a wireless or ethernet
                    int flags = 0;
                    struct ifreq ifrq2;

                    strcpy( ifrq2.ifr_name, dev.toLatin1().constData() );
                    if ( ioctl( sock, SIOCGIFFLAGS, &ifrq2 ) < 0 ) {
                        qLog(Network) << "SIOCGIFFLAGS failed";
                        continue;
                    }
                    flags = ifrq2.ifr_flags;
                    if ( (flags & IFF_BROADCAST) != IFF_BROADCAST ) //we want ethernet
                        continue;

                    //don't grab this device if another (W)LAN config holds exclusive right
                    //on this device
                    //isAvailableDevice() is expensive -> only do this when we don't have an initial
                    //assignment yet
                    if ( deviceName.isEmpty() && !isAvailableDevice( dev ) )
                        continue;

                    bool isPCMCIA = isPCMCIADevice( dev );
                    //PCMCIAMatch is true if the configuration file has the same
                    //type as the device in /proc/net/dev <==> !(a^b)
                    bool PCMCIAMatch =
                             (isPCMCIA && (t & QtopiaNetwork::PCMCIA))
                             || (!isPCMCIA && !(t & QtopiaNetwork::PCMCIA));

#ifndef NO_WIRELESS_LAN
                    struct iwreq wrq;
                    strcpy( wrq.ifr_name, dev.toLatin1().constData() );
                    int ret = ioctl( sock, SIOCGIWNAME, &wrq );
                    //ret is <0 if this device is not a wireless device
                    if ( ret < 0 ) {
#endif
                        if ( !wireless &&
                            //check that nobody else apart from this config uses this device already
                            (!devToConfig->contains( dev) || devToConfig->value(dev) == configIface->configFile() ))
                        {
                            if ( PCMCIAMatch )
                            {
                                qLog(Network) << "Using ethernet interface" << dev << (isPCMCIA ? "on PCMCIA device" : "");
                                deviceName = dev;
                                netSpace->setAttribute( "NetDevice", deviceName );
                                devToConfig->insert( deviceName, configIface->configFile() );
                                fclose( f );
                                ::close( sock );
                                return true;
                            }
                        }
#ifndef NO_WIRELESS_LAN
                    } else{
                        if ( wireless &&
                            //check that nobody else apart from this config uses this device already
                            ( !devToConfig->contains( dev ) || devToConfig->value(dev) == configIface->configFile() ) )
                        {
                            if ( PCMCIAMatch )
                            {
                                qLog(Network) << "Using wireless interface" << dev << (isPCMCIA ? "on PCMCIA device" : "");
                                deviceName = dev;
                                netSpace->setAttribute( "NetDevice", deviceName );
                                devToConfig->insert( deviceName, configIface->configFile() );
                                fclose( f );
                                ::close( sock );
                                return true;
                            }
                        }
                    }
#endif
                }
            }
        }
        fclose( f );
    }

    //we couldn't find a suitable device ->
    //make sure we don't have a device assigned to this config
    if ( !deviceName.isEmpty()
            && devToConfig->contains( deviceName )
            && devToConfig->value( deviceName ) == configIface->configFile() ) {
        //this deviceName was assigned to this plugin => remove it now so that
        //we can assign it to a different plugin at a later stage
       devToConfig->remove( deviceName );
    }
    deviceName = QString();
    netSpace->setAttribute( "NetDevice", QVariant() );

    ::close( sock );

    qLog(Network) << "LanImpl: No (W)LAN network interface found";
    return false;
}

bool LanImpl::isActive() const
{
    if (deviceName.isEmpty())
        return false;

    //TODO support for IPv4 only (PF_INET6)
    int inetfd = socket( PF_INET, SOCK_DGRAM, 0 );
    if ( inetfd == -1 )
        return false;

    int flags = 0;
    struct ifreq ifreqst;
    strcpy( ifreqst.ifr_name, deviceName.toLatin1().constData() );
    int ret = ioctl( inetfd, SIOCGIFFLAGS, &ifreqst );
    if ( ret == -1 ) {
        int error = errno;
        qLog(Network) << "LanImpl: " << strerror( error );
        ::close( inetfd );
        return false;
    }


    flags = ifreqst.ifr_flags;
    if ( ( flags & IFF_UP ) == IFF_UP  &&
            (flags & IFF_LOOPBACK) != IFF_LOOPBACK &&
            (flags & IFF_BROADCAST) == IFF_BROADCAST ) {
        //qLog(Network) << "LanImpl: " <<  deviceName << " is up and running";
        ::close( inetfd );
        return true;
    }

    qLog(Network) << "LanImpl: device is offline" ;
    ::close( inetfd );
    return false;
}

QtopiaNetworkConfiguration * LanImpl::configuration()
{
    return configIface;
}

void LanImpl::setProperties( const QtopiaNetworkProperties& properties )
{
    configIface->writeProperties(properties);
}

void LanImpl::updateTrigger( QtopiaNetworkInterface::Error code, const QString& desc )
{
    if ( !netSpace )
        return;
    trigger = (++trigger)%256;
    if ( !desc.isEmpty() ) //do not override old error string if nothing to report
        netSpace->setAttribute( "ErrorString", desc );
    netSpace->setAttribute( "Error", code );
    netSpace->setAttribute( "UpdateTrigger", trigger );
}

void LanImpl::reconnectWLAN()
{
#ifndef NO_WIRELESS_LAN
    qLog(Network) << "Reconnecting WLAN on interface" << device();device();
    stop();
    start( QVariant() );
#endif
}

void LanImpl::updateState()
{
    status(); //update state first and then set new gateway
    if ( delayedGatewayInstall ) {
        if ( ifaceStatus == QtopiaNetworkInterface::Up ) {
            //the assumption is that the last script starts the network device "lan-network start ethX"
            //once the device is up we can trigger the gateway updates
            if ( thread.remainingTasks() == 0 ) {
                QtopiaNetwork::setDefaultGateway( configIface->configFile() );
                delayedGatewayInstall = false;
            } else {
                qWarning("%s is up but has remaining script tasks.",configIface->configFile().toLatin1().constData());
            }
        } else if ( ifaceStatus == QtopiaNetworkInterface::Down
                || ifaceStatus == QtopiaNetworkInterface::Unavailable ) {
            // do not update gateway when we suddenly drop out during the startup of the network
            delayedGatewayInstall = false;
        } // else { //wait until we are online }
    }
#ifndef NO_WIRELESS_LAN
    QtopiaNetwork::Type t = type();
    if ( t & QtopiaNetwork::WirelessLAN
            && ifaceStatus == QtopiaNetworkInterface::Up )
    {
        wlanRegProvider->notifyClients();
    }
#endif
}
