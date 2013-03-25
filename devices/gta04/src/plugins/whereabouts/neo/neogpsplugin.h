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

#ifndef NEOGPSPLUGIN_H
#define NEOGPSPLUGIN_H

#include <QWhereaboutsPlugin>

class QWhereabouts;

class QTOPIA_PLUGIN_EXPORT NeoGpsPlugin : public QWhereaboutsPlugin
{
    Q_OBJECT
public:
    explicit NeoGpsPlugin(QObject * parent = 0);
    ~NeoGpsPlugin();

    virtual QWhereabouts *create(const QString & source);
};

#endif
