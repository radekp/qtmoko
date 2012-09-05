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

#ifndef CELLMODEMMANAGER_H
#define CELLMODEMMANAGER_H

#include <qvaluespace.h>
#include <qcbsmessage.h>
#include <qnetworkregistration.h>
#include <qcallforwarding.h>
#include <qsupplementaryservices.h>
#include <QPinManager>
#include <QPhoneBook>
#include "qtopiaserverapplication.h"
#include "qabstractcallpolicymanager.h"

class QPinManager;

class QtopiaServiceDescription;
class QLabel;

class CellModemManagerPrivate;
class CellModemManager : public QAbstractCallPolicyManager
{
Q_OBJECT
public:
    CellModemManager(QObject *parent = 0);
    virtual ~CellModemManager();

    static void disableProfilesControlModem();
    static bool profilesControlModem();
    bool profilesBlocked();

    enum State { NoCellModem, Initializing, Initializing2,
                 Ready, WaitingSIMPin, VerifyingSIMPin,
                 WaitingSIMPuk, VerifyingSIMPuk,
                 SIMDead, SIMMissing, AerialOff,
                 FailureReset, UnrecoverableFailure };

    State state() const;

    QString callType() const;
    QString trCallType() const;
    QString callTypeIcon() const;
    QAbstractCallPolicyManager::CallHandling handling(const QString& number);
    bool isAvailable(const QString& number);
    void updateOnThePhonePresence( bool isOnThePhone );
    bool doDnd();
    bool supportsPresence() const;
    QString registrationMessage() const;
    QString registrationIcon() const;

    QString networkOperator() const;
    QString networkOperatorCountry() const;
    QTelephony::RegistrationState registrationState() const;
    bool callForwardingEnabled() const;
    bool simToolkitAvailable() const;
    bool cellModemAvailable() const;
    QString simToolkitIdleModeText() const;
    QIcon simToolkitIdleModeIcon() const;
    bool simToolkitIdleModeIconSelfExplanatory() const;
    QString simToolkitMenuTitle() const;

    bool planeModeEnabled() const;
    bool planeModeSupported() const;

    bool networkRegistered() const;

    static QString stateToString(State state);
    static QStringList emergencyNumbers();

    void setCellLocation( const QString &location );

public slots:
    void setPlaneModeEnabled(bool);
    void setSimPin(const QString &pin);
    void setSimPuk(const QString &puk, const QString &newPin);
    void blockProfiles(bool);

signals:
    void planeModeEnabledChanged(bool);
    void registrationStateChanged(QTelephony::RegistrationState);
    void networkOperatorChanged(const QString &);
    void stateChanged(CellModemManager::State newState,
                      CellModemManager::State oldState);
    void callForwardingEnabledChanged(bool);
    void simToolkitAvailableChanged(bool);

private slots:
    void rfLevelChanged();
    void pinStatus(const QString& type, QPinManager::Status status,
                   const QPinOptions&);
    void currentOperatorChanged();
    void registrationStateChanged();
    void locationChanged();
    void autoRegisterTimeout();
    void planeModeChanged(bool);
    void queryCallForwarding();
    void forwardingStatus(QCallForwarding::Reason reason,
                          const QList<QCallForwarding::Status>& status);
    void setCallerIdRestriction();
    void simInserted();
    void simRemoved();
    void simToolkitAvailableChange();
    void simNotInserted();
    void readPhoneBooks();
    void fetchEmergencyNumbers();
    void emergencyNumbersFetched
        ( const QString& store, const QList<QPhoneBookEntry>& list );

private:
    void doAutoRegister();
    void tryDoReady();
    void tryDoAerialOff();
    void newlyRegistered();
    void setNotReadyStatus();
    void setReadyStatus();
    void updateStatus();
    void setAerialEnabled(bool);
    void doStateChanged(State newState);
    void doInitialize();
    void doInitialize2();
    void startAutoRegisterTimer();
    void stopAutoRegisterTimer();
    CellModemManagerPrivate *d;
};

#endif
