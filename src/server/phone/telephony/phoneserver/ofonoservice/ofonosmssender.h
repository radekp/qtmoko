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

#ifndef OFONOSMSSENDER_H
#define OFONOSMSSENDER_H

#include <QtDBus>
#include <qsmssender.h>
#include "ofonoutil.h"

class OFonoTelephonyService;

class OFonoSMSSender : public QSMSSender
{
    Q_OBJECT
public:
    OFonoSMSSender(OFonoTelephonyService * service);
    ~OFonoSMSSender();

    OFonoTelephonyService *service;
    QString smsId;              // we can have just one pending call if we use watchOFonoCall() so this works

public slots:
    void send(const QString & id, const QSMSMessage & msg);

private slots:
    void sendMessageFinished(QOFonoDBusPendingCall &);
};

#endif
