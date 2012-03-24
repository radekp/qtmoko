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

#ifndef NEOKBDHANDLER_H
#define NEOKBDHANDLER_H

#ifdef QT_QWS_GTA04

#include <QObject>
#include <QWSKeyboardHandler>
#include <QDebug>

#include <QValueSpaceItem>
//#include <qvibrateaccessory.h>
#include <QtopiaIpcAdaptor>

class QSocketNotifier;


/**
 * Start of a generic implementation to deal with the linux input event
 * handling. Open devices by physical address and later by name, product id
 * and vendor id
 */
class FicLinuxInputEventHandler : public QObject
{
    Q_OBJECT

public:
    FicLinuxInputEventHandler(QObject* parent);

    bool openByPhysicalBus(const QByteArray&);
    bool openByName(const QByteArray&);
    bool openById(const struct input_id&);
    
Q_SIGNALS:
    void inputEvent(struct input_event&);

private slots:
    void readData();

private:
    bool internalOpen(unsigned request, int length, const QByteArray&, struct input_id const * = 0);

private:
    int m_fd;
    QSocketNotifier* m_notifier;
};



class NeoKbdHandler : public QObject, public QWSKeyboardHandler
{
    Q_OBJECT

public:
    NeoKbdHandler();
    ~NeoKbdHandler();

private:
    QSocketNotifier *auxNotify;
    QSocketNotifier *powerNotify;

    QtopiaIpcAdaptor *mgr;
    QValueSpaceItem *m_headset;
     
    FicLinuxInputEventHandler *auxHandler;
    FicLinuxInputEventHandler *powerHandler;
    FicLinuxInputEventHandler *jackHandler;
     
private slots:
    void inputEvent(struct input_event&);
};

#endif // QT_QWS_GTA04

#endif
