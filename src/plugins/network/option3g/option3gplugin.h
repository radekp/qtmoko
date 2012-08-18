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

#ifndef DIALUPPLUGIN_H
#define DIALUPPLUGIN_H

#include <qtopianetworkinterface.h>
#include <qtopiaglobal.h>

#include <QByteArray>
#include <QList>
#include <QPointer>

#include "hsointerface.h"

class QTOPIA_PLUGIN_EXPORT Option3gPlugin : public QtopiaNetworkPlugin
{
public:
    Option3gPlugin();
    virtual ~Option3gPlugin();

    virtual QPointer<QtopiaNetworkInterface> network( const QString& confFile);
    virtual QtopiaNetwork::Type type() const;
    virtual QByteArray customID() const;
private:
    QList<QPointer<QtopiaNetworkInterface> > instances;
};

#endif
