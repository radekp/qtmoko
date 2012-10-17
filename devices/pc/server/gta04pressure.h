/****************************************************************************
**
** This file is part of the Qt Extended Opensource Package.
**
** Copyright (C) 2009 Trolltech ASA.
** Copyright (C) 2012 Radek Polak.
** Copyright (C) 2012 Neil Jerram.
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

#ifndef GTA04PRESSURE_H
#define GTA04PRESSURE_H

#include <QTimer>
#include <QObject>
#include <QProcess>
#include <QTcpSocket>
#include <QValueSpaceObject>
#include <QValueSpaceItem>

#include <qvaluespace.h>
#include <linux/input.h>

class QBootSourceAccessoryProvider;

class QSocketNotifier;

class Gta04Pressure : public QObject
{
    Q_OBJECT
public:
    Gta04Pressure();
    ~Gta04Pressure();

private:
    QTimer timer;
    QValueSpaceObject *pressure_space;

private slots:
    void updateStatus();
};

#endif
