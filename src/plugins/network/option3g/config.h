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

#ifndef CONFIG_H
#define CONFIG_H

#include <qtopianetworkinterface.h>
#include <qtranslatablesettings.h>

#include <QDialog>

#ifndef QTOPIA_NO_FSO
#include <qfsoutil.h>
#include <qfsogsmpdp.h>
#endif

class DialupUI;

class DialupConfig : public QtopiaNetworkConfiguration
{
public:
    DialupConfig( const QString& confFile );
    virtual ~DialupConfig();

    virtual QString configFile() const
    {
        return currentConfig;
    }

    virtual QStringList types() const;
    virtual QDialog* configure( QWidget* parent, const QString& type = QString() );

    virtual QVariant property( const QString& key ) const;
    virtual QtopiaNetworkProperties getProperties() const;
    virtual void writeProperties( const QtopiaNetworkProperties& properties );

private:
    QString currentConfig;
    mutable QTranslatableSettings cfg;
#ifndef QTOPIA_NO_FSO
    QFsoGSMPDP gsmPdp;
#endif
};

#endif
