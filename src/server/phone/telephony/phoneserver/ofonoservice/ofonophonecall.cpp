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

#include "llindicators.h"
#include "ofonophonecall.h"
#include "ofonotelephonyservice.h"
#include "ofonocallprovider.h"

OFonoPhoneCall::OFonoPhoneCall
    (OFonoTelephonyService * service, const QString & identifier,
     const QString & callType, const QString & path)
:QPhoneCallImpl(&service->call_provider, identifier, callType)
, service(service)
, oVoiceCall("org.ofono", path, QDBusConnection::systemBus(), this)
, voiceCallProperties()
{
    registerPropertyChanged();
}

OFonoPhoneCall::~OFonoPhoneCall()
{
}

static QPhoneCall::State ofonoStateToQt(QString ofonoStatus, bool hangupLocal)
{
    if (ofonoStatus == "active")    // The call is active
        return QPhoneCall::Connected;
    if (ofonoStatus == "held")  // The call is on hold
        return QPhoneCall::Hold;
    if (ofonoStatus == "dialing")   // The call is being dialed
        return QPhoneCall::Dialing;
    if (ofonoStatus == "alerting")  // The remote party is being alerted
        return QPhoneCall::Alerting;
    if (ofonoStatus == "incoming")  // Incoming call in progress
        return QPhoneCall::Incoming;
    if (ofonoStatus == "waiting")   // Call is waiting
        return QPhoneCall::Hold;
    if (ofonoStatus == "disconnected")  // No further use of this object
        return hangupLocal ? QPhoneCall::HangupLocal : QPhoneCall::HangupRemote;

    qWarning() << "ofonoStateToQt: unknown status " << ofonoStatus;
    return QPhoneCall::OtherFailure;
}

void OFonoPhoneCall::registerPropertyChanged()
{
    connect(&oVoiceCall,
            SIGNAL(PropertyChanged(const QString, const QDBusVariant)), this,
            SLOT(voiceCallPropertyChanged(const QString, const QDBusVariant)));
}

void OFonoPhoneCall::setOFonoState(QString ofonoStatus)
{
    setState(ofonoStateToQt(ofonoStatus, false));
}

void OFonoPhoneCall::dial(const QDialOptions & options)
{
    QString number = options.number();
    setNumber(number);

    qDebug() << "OFonoPhoneCall::dial(" << number << ")";

    // If the number starts with '*' or '#', then this is a request
    // for a supplementary service, not an actual phone call.
    // So we dial and then immediately hang up, allowing the network
    // to send us the SS/USSD response when it is ready.
    if (number.startsWith("*") || number.startsWith("#")) {
        service->oSuplServices.Initiate(number);
        setState(QPhoneCall::ServiceHangup);
        return;
    }

    QOFonoDBusPendingCall call = service->oVoiceCallManager.Dial(number, "");
    watchOFonoCall(call, this, SLOT(dialFinished(QOFonoDBusPendingCall &)));
    setState(QPhoneCall::Dialing);
}

void OFonoPhoneCall::dialFinished(QOFonoDBusPendingCall & call)
{
    QOFonoDBusPendingReply < QDBusObjectPath > reply = call;
    if (checkReply(reply)) {
        // Destruct old dbus interface and create new in the same place
        oVoiceCall. ~ OrgOfonoVoiceCallInterface();
        new(&oVoiceCall) OrgOfonoVoiceCallInterface("org.ofono",
                                                    reply.value().path(),
                                                    QDBusConnection::systemBus
                                                    (), this);
        voiceCallProperties.clear();
        registerPropertyChanged();
    }
}

void OFonoPhoneCall::hangup(QPhoneCall::Scope scope)
{
    qDebug() << "OFonoPhoneCall::hangup()";
    llIndicatorsVoiceCallHangup();

    if (scope == QPhoneCall::CallOnly) {
        QOFonoDBusPendingReply <> reply = oVoiceCall.Hangup();
        checkReply(reply);
    } else {
        QOFonoDBusPendingReply <> reply =
            service->oVoiceCallManager.HangupAll();
        checkReply(reply);
    }
    setState(QPhoneCall::HangupLocal);
}

void OFonoPhoneCall::accept()
{
    qDebug() << "OFonoPhoneCall::accept()";
    llIndicatorsVoiceCallAccept();

    QOFonoDBusPendingReply <> reply = oVoiceCall.Answer();
    checkReply(reply);
}

void OFonoPhoneCall::hold()
{
    qDebug() << "OFonoPhoneCall::hold()";
    QOFonoDBusPendingReply <> reply =
        service->oVoiceCallManager.HoldAndAnswer();
    checkReply(reply);
}

void OFonoPhoneCall::activate(QPhoneCall::Scope)
{
    qDebug() << "OFonoPhoneCall::activate()";
    QOFonoDBusPendingReply <> reply =
        service->oVoiceCallManager.ReleaseAndAnswer();
    checkReply(reply);
}

void OFonoPhoneCall::tone(const QString & tones)
{
    qDebug() << "OFonoPhoneCall::tone(" << tones << ")";
    QOFonoDBusPendingReply <> reply =
        service->oVoiceCallManager.SendTones(tones);
    checkReply(reply);
}

void OFonoPhoneCall::transfer(const QString & number)
{
    qDebug() << "OFonoPhoneCall::transfer(" << number << ")";
    QOFonoDBusPendingReply <> reply = service->oVoiceCallManager.Transfer();
    checkReply(reply);
}

void OFonoPhoneCall::voiceCallPropertyChanged(const QString & name,
                                              const QDBusVariant & value)
{
    qDebug() << "voiceCallPropertyChanged " << name << "=" << value.variant();
    if (name == "State") {
        setOFonoState(value.variant().toString());
    }
}
