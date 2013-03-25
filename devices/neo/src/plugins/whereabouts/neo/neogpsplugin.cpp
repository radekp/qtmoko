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

#include <QDebug>
#include <QWhereabouts>
#include <qgpsdwhereabouts.h>
#include <qtopialog.h>

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
    system("/opt/qtmoko/bin/gps-poweroff.sh");
}

QWhereabouts *NeoGpsPlugin::create(const QString &source)
{
    qLog(Hardware) << __PRETTY_FUNCTION__;

    QGpsdWhereabouts *whereabouts = new QGpsdWhereabouts;

    return whereabouts;
}

QTOPIA_EXPORT_PLUGIN(NeoGpsPlugin)
