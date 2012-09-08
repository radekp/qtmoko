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

#ifndef QATCHAT_H
#define QATCHAT_H

#include <qtopiaglobal.h>
#include <qatresult.h>
#include <qbytearray.h>


class QAtChatPrivate;
class QAtChatCommand;
class QSerialIODevice;

class QTOPIACOMM_EXPORT QAtChat : public QObject
{
    Q_OBJECT
    friend class QSerialIODevice;
private:
    explicit QAtChat( QSerialIODevice *device );
    ~QAtChat();

public:
    void chat( const QString& command );
    void chat( const QString& command, QObject *target, const char *slot,
               QAtResult::UserData *data = 0, bool noResponse = false );
    void chatPDU( const QString& command, const QByteArray& pdu,
                  QObject *target, const char *slot,
                  QAtResult::UserData *data = 0 );

    void registerNotificationType
        ( const QString& type, QObject *target,
          const char *slot, bool mayBeCommand = false );

    void abortDial();

    void suspend();
    void resume();

    void setDebugChars( char from, char to, char notify, char unknown );

    int deadTimeout() const;
    void setDeadTimeout( int msec );

    int retryOnNonEcho() const;
    void setRetryOnNonEcho( int msec );

    void setCPINTerminator();

    void requestNextLine( QObject *target, const char *slot );

    void send( const QString& command );

    void registerErrorPrefix( const QString& type );

    void registerWakeupCommand( const QString& cmd, int wakeupTime );

signals:
    void pduNotification( const QString& type, const QByteArray& pdu );
    void callNotification( const QString& type );
    void dead();

private slots:
    void incoming();
    void timeout();
    void failCommand();
    void retryTimeout();
    void performWakeup();
    void wakeupFinished();

private:
    QAtChatPrivate *d;

    void queue( QAtChatCommand *command );
    void done();
    bool writeLine( const QString& line );
    void writePduLine( const QString& line );
    bool processLine( const QString& line );

    void prime();
    void primeIfNecessary();
};

#endif
