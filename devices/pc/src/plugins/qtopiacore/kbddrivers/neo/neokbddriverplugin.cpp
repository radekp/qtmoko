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

#include "neokbddriverplugin.h"
#include "neokbdhandler.h"

#include <qtopiaglobal.h>

NeoKbdDriverPlugin::NeoKbdDriverPlugin( QObject *parent )
    : QKbdDriverPlugin( parent )
{}

NeoKbdDriverPlugin::~NeoKbdDriverPlugin()
{}

QWSKeyboardHandler* NeoKbdDriverPlugin::create(const QString& driver, const QString&)
{
    qWarning("NeoKbdDriverPlugin:create()");
    return create( driver );
}

QWSKeyboardHandler* NeoKbdDriverPlugin::create( const QString& driver)
{
    if( driver.toLower() == "neokbdhandler" ) {
        qWarning("Before call NeoKbdHandler()");
        return new NeoKbdHandler();
    }
    return 0;
}

QStringList NeoKbdDriverPlugin::keys() const
{
    return QStringList() << "neokbdhandler";
}

QTOPIA_EXPORT_QT_PLUGIN(NeoKbdDriverPlugin)
