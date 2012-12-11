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

#ifndef WIRELESSSCAN_H
#define WIRELESSSCAN_H

#include <custom.h>
#include "wnet.h"
#ifndef NO_WIRELESS_LAN

//must be defined to be able to include kernel includes
#ifndef __user
#define __user
#endif

#include <linux/types.h>        /* required for wireless.h */
#include <sys/socket.h>         /* required for wireless.h */
#include <net/if.h>             /* required for wireless.h */

/* A lot of wireless.h have kernel includes which should be protected by
   #ifdef __KERNEL__. They course include errors due to redefinitions of types.
   This prevents those kernel headers being included by Qtopia.
   */
#ifndef _LINUX_IF_H
#define _LINUX_IF_H
#endif
#ifndef _LINUX_SOCKET_H
#define _LINUX_SOCKET_H
#endif
#include <linux/wireless.h>

#include <QObject>
#include <QWidget>
#include <QList>
#include <QMessageBox>

#include <qtopianetworkinterface.h>

class WirelessScan : public QObject
{
    Q_OBJECT
public:
    enum ConnectionState
    {
        InterfaceUnavailable,
        NotConnected,
        Connected
    };

     WirelessScan(const QString & ifaceName, bool whileDown, QObject * parent =
                  0);
     virtual ~ WirelessScan();

    QString attachedInterface() const
    {
        return iface;
    };
    const QList < WirelessNetwork > results() const;

    QString currentAccessPoint() const;
    QString currentESSID() const;
    int currentSignalStrength() const;
    ConnectionState deviceState() const;
    bool isScanning()
    {
        return sockfd != -1;
    };

    void rangeInfo(struct iw_range *range, int *weVersion) const;

public slots:
     bool startScanning();
signals:
    void scanningFinished();
private slots:
    void checkResults();
private:
    void readData(unsigned char *data, int length, int weVersion,
                  struct iw_range *range);
    void ensureScanESSID() const;
    bool prepareInterface() const;
    void restoreInterfaceState() const;

    QString iface;
    QList < WirelessNetwork > entries;
    int sockfd;
    bool scanWhileDown;
    bool ifaceDown;
};

class QEvent;
class QLabel;
class QListWidget;
class QListWidgetItem;
class QNetworkDevice;

class WSearchPage : public QWidget
{
    Q_OBJECT
public:
    WSearchPage(const QString & config, QWidget * parent = 0, Qt::WFlags flags =
                0);
     virtual ~ WSearchPage();

    void attachToInterface(const QString & ifaceName);
    void saveScanResults();
    bool eventFilter(QObject * watched, QEvent * event);

public slots:
    void updateConnectivity();

private slots:
    void updateActions(QListWidgetItem * cur, QListWidgetItem * prev);
    void connectToNetwork();
    void deleteNetwork();
    void showAllNetworks();
    void stateChanged(QtopiaNetworkInterface::Status newState, bool error);
    void itemActivated(QListWidgetItem * item);
    void changePriority();
    void startScanning();
    void scanningFinished();

private:
    void initUI();
    void loadKnownNetworks();
    void saveKnownNetworks();
    void updateKnownNetworkList(const WirelessNetwork & record,
                                QListWidgetItem * item = 0);

private:
     QString config;
    WirelessScan *scanEngine;
    QAction *connectAction, *scanAction, *environmentAction, *deleteAction,
        *filterHidden, *priorityAction;
    QListWidget *netList;
    QListWidgetItem *noKnownNetworksItem;
    QLabel *descr;
    QLabel *currentNetwork;
     QtopiaNetworkInterface::Status state;
    QListWidgetItem *currentSelection;
    bool isRestart;
    QNetworkDevice *devSpace;
    bool changingPriority;
};
#endif
#endif
