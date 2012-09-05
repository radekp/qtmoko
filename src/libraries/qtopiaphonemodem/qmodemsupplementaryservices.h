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

#ifndef QMODEMSUPPLEMENTARYSERVICES_H
#define QMODEMSUPPLEMENTARYSERVICES_H

#include <qsupplementaryservices.h>

class QModemService;
class QAtResult;

class QTOPIAPHONEMODEM_EXPORT QModemSupplementaryServices
            : public QSupplementaryServices
{
    Q_OBJECT
public:
    explicit QModemSupplementaryServices( QModemService *service );
    ~QModemSupplementaryServices();

public slots:
    void cancelUnstructuredSession();
    void sendUnstructuredData( const QString& data );
    void sendSupplementaryServiceData( const QString& data );

private slots:
    void resetModem();
    void cusdDone( bool ok, const QAtResult& result );
    void atdDone( bool ok, const QAtResult& result );
    void cssi( const QString& msg );
    void cssu( const QString& msg );
    void cusd( const QString& msg );
    void cusdData(const QString& msg);

private:
    QModemService *service;
    QString _cusdData;
    uint _cusdMFlag;
};

#endif
