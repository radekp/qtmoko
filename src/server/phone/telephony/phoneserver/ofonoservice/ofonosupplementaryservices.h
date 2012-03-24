/****************************************************************************
**
** This file is part of the Qt Extended Opensource Package.
**
** Copyright (C) 2011 Radek Polak.
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

#ifndef OFONOSUPPLEMENTARYSERVICES_H
#define OFONOSUPPLEMENTARYSERVICES_H

#include <QTimer>
#include <QtDBus>
#include <qsupplementaryservices.h>
#include "ofonoutil.h"

class OFonoTelephonyService;

class OFonoSupplementaryServices : public QSupplementaryServices
{
    Q_OBJECT
public:
    OFonoSupplementaryServices( OFonoTelephonyService * service );
    ~OFonoSupplementaryServices();

    OFonoTelephonyService * service;
    
    void onIncomingUssd(const QString &mode, const QString &message);

public slots:
    void cancelUnstructuredSession();
    void sendUnstructuredData( const QString& data );
    void sendSupplementaryServiceData( const QString& data );

private slots:
    void sendUssdRequestFinished(QOFonoDBusPendingCall &);
};

#endif

