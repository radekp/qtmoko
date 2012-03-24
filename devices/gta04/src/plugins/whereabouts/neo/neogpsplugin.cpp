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

#include "neogpsplugin.h"

#include <QFile>
#include <QSettings>
#include <QSocketNotifier>
#include <QDebug>

#include <QWhereabouts>
#include <QNmeaWhereabouts>
#include <QMessageBox>
#include <qtopialog.h>

#define NMEA_GPS_DEVICE "/dev/ttyO1"
/*
 This plugin only works for Neo neo Freerunner
*/
NeoGpsPlugin::NeoGpsPlugin(QObject *parent)
    : QWhereaboutsPlugin(parent)
{
    qLog(Hardware) << __PRETTY_FUNCTION__;
    system("/opt/qtmoko/bin/gps-poweron.sh");
}

NeoGpsPlugin::~NeoGpsPlugin()
{
    system("/opt/qtmoko/bin/gps-poweroff.sh");
}

QWhereabouts *NeoGpsPlugin::create(const QString &source)
{
    qLog(Hardware) << __PRETTY_FUNCTION__;
    QString path = source;
    if (path.isEmpty()) {
#ifdef NMEA_GPS_DEVICE
        path = NMEA_GPS_DEVICE;
#endif
        QSettings cfg("Trolltech",  "Whereabouts");
        cfg.beginGroup("Hardware");
        path = cfg.value("Device", path).toString();
    }

    QFile *sourceFile = new QFile(path, this);
    if (!sourceFile->open(QIODevice::ReadOnly)) {
        QMessageBox::warning( 0,tr("Mappingdemo"),tr("Cannot open GPS device at %1").arg(path),
            QMessageBox::Ok,  QMessageBox::Ok);
        qWarning() << "Cannot open GPS device at" << path;
        delete sourceFile;
        return 0;
    }

    QNmeaWhereabouts *whereabouts = new QNmeaWhereabouts(QNmeaWhereabouts::RealTimeMode, this);
    whereabouts->setSourceDevice(sourceFile);

    // QFile does not emit readyRead(), so we must call
    // newDataAvailable() when necessary
    QSocketNotifier *notifier = new QSocketNotifier(sourceFile->handle(), QSocketNotifier::Read, this);
    connect(notifier, SIGNAL(activated(int)),
              whereabouts, SLOT(newDataAvailable()));

    return whereabouts;
}

QTOPIA_EXPORT_PLUGIN(NeoGpsPlugin)
