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

#ifndef DIALUP_H
#define DIALUP_H

#include <QAtResult>
#include <QSerialPort>

#include <qtopianetworkinterface.h>
#include <qtopianetwork.h>
#include <qvaluespace.h>
#include <scriptthread.h>

#ifdef QTOPIA_CELL
#include <qphonecallmanager.h>
#include <qnetworkregistration.h>
#include <qcommservicemanager.h>
#endif

#ifndef QTOPIA_NO_FSO
#include <qfsoutil.h>
#include <qfsogsmdevice.h>
#include <qfsogsmpdp.h>
#endif

class HsoInterface : public QtopiaNetworkInterface
{
    Q_OBJECT
public:
    HsoInterface(const QString & confFile);
     virtual ~ HsoInterface();

    enum State
    { Uninitialized, Down, SettingApn, EnablingWan, GettingWanParams, Up,
            DisablingWan } state;

    virtual Status status();

    virtual void initialize();
    virtual void cleanup();
    virtual bool start(const QVariant options = QVariant());
    virtual bool stop();
    virtual QString device() const;
    virtual bool setDefaultGateway();

    virtual QtopiaNetwork::Type type() const;

    virtual QtopiaNetworkConfiguration *configuration();

    virtual void setProperties(const QtopiaNetworkProperties & properties);

protected:
     bool isAvailable();
    bool isActive();

private:
     QtopiaNetworkConfiguration * configIface;
    QSerialPort *port;

#ifndef QTOPIA_NO_FSO
    QFsoGSMDevice gsmDev;
    QFsoGSMPDP gsmPdp;
    bool fsoEnabled;
#endif

    QValueSpaceObject *netSpace;
    int trigger;

    bool openPort();
    void closePort();
    void setState(State newState);

private slots:
    void atFinished(bool, QAtResult);
    void wanCallNotification(QString);
    void wanDataNotification(QString);

};

#endif
