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

#include "ofonophonebook.h"
#include "ofonotelephonyservice.h"

OFonoPhoneBook::OFonoPhoneBook(OFonoTelephonyService * service)
:  QPhoneBook(service->service(), service, QCommInterface::Server)
    , service(service)
    , wantEntries(false)
    , wantLimits(false)
    , numUsed(-1)
{
}

OFonoPhoneBook::~OFonoPhoneBook()
{
}

void OFonoPhoneBook::modemPropertyChanged(const QString & name,
                                          const QDBusVariant &)
{
    if (name != "Interfaces"
        || !service->interfaceAvailable(&service->oPhoneBook)) {
        return;
    }

    if (wantEntries) {
        import();
    }
    if (wantLimits) {
        emitLimits("SM");
    }
}

void OFonoPhoneBook::import()
{
    QOFonoDBusPendingCall call = service->oPhoneBook.Import();
    watchOFonoCall(call, this, SLOT(importFinished(QOFonoDBusPendingCall &)));
}

void OFonoPhoneBook::emitLimits(const QString & store)
{
    QPhoneBookLimits l;
    l.setUsed(numUsed);
    l.setFirstIndex(1);
    l.setLastIndex(numUsed);
    l.setNumberLength(1);
    l.setTextLength(1);
    emit limits(store, l);
}

void OFonoPhoneBook::importFinished(QOFonoDBusPendingCall & call)
{
    QOFonoDBusPendingReply < QString > reply = call;
    if (!reply.isValid()) {
        return;
    }
    QString str = reply.value();

    int index = 0;
    int entryIndex = 0;
    numUsed = 0;
    QList < QPhoneBookEntry > list;
    while ((index = str.indexOf("\nFN:", index)) > 0) {
        index += 4;
        int end = str.indexOf("\n", index) - 1;
        if (end < 0) {
            break;
        }
        QString name = str.mid(index, end - index);
        index = str.indexOf("TEL;TYPE=VOICE:", index);
        if (index < 0) {
            break;
        }
        index += 16;
        end = str.indexOf("\n", index) - 1;
        if (end < 0) {
            break;
        }
        QString number = str.mid(index, end - index);

        QPhoneBookEntry item;
        item.setIndex(++entryIndex);
        item.setNumber(number);
        item.setText(name);
        list.append(item);
    }
    qDebug() << "OFonoPhoneBook contacts imported count=" << list.count();
    emit entries("SM", list);
    emitLimits("SM");
}

void OFonoPhoneBook::getEntries(const QString & store)
{
    qDebug() << "OFonoPhoneBook::getEntries store=" << store;

    // We support only "SM" store
    if (store != "SM") {
        QList < QPhoneBookEntry > list;
        emit entries(store, list);
        return;
    }

    if (!service->interfaceAvailable(&service->oPhoneBook)) {
        wantEntries = true;
        return;
    }
    import();
}

void OFonoPhoneBook::add(const QPhoneBookEntry & entry, const QString & store,
                         bool flush)
{
    // oFono does not support adding contacts to sim
}

void OFonoPhoneBook::remove(uint index, const QString & store, bool flush)
{
    // oFono does not support removing contacts from sim
}

void OFonoPhoneBook::update(const QPhoneBookEntry & entry,
                            const QString & store, bool flush)
{
    // oFono does not support updating contacs on sim
}

void OFonoPhoneBook::flush(const QString & store)
{
    // oFono can only read contacts on sim
}

void OFonoPhoneBook::setPassword(const QString &, const QString &)
{
    // Nothing to do here.
}

void OFonoPhoneBook::clearPassword(const QString &)
{
    // Nothing to do here.
}

void OFonoPhoneBook::requestLimits(const QString & store)
{
    qDebug() << "OFonoPhoneBook::requestLimits store=" << store;

    if (!service->interfaceAvailable(&service->oPhoneBook) && numUsed < 0) {
        wantLimits = true;
        return;
    }
    emitLimits(store);
}

void OFonoPhoneBook::requestFixedDialingState()
{
    qDebug() << "OFonoPhoneBook::requestFixedDialingState";
    bool fixedDialingEnabled = false;
    emit fixedDialingState(fixedDialingEnabled);
}

void OFonoPhoneBook::setFixedDialingState(bool enabled, const QString &)
{
    qDebug() << "OFonoPhoneBook::setFixedDialingState enabled=" << enabled;
    emit setFixedDialingStateResult(QTelephony::OK);
}
