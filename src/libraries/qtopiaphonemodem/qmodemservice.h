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

#ifndef QMODEMSERVICE_H
#define QMODEMSERVICE_H

#include <qtelephonyservice.h>
#include <qserialiodevice.h>
#include <qserialiodevicemultiplexer.h>
#include <qatchat.h>
#include <qatresult.h>
#include <qvaluespace.h>

class QModemServicePrivate;
class QModemCallProvider;
class QModemIndicators;

class QTOPIAPHONEMODEM_EXPORT QModemService : public QTelephonyService
{
    Q_OBJECT
    friend class QModemSimToolkit;
public:
    explicit QModemService( const QString& service,
                   const QString& device = QString(),
                   QObject *parent = 0 );
    QModemService( const QString& service, QSerialIODeviceMultiplexer *mux,
                   QObject *parent = 0 );
    ~QModemService();

    virtual void initialize();

    QSerialIODeviceMultiplexer *multiplexer() const;
    QAtChat *primaryAtChat() const;
    QAtChat *secondaryAtChat() const;

    void chat( const QString& command );
    void chat( const QString& command, QObject *target, const char *slot,
               QAtResult::UserData *data = 0 );

    void retryChat( const QString& command );

    static QModemService *createVendorSpecific
                ( const QString& service = "modem",
                  const QString& device = QString(),
                  QObject *parent = 0 );

    void post( const QString& item );
    void connectToPost
        ( const QString& item, QObject *target, const char *slot );

    QModemIndicators *indicators() const;
    virtual bool supportsAtCced();                      // does modem support AT+CCED command?

protected slots:
    virtual void needSms();
    virtual void suspend();
    virtual void wake();
    void suspendDone();
    void wakeDone();

signals:
    void posted( const QString& item );
    void resetModem();

private:
    void init( QSerialIODeviceMultiplexer *mux );

private slots:
    void cmgfDone( bool ok );
    void sendNeedSms();
    void postItems();
    void firstTimeInit();
    void phoneBookPreload();
    void stkInitDone();
    void phoneVsChanged();

private:
    QModemServicePrivate *d;
    QValueSpaceItem phoneVs;
};

#endif
