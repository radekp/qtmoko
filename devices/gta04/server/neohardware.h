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

#include <QTimer>
#include <QObject>
#include <QProcess>
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
    QTcpSocket ueventSocket;
    QTimer timer;
    bool hasSmartBattery;
    QtopiaIpcAdaptor *adaptor;
    QtopiaIpcAdaptor *audioMgr;

private slots:
    void headphonesInserted(bool);
    void shutdownRequested();
    void uevent();
    void updateStatus();
};

#endif
