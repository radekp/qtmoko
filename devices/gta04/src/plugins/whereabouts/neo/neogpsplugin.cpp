/****************************************************************************
**
** This file is part of the Qt Extended Opensource Package.
**
** Copyright (C) 2009 Trolltech ASA.
** Copyright (C) 2012 Radek Polak
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
#include <QDebug>

#include <QTimer>
#include <QWhereabouts>
#include <QNmeaWhereabouts>
#include <QMessageBox>
#include <qtopialog.h>

/*
  This plugin uses "gpspipe -r" to get NMEA sentences out of GPSD,
  then feeds those to QNmeaWhereabouts.  It should work on any
  distribution where GPSD is running and successfully accessing the
  GPS device.

  The benefit of using GPSD, instead of reading the GPS device
  directly, is that multiple clients can access GPS information at the
  same time.  For example, GPS can be simultaneously used by a local
  application such as NeronGPS, and exported over Bluetooth to another
  device.

  An alternative GPSD-based solution would be to use QGpsdWhereabouts
  instead of QNmeaWhereabouts, but that would require updating
  QGpsdWhereabouts for GPSD's new JSON-based protocol.
*/
NeoGpsPlugin::NeoGpsPlugin(QObject * parent)
:  QWhereaboutsPlugin(parent)
{
    qLog(Hardware) << __PRETTY_FUNCTION__;
    system("/opt/qtmoko/bin/gps-poweron.sh");
}

NeoGpsPlugin::~NeoGpsPlugin()
{
    if(reader) {
        reader->terminate();
        if(!reader->waitForFinished(1000))
            reader->kill();
        delete(reader);
    }
    
    system("/opt/qtmoko/bin/gps-poweroff.sh");
}

QWhereabouts *NeoGpsPlugin::create(const QString &)
{
    qLog(Hardware) << __PRETTY_FUNCTION__;

    reader = new QProcess(this);
    reader->start("gpspipe", QStringList() << "-r", QIODevice::ReadWrite);

    if (!reader->waitForStarted()) {
        qWarning() << "Couldn't start gpspipe -r: " + reader->errorString();
        QMessageBox::warning(0, tr("GPS"),
                             tr("Couldn't start gpspipe -r"),
                             QMessageBox::Ok, QMessageBox::Ok);
        delete reader;
        reader = 0;
        return 0;
    }

    QNmeaWhereabouts *whereabouts =
        new QNmeaWhereabouts(QNmeaWhereabouts::RealTimeMode, this);
    whereabouts->setSourceDevice(reader);

    if (!reader->waitForReadyRead(3000))
        system("/opt/qtmoko/bin/gps-toggle.sh");

    return whereabouts;
}

QTOPIA_EXPORT_PLUGIN(NeoGpsPlugin)
