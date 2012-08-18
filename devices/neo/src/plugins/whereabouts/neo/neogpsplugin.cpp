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

#define NMEA_GPS_DEVICE "/dev/ttySAC1"
/*
 This plugin only works for Neo neo Freerunner
*/
NeoGpsPlugin::NeoGpsPlugin(QObject *parent)
    : QWhereaboutsPlugin(parent)
{
    qLog(Hardware) << __PRETTY_FUNCTION__;
    system("/opt/qtmoko/bin/gps-poweron.sh");
    system("stty -F /dev/ttySAC1 -echo");
}

NeoGpsPlugin::~NeoGpsPlugin()
{
    if(reader) {
        reader->terminate();
        if(!reader->waitForFinished(1000))
            reader->kill();
    }
    
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

    reader = new QProcess(this);
    reader->start("cat", QStringList() << "/dev/ttySAC1", QIODevice::ReadWrite);

    if (!reader->waitForStarted()) {
        qWarning() << "couldnt start cat /dev/ttySAC1: " + reader->errorString();
        QMessageBox::warning(0, tr("GPS"),
                             tr("Cannot open GPS device at /dev/ttySAC1"),
                             QMessageBox::Ok, QMessageBox::Ok);
        delete reader;
        reader = 0;
        return 0;
    }

    QNmeaWhereabouts *whereabouts =
        new QNmeaWhereabouts(QNmeaWhereabouts::RealTimeMode, this);
    whereabouts->setSourceDevice(reader);

    return whereabouts;
}

QTOPIA_EXPORT_PLUGIN(NeoGpsPlugin)
