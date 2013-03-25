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

#include <QDebug>
#include <QWhereabouts>
#include <qgpsdwhereabouts.h>
#include <qtopialog.h>

/*
  The benefit of using GPSD, instead of reading the GPS device
  directly, is that multiple clients can access GPS information at the
  same time.  For example, GPS can be simultaneously used by a local
  application such as NeronGPS, and exported over Bluetooth to another
  device.
*/
NeoGpsPlugin::NeoGpsPlugin(QObject * parent)
:  QWhereaboutsPlugin(parent)
{
    qLog(Hardware) << __PRETTY_FUNCTION__;
    system("/opt/qtmoko/bin/gps-poweron.sh");
}

NeoGpsPlugin::~NeoGpsPlugin()
{
    system("/opt/qtmoko/bin/gps-poweroff.sh");
}

QWhereabouts *NeoGpsPlugin::create(const QString &)
{
    qLog(Hardware) << __PRETTY_FUNCTION__;

    QGpsdWhereabouts *whereabouts = new QGpsdWhereabouts;

    return whereabouts;
}

QTOPIA_EXPORT_PLUGIN(NeoGpsPlugin)
