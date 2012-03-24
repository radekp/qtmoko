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

#include "ofonosmsreader.h"
#include "ofonotelephonyservice.h"

OFonoSMSReader::OFonoSMSReader(OFonoTelephonyService * service)
:  QSMSReader(service->service(), service, QCommInterface::Server)
    , service(service)
    , index(-1)
    , numSlots(-1)
{
    //QTimer::singleShot(40000, this, SLOT(test()));
}

OFonoSMSReader::~OFonoSMSReader()
{
}

void OFonoSMSReader::test()
{
    check();
    //qDebug() << "OFonoSMSReader::test()";
    //setReady(true);
    //emit messageCount(10);
    //setUnreadCount(10);
}

void OFonoSMSReader::deviceStatus(QString status)
{
    bool oldReady = ready();
    bool newReady = (status == "alive-registered" || status == "alive-sim-ready");

    qDebug() << "OFonoSMSReader::deviceStatus oldReady=" << oldReady <<
        ", newReady=" << newReady;

    if (oldReady == newReady) {
        return;
    }
    setReady(newReady);
    if (newReady) {
        check();
    }
}

void OFonoSMSReader::check()
{
    qDebug() << "OFonoSMSReader::check()";

    numSlots = service->sim_info.info.value("slots").toInt();
    if (numSlots <= 0) {
        QTimer::singleShot(30000, this, SLOT(test()));  // sim info not available, try again later
        return;
    }
    // Fill messages with values from SIM
/*    messages.clear();
    index = 0;

    QOFonoDBusPendingCall call = service->gsmSim.RetrieveMessage(0);
    watchOFonoCall(call, this,
                 SLOT(retrieveMessageFinished(QOFonoDBusPendingCall &)));*/
}

// Remove SMS from incoming if it is on SIM
/*    
static void removeIncomingOnSim(QOFonoSIMMessageList & incoming,
                                QOFonoSIMMessageList & messages)
{
    // Match messages by timestamp
    for (int i = 0; i < incoming.count();) {
        QOFonoSIMMessage ii = incoming.at(i);
        int simLen = 0;
        for (int j = 0; j < messages.count(); j++) {
            QOFonoSIMMessage mj = messages[j];
            if (ii.timestamp == mj.timestamp) {
                simLen += mj.contents.count();
            }
            qDebug() << "removeFromIncoming mj.timestamp=" << mj.timestamp <<
                ", ii.timestamp=" << ii.timestamp << ", simLen=" << simLen <<
                ", ii.contents.count()=" << ii.contents.count();
        }
        qDebug() << "simLen=" << simLen << "ii.contents.count()=" <<
            ii.contents.count();
        // Delete from incoming if the whole SMS is on SIM (dont delete if there
        // is e.g. just first part on SIM)
        if (simLen >= ii.contents.count()) {
            incoming.removeAt(i);
        } else {
            i++;
        }
    }
}*/

void OFonoSMSReader::retrieveMessageFinished(QOFonoDBusPendingCall & call)
{
    /*
    QOFonoDBusPendingReply < QString, QString, QString, QVariantMap > reply =
        call;
    if (checkReply(reply)) {
        QOFonoSIMMessage f;
        f.index = index;
        f.status = reply.argumentAt(0).toString();
        f.number = reply.argumentAt(1).toString();
        f.contents = reply.argumentAt(2).toString();
        f.properties = qdbus_cast < QVariantMap > (reply.argumentAt(3));
        f.timestamp = f.properties.value("timestamp").toString();
        messages.append(f);
    }
    index++;
    if (index >= numSlots) {
        setValue("totalMessages", numSlots);
        setValue("usedMessages", messages.count());
        removeIncomingOnSim(incoming, messages);    // remove incoming messages that are on SIM
        qDebug() << "emit messageCount=" << messages.count() + incoming.count();
        emit messageCount(messages.count() + incoming.count());
        return;
    }
    QOFonoDBusPendingCall nextCall = service->gsmSim.RetrieveMessage(index);
    watchOFonoCall(nextCall, this,
                 SLOT(retrieveMessageFinished(QOFonoDBusPendingCall &)));
                 */
}

// Parse timestamp string e.g. "11/09/12,11:12:31+08"
static QDateTime parseTimestamp(const QString & str)
{
    QDateTime dt;
    if (str.count() == 20) {
        dt = QDateTime::fromString(str.left(17),
                                   "yy/MM/dd,HH:mm:ss").addYears(100);
    }
    if (dt.isNull()) {
        qDebug() << "parseTimestamp(): invalid date" << str;
    }
    return dt;
}

static QString getMsgId(const QString & contents, const QString & timestamp)
{
    QByteArray contentBytes = contents.toLatin1();
    quint16 crc = qChecksum(contentBytes.constData(), contentBytes.length());
    return QString("%1:%2").arg(timestamp).arg(crc);
}

/*
static QString fillMsg(const QOFonoSIMMessage & f, QSMSMessage & m, int index)
{
    qDebug() << "fillMsg index=" << index << "f.number=" << f.number +
        ", f.index=" << f.
        index << "f.timestamp=" << f.timestamp << ", f.contents=" << f.contents;

    m.setText(f.contents);
    m.setSender(f.number);
    m.setTimestamp(parseTimestamp(f.timestamp));

    return getMsgId(f.contents, f.timestamp);
}
*/

void OFonoSMSReader::firstMessage()
{
    qDebug() << "OFonoSMSReader::firstMessage()";
    index = 0;
    nextMessage();
}

void OFonoSMSReader::nextMessage()
{
    qDebug() << "OFonoSMSReader::nextMessage()";
/*    QSMSMessage msg;
    QString id;
    int incomingIndex = index - messages.count();
    if (index < messages.count()) {
        QOFonoSIMMessage f = messages.at(index);
        id = fillMsg(f, msg, index);
    } else if (incomingIndex < incoming.count()) {
        id = fillMsg(incoming.at(incomingIndex), msg, index);
    }
    index++;
    emit fetched(id, msg);*/
}

void OFonoSMSReader::deleteMessage(const QString & id)
{
    qDebug() << "OFonoSMSReader::deleteMessage() id=" << id;

/*
    // Find message by id
    for (int i = 0; i < messages.count(); i++) {
        QOFonoSIMMessage f = messages.at(i);
        QString msgId = getMsgId(f.contents, f.timestamp);
        if (msgId != id) {
    
            continue;
        }
        QOFonoDBusPendingReply <> delReply =
            service->gsmSim.DeleteMessage(f.index);
        checkReply(delReply);
    }
    */
}

void OFonoSMSReader::setUnreadCount(int value)
{
    qDebug() << "OFonoSMSReader::setUnreadCount() value=" << value;
}

void OFonoSMSReader::incomingTextMessage(const QString & number,
                                       const QString & timestamp,
                                       const QString & contents)
{
    qDebug() << "OFonoSMSReader::incomingTextMessage() number=" << number <<
        ", timestamp=" << timestamp << ", contents=" << contents;
/*        
        
    // Keep the message in memory in case that it does not make it to SIM
    // (e.g. SIM is full)
    QOFonoSIMMessage f;
    f.number = number;
    f.timestamp = timestamp;
    f.contents = contents;
    incoming.append(f);

    check(); */
}
