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

#include "messagecontrol.h"
#include "qabstractmessagebox.h"
#include <qled.h>
#include <qtopiapowermanager.h>
#ifdef QTOPIA_CELL
#include <qcommservicemanager.h>
#endif
#include <QtopiaServiceRequest>
#include <QString>
#include <QSettings>
#include <QByteArray>
#include <QDataStream>

static const int WarningTimeout = 5000;  

/*!
  \class MessageControl
    \inpublicgroup QtPkgManagementModule
    \inpublicgroup QtTelephonyModule
  \brief The MessageControl class provides message related management services.
  \ingroup QtopiaServer::Telephony

  It keeps track of arriving messages and notifies the \l MessagesService via 
  \l MessagesService::viewNewMessages() if required. In addition this class maintains 
  the following value space entries:
 
  \table
  \header \o Path \o Description
  \row  \o \c{Communications/Messages/NewMessages} \o number of new not yet read messages
  \row  \o \c{Communications/Messages/NewSMS} \o number of unread SMS
  \row  \o \c{Communications/Messages/NewMMS} \o number of unread MMS
  \row  \o \c{Communications/Messages/NewSystem} \o number of unread system messages
  \row  \o \c{Communications/Messages/NewInstant} \o number of unread instant messages
  \row  \o \c{Communications/Messages/NewEmail} \o number of unread email messages
  \row  \o \c{Communications/Messages/SMSFull} \o true if the sms storage is full
  \row  \o \c{Communications/Messages/SMSRejected} \o true if an sms was rejected due to a full storage
  \endtable

  This task opens a message box if a message was rejected due to full SMS memory. 
  If this is not a desired behavior or such a notification
  would be handled by a different class the message box can be disabled by setting the
  \c {MessageControl\DisablePopup} key to \c true in the \c {Trolltech/Phone} configuration file. If this setting
  is missing, MessageControl shows the above mentioned popup box by default.

  This class is a Qt Extended server task. It cannot be used by other Qt Extended applications.
  */

/*!
    \fn void MessageControl::smsMemoryFull(bool)
    \internal
*/

/*!
  \fn void MessageControl::messageRejected()
  \internal
  */

/*!
  Creates a new instance of this class with the given \a parent.
  */
MessageControl::MessageControl( QObject *parent) : QObject( parent ),
    phoneValueSpace("/Communications/Messages"),
#ifdef QTOPIA_CELL
    smsMemFull("/Telephony/Status/SMSMemoryFull"),
    prevSmsMemoryFull(0),
#endif
    messageCountUpdate(0),
    channel("QPE/MessageControl"),
    smsCount(0), mmsCount(0), systemCount(0), instantCount(0), emailCount(0)
{
    QSettings setting("Trolltech", "qpe");
    setting.beginGroup("Messages");
    smsCount = setting.value("MissedSMSMessages", 0).toInt();
    mmsCount = setting.value("MissedMMSMessages", 0).toInt();
    systemCount = setting.value("MissedSystemMessages", 0).toInt();
    instantCount = setting.value("MissedInstantMessages", 0).toInt();
    emailCount = setting.value("MissedEmailMessages", 0).toInt();

#ifdef QTOPIA_CELL
    connect(&smsMemFull, SIGNAL(contentsChanged()),
            this, SLOT(smsMemoryFullChanged()) );
    prevSmsMemoryFull = smsMemFull.value().toInt();
#endif

    connect(&channel, SIGNAL(received(QString,QByteArray)),
            this, SLOT(controlMessage(QString,QByteArray)) );

    // React to changes in incoming message count
    messageCountUpdate = new QtopiaIpcAdaptor("QPE/Messages/MessageCountUpdated");
    QtopiaIpcAdaptor::connect(messageCountUpdate, MESSAGE(changeValue()), 
                              this, SLOT(messageCountChanged()));

    doNewCount(false);
    messageCountChanged();
}

void MessageControl::doNewCount(bool write)
{
    if (write) {
        QSettings setting("Trolltech", "qpe");
        setting.beginGroup( "Messages" );
        setting.setValue( "MissedSMSMessages", smsCount);
        setting.setValue( "MissedMMSMessages", mmsCount);
        setting.setValue( "MissedSystemMessages", systemCount );
        setting.setValue( "MissedInstantMessages", instantCount );
        setting.setValue( "MissedEmailMessages", emailCount );
    }

    int count = messageCount();
    if (count == 0) {
        // We have to do this, since our count reset notification doesn't arrive until after
        // the notification that we should update the message count...
        messageCountChanged();
    }
}

/*! 
  \internal
  Returns the number of unread messages 
 */
int MessageControl::messageCount() const
{
    return smsCount + mmsCount + systemCount + instantCount + emailCount;
}

void MessageControl::updateMessageCount(int &messageCount, QDataStream &stream)
{
    int count;
    stream >> count;
    if (count != messageCount) {
        messageCount = count;
        doNewCount(true);
    }
}

void MessageControl::controlMessage(const QString& message, const QByteArray &data)
{
    QDataStream stream( data );

    if (message == "newSmsCount(int)") {
        updateMessageCount(smsCount, stream);
    } else if (message == "newMmsCount(int)") {
        updateMessageCount(mmsCount, stream);
    } else if (message == "newSystemCount(int)") {
        updateMessageCount(systemCount, stream);
    } else if (message == "newInstantCount(int)") {
        updateMessageCount(instantCount, stream);
    } else if (message == "newEmailCount(int)") {
        updateMessageCount(emailCount, stream);
    }
}

#ifdef QTOPIA_CELL
void MessageControl::smsMemoryFullChanged()
{
    // Check for the "message rejected state", so we can report it.
    int fullState = smsMemFull.value().toInt();
    if ( fullState != prevSmsMemoryFull ) {
        prevSmsMemoryFull = fullState;

        emit smsMemoryFull( fullState != 0 );
        if ( fullState == 2 ) {
            emit messageRejected();
            QSettings cfg( "Trolltech", "Phone" );
            cfg.beginGroup("MessageControl");
            if ( !cfg.value("DisablePopup", false).toBool() ) {
                QAbstractMessageBox *box = QAbstractMessageBox::messageBox(0,
                        tr("Message Rejected"),
                        tr("<qt>An incoming message was rejected because "
                           "there is insufficient space to store it.</qt>"),
                        QAbstractMessageBox::Warning);
                if ( box ) {
                    box->setTimeout(WarningTimeout,QAbstractMessageBox::NoButton);
                    QtopiaApplication::showDialog(box);
                    delete box;
                } else {
                    qLog(Component) << "MessageControl: Missing messagebox component";
                }
            }
        }
    }
    phoneValueSpace.setAttribute("SMSRejected", (fullState==2));
    phoneValueSpace.setAttribute("SMSFull", (fullState>0));
}
#endif

void MessageControl::messageCountChanged()
{
    // The incoming message count has been updated externally
    const int totalCount(messageCount());

    // Update the valueSpace variables
    phoneValueSpace.setAttribute("NewMessages", totalCount);
    phoneValueSpace.setAttribute("NewSMS", smsCount);
    phoneValueSpace.setAttribute("NewMMS", mmsCount);
    phoneValueSpace.setAttribute("NewSystemMessages", systemCount);
    phoneValueSpace.setAttribute("NewInstantMessages", instantCount);
    phoneValueSpace.setAttribute("NewEmailMessages", emailCount);

    if (totalCount) {
        // Turn off screen saver so the new message notification will be visible
        QtopiaPowerManager::setActive(false);
    } else {
        qLedIndicatorNewSmsOff();       // no unread messages - stop blinking with SMS led
    }
}

QTOPIA_TASK(MessageControl,MessageControl);
