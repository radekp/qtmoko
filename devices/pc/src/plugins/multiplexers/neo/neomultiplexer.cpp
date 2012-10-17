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

#include "neomultiplexer.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QSettings>
#include <qtopialog.h>
#include <QValueSpaceItem>

#include <qgsm0710multiplexer.h>
#include <qserialport.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <termios.h>

QTOPIA_EXPORT_PLUGIN(NeoMultiplexerPlugin)

NeoMultiplexerPlugin::NeoMultiplexerPlugin(QObject * parent)
:  QSerialIODeviceMultiplexerPlugin(parent)
{
}

NeoMultiplexerPlugin::~NeoMultiplexerPlugin()
{
}

bool NeoMultiplexerPlugin::detect(QSerialIODevice *)
{
    return true;
}

QSerialIODeviceMultiplexer *NeoMultiplexerPlugin::create(QSerialIODevice *
                                                         device)
{
    return new QNullSerialIODeviceMultiplexer(device);
}
