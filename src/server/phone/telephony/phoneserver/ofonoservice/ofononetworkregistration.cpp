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

#include "ofononetworkregistration.h"
#include "ofonotelephonyservice.h"
#include "ofonoutil.h"

OFonoNetworkRegistration::OFonoNetworkRegistration(OFonoTelephonyService *
                                                   service)
:  QNetworkRegistrationServer(service->service(), service)
    , service(service)
    , signalProvider(service->service(), "modem", this)
{
}

OFonoNetworkRegistration::~OFonoNetworkRegistration()
{
}

static QTelephony::RegistrationState ofonoStatusToQt(QString status)
{
    if (status == "registered") // Registered to home network
        return QTelephony::RegistrationHome;
    if (status == "unregistered")   // Not registered to any network
        return QTelephony::RegistrationNone;
    if (status == "searching")  // Not registered, but searching
        return QTelephony::RegistrationSearching;
    if (status == "denied")     // Registration has been denied
        return QTelephony::RegistrationDenied;
    if (status == "unknown")    // Status is unknown
        return QTelephony::RegistrationUnknown;
    if (status == "roaming")    // Registered, but roaming
        return QTelephony::RegistrationRoaming;

    qWarning() << "unknown oFono registration status" << status;
    return QTelephony::RegistrationNone;
}

static QTelephony::OperatorMode ofonoOpModeToQt(QString mode)
{
    if (mode == "auto")         // Network registration is performed automatically.
        return QTelephony::OperatorModeAutomatic;
    if (mode == "auto-only")    // Network registration is performed automatically, and manual selection is disabled.
        return QTelephony::OperatorModeAutomatic;
    if (mode == "manual")       // Network operator is selected manually. If the operator is currently not selected, registration is not attempted.
        return QTelephony::OperatorModeManual;

    qWarning() << "unknown oFono operator mode " << mode;
    return QTelephony::OperatorModeAutomatic;
}

static QTelephony::OperatorAvailability ofonoOpStatusToQt(QString status)
{
    if (status == "available")
        return QTelephony::OperatorAvailable;
    if (status == "current")
        return QTelephony::OperatorCurrent;
    if (status == "forbidden")
        return QTelephony::OperatorForbidden;

    qWarning() << "unknown oFono operator status " << status;
    return QTelephony::OperatorUnavailable;
}

void OFonoNetworkRegistration::modemPropertyChanged(const QString & name,
                                                    const QDBusVariant &)
{
    if (name != "Interfaces" || !service->interfaceAvailable(&service->oNetReg)) {
        return;
    }

    QOFonoDBusPendingReply < QVariantMap > reply =
        service->oNetReg.GetProperties();
    if (!checkReply(reply)) {
        return;
    }
    QVariantMap properties = service->netRegProperties = reply.value();

    // Notfify about initial properties
    netRegPropertyChanged("Name", QDBusVariant(properties.value("Name")));
    netRegPropertyChanged("Status", QDBusVariant(properties.value("Status")));

    // Register to network
    QOFonoDBusPendingCall call = service->oNetReg.Register();
    watchOFonoCall(call, this, SLOT(registerFinished(QOFonoDBusPendingCall &)));
}

void OFonoNetworkRegistration::registerFinished(QOFonoDBusPendingCall & call)
{
    QOFonoDBusPendingReply <> reply = call;
    checkReply(reply);
}

void OFonoNetworkRegistration::netRegPropertyChanged(const QString & name,
                                                     const QDBusVariant & value)
{
    if (name == "Strength") {
        signalProvider.setAvailability(QSignalSource::Available);
        signalProvider.setSignalStrength(value.variant().toInt());
    } else if (name == "Status") {
        updateRegistrationState(ofonoStatusToQt(value.variant().toString()));
        updateInitialized(true);
    } else if (name == "Name" || name == "Mode" || name == "Technology") {
        QVariantMap properties = service->netRegProperties;
        QString mode = properties.value("Mode").toString();
        QString provider = properties.value("Name").toString();;
        QString technology = properties.value("Technology").toString();;
        updateCurrentOperator(ofonoOpModeToQt(mode), provider, provider,
                              technology);
    }
}

void OFonoNetworkRegistration::setCurrentOperator
    (QTelephony::OperatorMode, const QString & id, const QString &)
{
    OrgOfonoNetworkOperatorInterface op("org.ofono", id,
                                        QDBusConnection::systemBus(), this);
    QOFonoDBusPendingReply <> reply = op.Register();
    emit setCurrentOperatorResult(qTelResult(reply));
}

void OFonoNetworkRegistration::requestAvailableOperators()
{
    QOFonoDBusPendingReply < QOFonoObjectList > reply =
        service->oNetReg.GetOperators();
    if (!checkReply(reply, false, true)) {
        return;
    }

    QOFonoObjectList list = reply.value();
    QList < QNetworkRegistration::AvailableOperator > opers;
    for (int i = 0; i < list.count(); i++) {
        QOFonoObject provider = list.at(i);
        QVariantMap properties = provider.properties;
        QNetworkRegistration::AvailableOperator oper;

        QStringList techList = properties["Technologies"].toStringList();
        QString technology;
        for (int i = 0; i < techList.count(); i++) {
            if (i > 0) {
                technology.append(",");
            }
            technology += techList.at(i);
        }

        oper.availability = ofonoOpStatusToQt(properties["Status"].toString());
        oper.id = provider.object.path();
        oper.name = properties["Name"].toString();
        oper.technology = technology;

        opers.append(oper);
    }
    emit availableOperators(opers);
}
