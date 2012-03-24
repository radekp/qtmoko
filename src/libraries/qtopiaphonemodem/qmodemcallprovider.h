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

#ifndef QMODEMCALLPROVIDER_H
#define QMODEMCALLPROVIDER_H

#include <qtopiaglobal.h>
#include <qphonecallprovider.h>

class QModemCallProviderPrivate;
class QSerialIODeviceMultiplexer;
class QModemPPPdManager;
class QAtChat;
class QAtResult;
class QModemCall;
class QModemDataCall;
class QModemService;

class QTOPIAPHONEMODEM_EXPORT QModemCallProvider : public QPhoneCallProvider
{
    Q_OBJECT
    friend class QModemCall;
    friend class QModemDataCall;
public:
    explicit QModemCallProvider( QModemService *service );
    ~QModemCallProvider();

    enum AtdBehavior
    {
        AtdOkIsConnect,
        AtdOkIsDialing,
        AtdOkIsDialingWithStatus,
        AtdUnknown,
	AtdOkIgnore
    };

    QModemService *service() const;
    QAtChat *atchat() const;
    QSerialIODeviceMultiplexer *multiplexer() const;
    QModemCall *incomingCall() const;
    QModemCall *dialingCall() const;
    QModemCall *callForIdentifier( uint id ) const;
    uint nextModemIdentifier();

public slots:
    virtual void ringing( const QString& number, const QString& callType,
                          uint modemIdentifier = 0 );
    void hangupRemote( QModemCall *call );

protected:
    QPhoneCallImpl *create
        ( const QString& identifier, const QString& callType );
    virtual QString resolveRingType( const QString& type ) const;
    virtual QString resolveCallMode( int mode ) const;
    virtual bool hasRepeatingRings() const;
    virtual QModemCallProvider::AtdBehavior atdBehavior() const;
    virtual void abortDial( uint modemIdentifier, QPhoneCall::Scope scope );
    virtual bool partOfHoldGroup( const QString& callType ) const;

    virtual QString dialServiceCommand( const QDialOptions& options ) const;
    virtual QString dialVoiceCommand( const QDialOptions& options ) const;
    virtual QString releaseCallCommand( uint modemIdentifier ) const;
    virtual QString releaseActiveCallsCommand() const;
    virtual QString releaseHeldCallsCommand() const;
    virtual QString putOnHoldCommand() const;
    virtual QString setBusyCommand() const;
    virtual QString acceptCallCommand( bool otherActiveCalls ) const;
    virtual QString activateCallCommand
                ( uint modemIdentifier, bool otherActiveCalls ) const;
    virtual QString activateHeldCallsCommand() const;
    virtual QString joinCallsCommand( bool detachSubscriber ) const;
    virtual QString deflectCallCommand( const QString& number ) const;
    virtual QStringList gprsSetupCommands() const;

    bool useDetectTimer() const;
    void setUseDetectTimer(bool);
    bool useMissedTimer() const;
    void setUseMissedTimer(bool);
    void llIndicatorsRinging();

protected slots:
    virtual void resetModem();

private slots:
    void ring();
    void cring( const QString& msg );
    void callNotification( const QString& msg );
    void clip( const QString& msg );
    void colp( const QString& msg );
    void ccwa( const QString& msg );
    void detectTimeout();
    void missedTimeout();
    void clccIncoming( bool, const QAtResult& );

protected:
    void announceCall();

private:
    QModemCallProviderPrivate *d;

    void missedTimeout(QModemCall *call);
    void stopRingTimers();
    void useModemIdentifier( uint id );
    void releaseModemIdentifier( uint id );
    bool otherActiveCalls( QModemCall *notThis=0 );
    void changeGroup( QPhoneCall::State oldState, QPhoneCall::State newState,
                      QPhoneCallImpl::Actions newActions );
    bool hasGroup( QPhoneCall::State state );
    void activateWaitingOrHeld();
    QModemPPPdManager *pppdManager() const;
    int vtsType() const;
    void setVtsType( int type );
};

#endif
