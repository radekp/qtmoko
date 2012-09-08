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

#ifndef QATCHAT_P_H
#define QATCHAT_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt Extended API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <qatresult.h>
#include <qobject.h>
#include <qstring.h>
#include <qbytearray.h>

class QAtChatCommandPrivate;

class QAtChatCommand : public QObject
{
    Q_OBJECT
    friend class QAtChat;
    friend class QAtChatPrivate;
public:
    QAtChatCommand( const QString& command, QAtResult::UserData *data,
		    bool noResponse = false );
    QAtChatCommand( const QString& command, const QByteArray& pdu,
                    QAtResult::UserData *data );
    ~QAtChatCommand();

signals:
    void done( bool ok, const QAtResult& result );

private:
    QAtChatCommandPrivate *d;

    void emitDone();
};

class QAtChatLineRequest : public QObject
{
    Q_OBJECT
public:
    QAtChatLineRequest( QObject *target, const char *slot )
    {
        connect( this, SIGNAL(line(QString)), target, slot );
    }
    ~QAtChatLineRequest() {}

    void sendLine( const QString& value ) { emit line( value ); }

signals:
    void line( const QString& value );
};

#endif
