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

#ifndef LAN_H
#define LAN_H

#include <qtopianetworkinterface.h>
#include <qtopianetwork.h>
#include <qvaluespace.h>
#include <scriptthread.h>
#include <custom.h>

#ifndef NO_WIRELESS_LAN
class RoamingMonitor;
class WlanRegistrationProvider;
#endif

class LanImpl : public QtopiaNetworkInterface
{
    Q_OBJECT
public:
    LanImpl( const QString& confFile );
    virtual ~LanImpl();

    virtual Status status();

    virtual void initialize();
    virtual void cleanup();
    virtual bool start( const QVariant options );
    virtual bool stop();
    virtual QString device() const;
    virtual bool setDefaultGateway();

    virtual QtopiaNetwork::Type type() const;

    virtual QtopiaNetworkConfiguration * configuration();

    virtual void setProperties(
            const QtopiaNetworkProperties& properties);

protected:
    bool isAvailable() const;
    bool isPCMCIADevice( const QString& dev) const;
    bool isActive() const;

private slots:
    void reconnectWLAN();
    void updateState();

private:
    void installDNS(bool);
    bool isAvailableDevice( const QString& ) const;
    void updateTrigger( QtopiaNetworkInterface::Error code = QtopiaNetworkInterface::NoError, const QString& desc = QString() );
    void updateNetSpaceStatus();
    
    QtopiaNetworkConfiguration *configIface;
    Status ifaceStatus;
    mutable QString deviceName;

#ifndef NO_WIRELESS_LAN
    RoamingMonitor* roaming;
    WlanRegistrationProvider* wlanRegProvider;
    int netIndex;
#endif
    QValueSpaceObject *netSpace;
    QValueSpaceObject *wifiSpace;
    ScriptThread thread;
    bool delayedGatewayInstall;
    int trigger;
};

#endif
