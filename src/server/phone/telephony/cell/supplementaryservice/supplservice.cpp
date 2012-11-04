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

#include "supplservice.h"

#include "qabstractmessagebox.h"
#include "qtopiaserverapplication.h"

#include <QCommServiceManager>

/*!
  \class SupplementaryServiceTask
    \inpublicgroup QtCellModule
  \brief The SupplementaryServiceTask class provides notification about unstructured notifications 
  within GSM networks.
  \ingroup QtopiaServer::Telephony

  This task will show a message box if the user receives an unstructured notification. 
  For more details about such notifications refer to QSupplementaryServices.

  \sa QSupplementaryServices, QSupplementaryServices::unstructuredNotification()
  */

/*!
  Creates a new SupplementaryServiceTask instance with the given \a parent.
  */
SupplementaryServiceTask::SupplementaryServiceTask( QObject *parent )
    : QObject(parent), serviceManager(0)
{
    serviceManager = new QCommServiceManager( this );
    connect( serviceManager, SIGNAL(servicesChanged()),
             this, SLOT(servicesChanged()) );
    servicesChanged();
}

void SupplementaryServiceTask::servicesChanged()
{
    if ( !serviceManager ) //just to be sure
        return;

    if ( serviceManager->supports<QSupplementaryServices>().contains("modem") ) {
        suppService = new QSupplementaryServices( "modem", this );
        connect( suppService, SIGNAL(unstructuredNotification(QSupplementaryServices::UnstructuredAction,QString)),
                 this, SLOT(unstructuredNotification(QSupplementaryServices::UnstructuredAction,QString)) );
        serviceManager->disconnect();
        serviceManager->deleteLater();
        serviceManager = 0;
    }
}

void SupplementaryServiceTask::unstructuredNotification
        ( QSupplementaryServices::UnstructuredAction action, const QString& data)
{
    QString text;
    if ( !data.isEmpty() ) {
        text = data;
    } else {
        switch ( action ) {
            case QSupplementaryServices::TerminatedByNetwork:
                text = tr("Operation terminated by network");
                break;

            case QSupplementaryServices::OtherLocalClientResponded:
                text = tr("Other local client has responded");
                break;

            case QSupplementaryServices::OperationNotSupported:
                text = tr("Operation is not supported");
                break;

            case QSupplementaryServices::NetworkTimeout:
                text = tr("Operation timed out");
                break;

            default:
                text = tr("Network response: %1").arg((int)action);
                break;
        }
    }

    QString title = tr("Service request");
    QString displayText = "<qt>" + text + "</qt>";
    static QAbstractMessageBox *serviceMsgBox = 0;
    if (serviceMsgBox) {
        delete serviceMsgBox;
    }
    serviceMsgBox = QAbstractMessageBox::messageBox(0, title, displayText,
                                       QAbstractMessageBox::Information);
    QtopiaApplication::showDialog(serviceMsgBox);
}

QTOPIA_TASK(SupplementaryServiceTask,SupplementaryServiceTask);
