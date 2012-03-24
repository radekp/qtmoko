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

#ifndef NEOMUTLIPLEXER_H
#define NEOMUTLIPLEXER_H

#include <qserialiodevicemultiplexerplugin.h>
#include <qserialiodevicemultiplexer.h>

class NeoMultiplexerPlugin : public QSerialIODeviceMultiplexerPlugin
{
    Q_OBJECT
public:
    NeoMultiplexerPlugin(QObject * parent = 0);
    virtual ~ NeoMultiplexerPlugin();

    bool detect(QSerialIODevice * device);
    QSerialIODeviceMultiplexer *create(QSerialIODevice * device);
};

#endif
