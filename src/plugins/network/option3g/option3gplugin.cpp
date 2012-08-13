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

#include "option3gplugin.h"

#include <QPointer>
#include <qtopialog.h>
#include <qtopiaapplication.h>

Option3gPlugin::Option3gPlugin()
{
    //Load translation for libqtopianetwork
    QtopiaApplication::loadTranslations( "libqtopianetwork" );
}

Option3gPlugin::~Option3gPlugin()
{
    qLog(Network) << "Deleting Option3gPlugin (" << instances.count()
        << " instances)";
    //cleanup all interface instances
    while ( !instances.isEmpty() )
    {
        QPointer<QtopiaNetworkInterface> impl = instances.takeFirst();
        if (impl)
            delete impl;
    }
}

QPointer<QtopiaNetworkInterface> Option3gPlugin::network( const QString& confFile)
{
    qLog(Network) << "new Option3gPlugin interface instance requested -> " << confFile;
    QPointer<QtopiaNetworkInterface> hso = new HsoInterface( confFile );
    instances.append(hso);

    return hso;
}

QtopiaNetwork::Type Option3gPlugin::type() const
{
        return ( QtopiaNetwork::Dialup |
             QtopiaNetwork::GPRS |
             QtopiaNetwork::PhoneModem );
}

QByteArray Option3gPlugin::customID() const
{
    return QByteArray();
}
QTOPIA_EXPORT_PLUGIN( Option3gPlugin );
