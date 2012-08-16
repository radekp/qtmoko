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

#ifndef QNMEAWHEREABOUTS_P_H
#define QNMEAWHEREABOUTS_P_H

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

#include "qnmeawhereabouts.h"
#include <QWhereaboutsUpdate>

#include <QObject>
#include <QQueue>

class QNmeaReader;
class QBasicTimer;
class QTimerEvent;


struct QWhereaboutsUpdateInfo
{
    QWhereaboutsUpdate update;
    QWhereaboutsUpdate::PositionFixStatus status;
    int numSatellites;
};


class QNmeaWhereaboutsPrivate : public QObject
{
    Q_OBJECT
public:
    explicit QNmeaWhereaboutsPrivate(QNmeaWhereabouts *parent);
    ~QNmeaWhereaboutsPrivate();

    void startUpdates();
    void stopUpdates();
    void requestUpdate();

    void notifyNewUpdate(QWhereaboutsUpdate *update, QWhereaboutsUpdate::PositionFixStatus fixStatus, int numSatellites);
    void notifyReachedEndOfFile();

    QNmeaWhereabouts::UpdateMode m_updateMode;
    QPointer<QIODevice> m_source;

public slots:
    void readyRead();

protected:
    void timerEvent(QTimerEvent *event);

private slots:
    void emitPendingUpdate();
    void sourceDataClosed();

private:
    bool openSourceDevice();
    bool initialize();
    void prepareSourceDevice();

    QNmeaWhereabouts *m_whereabouts;
    bool m_invokedStart;
    QNmeaReader *m_nmeaReader;
    QBasicTimer *m_updateTimer;
    bool m_requestedUpdate;
    QWhereaboutsUpdate m_pendingUpdate;
    QDate m_currentDate;
};


class QNmeaReader
{
public:
    explicit QNmeaReader(QNmeaWhereaboutsPrivate *whereaboutsProxy)
        : m_proxy(whereaboutsProxy) {}
    virtual ~QNmeaReader() {}

    virtual void sourceReadyRead() = 0;

protected:
    QNmeaWhereaboutsPrivate *m_proxy;
};


class QNmeaRealTimeReader : public QNmeaReader
{
public:
    explicit QNmeaRealTimeReader(QNmeaWhereaboutsPrivate *whereaboutsProxy);
    ~QNmeaRealTimeReader();
    virtual void sourceReadyRead();

private:
    char readBuf[1024];
    int readPos;
    bool ledOn;
};


class QNmeaSimulatedReader : public QObject, public QNmeaReader
{
    Q_OBJECT
public:
    explicit QNmeaSimulatedReader(QNmeaWhereaboutsPrivate *whereaboutsProxy);
    ~QNmeaSimulatedReader();
    virtual void sourceReadyRead();

protected:
    virtual void timerEvent(QTimerEvent *event);

private slots:
    void simulatePendingUpdate();

private:
    bool setFirstDateTime();
    void processNextSentence();

    QQueue<QWhereaboutsUpdateInfo> m_pendingUpdatesInfo;
    int m_currTimerId;
    bool m_hasValidDateTime;
};


#endif
