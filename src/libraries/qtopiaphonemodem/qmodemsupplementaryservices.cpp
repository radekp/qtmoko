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

#include <qmodemsupplementaryservices.h>
#include <qmodemservice.h>
#include <qatutils.h>
#include <qatresult.h>

/*!
    \class QModemSupplementaryServices
    \inpublicgroup QtCellModule

    \brief The QModemSupplementaryServices class provides access to structured and unstructured supplementary services for AT-based modems.
    \ingroup telephony::modem

    This class uses the \c{AT+CUSD} and \c{ATD} commands from 3GPP TS 27.007.  This class also
    processes the \c{+CSSI}, \c{+CSSU}, and \c{+CUSD} unsolicited result codes.

    QModemSupplementaryServices implements the QSupplementaryServices telephony interface.
    Client applications should use QSupplementaryServices instead of this class to
    access supplementary services.

    \sa QSupplementaryServices
*/

/*!
    Create an AT-based supplementary service handler for \a service.
*/
QModemSupplementaryServices::QModemSupplementaryServices
        ( QModemService *service )
    : QSupplementaryServices( service->service(), service, QCommInterface::Server )
{

    _cusdData = "";
    this->service = service;
    connect( service, SIGNAL(resetModem()), this, SLOT(resetModem()) );
    service->primaryAtChat()->registerNotificationType
        ( "+CSSI:", this, SLOT(cssi(QString)) );
    service->primaryAtChat()->registerNotificationType
        ( "+CSSU:", this, SLOT(cssu(QString)) );
    service->primaryAtChat()->registerNotificationType
        ( "+CUSD:", this, SLOT(cusd(QString)), true );
}

/*!
    Destroy this AT-based supplementary service handler.
*/
QModemSupplementaryServices::~QModemSupplementaryServices()
{
}

/*!
    \reimp
*/
void QModemSupplementaryServices::cancelUnstructuredSession()
{
    service->primaryAtChat()->chat
        ( "AT+CUSD=2", this, SLOT(cusdDone(bool,QAtResult)) );
}

/*!
    \reimp
*/
void QModemSupplementaryServices::sendUnstructuredData( const QString& data )
 {
    // struppi: ,15" means "GSM encoding, Language unspecified, that's
    // basically the only one that works at all for me
    service->primaryAtChat()->chat
        ( "AT+CUSD=1,\"" + QAtUtils::quote( data ) + "\",15",
          this, SLOT(cusdDone(bool,QAtResult)) );
}

/*!
    \reimp
*/
void QModemSupplementaryServices::sendSupplementaryServiceData
        ( const QString& data )
{
    service->primaryAtChat()->chat
        ( "ATD" + data, this, SLOT(atdDone(bool,QAtResult)) );
}

void QModemSupplementaryServices::resetModem()
{
    // Turn on [unstructured] supplementary service notifications.
    service->primaryAtChat()->chat( "AT+CSSN=1" );
    service->primaryAtChat()->chat( "AT+CUSD=1" );

}

void QModemSupplementaryServices::cusdDone( bool, const QAtResult& result )
{
    emit unstructuredResult( (QTelephony::Result)result.resultCode() );
}

void QModemSupplementaryServices::atdDone( bool, const QAtResult& result )
{
    emit supplementaryServiceResult( (QTelephony::Result)result.resultCode() );
}

void QModemSupplementaryServices::cssi( const QString& msg )
{
    uint posn = 6;
    uint code1 = QAtUtils::parseNumber( msg, posn );
    uint index = QAtUtils::parseNumber( msg, posn );
    emit outgoingNotification( (OutgoingNotification)code1, (int)index );
}

void QModemSupplementaryServices::cssu( const QString& msg )
{
    uint posn = 6;
    uint code2 = QAtUtils::parseNumber( msg, posn );
    uint index = QAtUtils::parseNumber( msg, posn );
    QString number;
    if ( ((int)posn) < msg.length() ) {
        number = QAtUtils::nextString( msg, posn );
        number = QAtUtils::decodeNumber
            ( number, QAtUtils::parseNumber( msg, posn ) );
    }
    emit incomingNotification
        ( (IncomingNotification)code2, (int)index, number );
}

void QModemSupplementaryServices::cusd( const QString& msg )
{
    uint posn = 6;
    uint mflag = QAtUtils::parseNumber( msg, posn );
    QString value = QAtUtils::nextString( msg, posn );

    /* Some Providers send rather lengthy USSD responses
     * that spread across multiple lines in the AT syntax.
     * Check if that is one of them.
     */
    if (posn < msg.length()-1) {
        /* looks like this one comes in a single line. */
        uint dcs = QAtUtils::parseNumber( msg, posn );
        value = QAtUtils::decodeString( value, dcs );
        emit unstructuredNotification( (UnstructuredAction)mflag, value );
    }
    else {
        /* looks like this one comes in multiple lines. get all of them */
        _cusdData = msg;
        _cusdMFlag = mflag;
        service->primaryAtChat()->requestNextLine(this, SLOT(cusdData(QString)));
    }
}


/*
 Some providers do sometimes send lengthy responses to AT+CUSD requests.
 Those responses span across multiple lines. In case one of these
 lengthy messages is detected, this function gets invoked via
 QModemSupplementaryServices::cusd() to collect the remaining lines in
 the response.
*/
void QModemSupplementaryServices::cusdData( const QString& msg )
{
    uint posn = 6;
    _cusdData.append(msg);
    QString data = QAtUtils::nextString( _cusdData, posn );
    if (posn < _cusdData.length()-1) {
        uint dcs = QAtUtils::parseNumber( _cusdData, posn );
        data = QAtUtils::decodeString( data, dcs );
        _cusdData = "";
        emit unstructuredNotification( (UnstructuredAction)_cusdMFlag, data );
    }
    else {
        service->primaryAtChat()->requestNextLine(this, SLOT(cusdData(QString)));
    }
}

