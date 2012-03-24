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

#ifndef OFONONETWORKREGISTRATION_H
#define OFONONETWORKREGISTRATION_H

#include <QTimer>
#include <QSignalSourceProvider>
#include <qofonoutil.h>
#include <qnetworkregistration.h>

class OFonoTelephonyService;

class OFonoNetworkRegistration : public QNetworkRegistrationServer
{
    Q_OBJECT
public:
    OFonoNetworkRegistration(OFonoTelephonyService * service);
    ~OFonoNetworkRegistration();

    OFonoTelephonyService *service;
    QSignalSourceProvider signalProvider;

    void modemPropertyChanged(const QString & name, const QDBusVariant & value);    // called by ofono telephony service when modem property changes
    void netRegPropertyChanged(const QString & name,
                               const QDBusVariant & value);

public slots:
    void setCurrentOperator(QTelephony::OperatorMode mode,
                            const QString & id, const QString & technology);
    void requestAvailableOperators();

private slots:
    void registerFinished(QOFonoDBusPendingCall &);
};

#endif
