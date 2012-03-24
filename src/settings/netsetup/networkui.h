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
#ifndef NETWORKUI_H
#define NETWORKUI_H

#include <QDialog>
#include <QTableWidget>
#include <qtopianetworkinterface.h>
#include <qtopiaabstractservice.h>
#include <QDSActionRequest>

class QAction;
class QTabWidget;
class QMenu;
class QOtaReader;

class NetworkWidgetItem;
class WapUI;
class VpnUI;
class QWspPush;

class NetworkUI : public QDialog
{
    Q_OBJECT
public:
    NetworkUI(QWidget * parent = 0, Qt::WFlags fl = 0);
    ~NetworkUI();

    void setCurrentTab(int tab);

private:
    void init();
    void updateExtraActions(const QString & config,
                            QtopiaNetworkInterface::Status newState);
#ifdef QTOPIA_CELL
    void applyRemoteSettings(const QString & from,
                             const QtopiaNetworkProperties & prop);
#endif
    void addService(const QString & newConfFile);

private slots:
    void addService();
    void removeService();
    void serviceSelected();
    void doAction(QString config, QString type);
    void doProperties();
    void updateActions();
    void updateIfaceStates();
    void tabChanged(int index);
    void setGateway();
    void updateConfig();
    void tableCellClicked(int row, int column);

public slots:
#ifdef QTOPIA_CELL
    void otaDatagram(QOtaReader * reader, const QByteArray & data,
                     const QString & sender);
#endif
    void lanDatagram(const QByteArray & data);

private:
     QTableWidget * table;
    QTabWidget *tabWidget;
    WapUI *wapPage;
    VpnUI *vpnPage;
    QMenu *contextMenu;
     QMultiMap < QString, QPointer < QAction > >actionMap;
    QAction *a_add, *a_remove, *a_props, *a_gateway, *a_startStop;
};

class NetworkSetupService : public QtopiaAbstractService
{
    Q_OBJECT
     friend class NetworkUI;
private:
    NetworkSetupService(NetworkUI * parent)
    :QtopiaAbstractService("NetworkSetup", parent)
    {
        this->parent = parent;
        publishAll();
    }

public:
    ~NetworkSetupService();

public slots:
    void configureData();
#ifdef QTOPIA_CELL
    void configureWap();
    void pushWapNetworkSettings(const QDSActionRequest & request);
    void pushNokiaNetworkSettings(const QDSActionRequest & request);
#endif
    void pushLanNetworkSettings(const QDSActionRequest & request);

private:
    NetworkUI * parent;
};

#endif
