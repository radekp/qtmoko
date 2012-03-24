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

#ifndef OFONOMODEMSERVICE_H
#define OFONOMODEMSERVICE_H

#include <qvaluespace.h>
#include <qmodemservice.h>
#include <qmodemcall.h>
#include <qmodemcallprovider.h>
#include <qmodemsimtoolkit.h>
#include <qbandselection.h>

#include <qofonoutil.h>
#include <qofonoobject.h>
#include <qofonomodem.h>
#include <qofonocellbroadcast.h>
#include <qofonodbuspendingreply.h>
#include <qofonoobjectlist.h>
#include <qofonosimmanager.h>
#include <qofonovoicecallmanager.h>
#include <qofonoconnectionmanager.h>
#include <qofonophonebook.h>
#include <qofonostringmap.h>
#include <qofonodbusabstractinterface.h>
#include <qofonomessagemanager.h>
#include <qofonopushnotification.h>
#include <qofonosupplementaryservices.h>
#include <qofonodbuspendingcall.h>
#include <qofononetworkregistration.h>
#include <qofonoradiosettings.h>
#include <qofonovoicecall.h>
#include <qofononetworkoperator.h>

#include "ofonophonecall.h"
#include "ofonocallprovider.h"
#include "ofononetworkregistration.h"
#include "ofonosupplementaryservices.h"
#include "ofonoservicechecker.h"
#include "ofonorffunctionality.h"
#include "ofonosmssender.h"
#include "ofonosmsreader.h"
#include "ofonophonebook.h"
#include "ofonosiminfo.h"

class OFonoCallProvider;

class OFonoTelephonyService : public QTelephonyService
{
    Q_OBJECT
public:
    OFonoTelephonyService(const QString & service, QString modemDbusPath,
                          QObject * parent = 0);
    ~OFonoTelephonyService();

    OrgOfonoModemInterface oModem;
    OrgOfonoCellBroadcastInterface oCellBroadcast;
    OrgOfonoConnectionManagerInterface oConnMan;
    OrgOfonoMessageManagerInterface oMessageManager;
    OrgOfonoNetworkRegistrationInterface oNetReg;
    OrgOfonoPhonebookInterface oPhoneBook;
    OrgOfonoPushNotificationInterface oPushNotify;
    OrgOfonoRadioSettingsInterface oRadio;
    OrgOfonoSimManagerInterface oSim;
    OrgOfonoSupplementaryServicesInterface oSuplServices;
    OrgOfonoVoiceCallManagerInterface oVoiceCallManager;

    OFonoServiceChecker service_checker;
    OFonoRfFunctionality rf_functionality;
    OFonoNetworkRegistration network_registration;
    OFonoSupplementaryServices suppl_services;
    OFonoCallProvider call_provider;
    OFonoSMSSender sms_sender;
    OFonoSMSReader sms_reader;
    OFonoPhoneBook phone_book;
    OFonoSimInfo sim_info;

    QVariantMap modemProperties;
    QVariantMap netRegProperties;
    QVariantMap voiceCallManagerProperties;

    QValueSpaceItem phoneVs;

    void initialize();
    bool interfaceAvailable(QOFonoDbusAbstractInterface * interface);

private slots:
    void poweredFinished(QOFonoDBusPendingCall &);
    void modemPropertyChanged(const QString & name, const QDBusVariant & value);
    void netRegPropertyChanged(const QString & name,
                               const QDBusVariant & value);
    void voiceCallManagerPropertyChanged(const QString & name,
                                         const QDBusVariant & value);
    void voiceCallAdded(const QDBusObjectPath & path,
                        const QVariantMap & properties);
    void voiceCallRemoved(const QDBusObjectPath & path);
    void phoneVsChanged();
};

#endif
