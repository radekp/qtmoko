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

#include "qnmeawhereabouts.h"
#include "qnmeawhereabouts_p.h"

#include <qled.h>
#include <QIODevice>
#include <QBasicTimer>
#include <QTimerEvent>
#include <QDebug>


QNmeaRealTimeReader::QNmeaRealTimeReader(QNmeaWhereaboutsPrivate *whereaboutsProxy)
    : QNmeaReader(whereaboutsProxy)
    , readPos(0)
    , ledOn(false)
{
}

QNmeaRealTimeReader::~QNmeaRealTimeReader()
{
    qLedSetPower(qLedAttrBrightness(), "0");
}

void QNmeaRealTimeReader::sourceReadyRead()
{
    QWhereaboutsUpdate update;
    QWhereaboutsUpdate::PositionFixStatus fixStatus = QWhereaboutsUpdate::FixStatusUnknown;
    int numSatellites;
    
    for(;;)
    {
        quint64 res = m_proxy->m_source->readLine(readBuf + readPos, sizeof(readBuf) - readPos);
     
        if(res <= 0)
            return;

        readPos += res;
        if(readBuf[readPos - 1] != '\n')
            return;

        QByteArray line(readBuf, readPos);
        update = QWhereaboutsUpdate::fromNmea(line, &fixStatus, &numSatellites);
        readPos = 0;
        if (update.isNull())
            continue;
        
        m_proxy->notifyNewUpdate(&update, fixStatus, numSatellites);

        // Blink with led until some satellites are shown to user
        if(numSatellites >= 0) {        // -1 means that no satellite info was in this nmea
            if(ledOn) {
                qLedSetPower(qLedAttrBrightness(), "0");
                ledOn = false;
            }
            else if(fixStatus != QWhereaboutsUpdate::FixAcquired) {
                qLedSetPower(qLedAttrBrightness(), qLedMaxBrightness());
                ledOn = true;
            }
        }
    }
}


//============================================================

QNmeaSimulatedReader::QNmeaSimulatedReader(QNmeaWhereaboutsPrivate *whereaboutsProxy)
    : QNmeaReader(whereaboutsProxy),
      m_currTimerId(-1),
      m_hasValidDateTime(false)
{
}

QNmeaSimulatedReader::~QNmeaSimulatedReader()
{
    if (m_currTimerId > 0)
        killTimer(m_currTimerId);
}

void QNmeaSimulatedReader::sourceReadyRead()
{
    if (m_currTimerId > 0)     // we are already reading
        return;

    if (!m_hasValidDateTime) {      // first update
        Q_ASSERT(m_proxy->m_source && (m_proxy->m_source->openMode() & QIODevice::ReadOnly));

        if (!setFirstDateTime()) {
            m_proxy->notifyReachedEndOfFile();
            qWarning("QNmeaWhereabouts: cannot find NMEA sentence with valid date & time");
            return;
        }

        m_hasValidDateTime = true;
        simulatePendingUpdate();

    } else {
        // previously read to EOF, but now new data has arrived
        processNextSentence();
    }
}

bool QNmeaSimulatedReader::setFirstDateTime()
{
    // find the first update with date and time both valid
    QWhereaboutsUpdate update;
    QWhereaboutsUpdate::PositionFixStatus fixStatus;
    int numSatellites;
    while (m_proxy->m_source->bytesAvailable() > 0) {
        char buf[1024];
        if (m_proxy->m_source->readLine(buf, sizeof(buf)) <= 0)
            continue;
        update = QWhereaboutsUpdate::fromNmea(QByteArray(buf), &fixStatus, &numSatellites);
        if (update.updateDateTime().isValid()) {
            QWhereaboutsUpdateInfo info;
            info.update = update;
            info.status = fixStatus;
            info.numSatellites = numSatellites;
            m_pendingUpdatesInfo.enqueue(info);
            return true;
        }
    }
    return false;
}

void QNmeaSimulatedReader::simulatePendingUpdate()
{
    if (m_pendingUpdatesInfo.size() > 0) {
        // will be dequeued in processNextSentence()
        QWhereaboutsUpdateInfo &pendingUpdate = m_pendingUpdatesInfo.head();
        if (pendingUpdate.update.coordinate().type() != QWhereaboutsCoordinate::InvalidCoordinate)
            m_proxy->notifyNewUpdate(&pendingUpdate.update, pendingUpdate.status, pendingUpdate.numSatellites);
    }

    processNextSentence();
}

void QNmeaSimulatedReader::timerEvent(QTimerEvent *event)
{
    killTimer(event->timerId());
    m_currTimerId = -1;
    simulatePendingUpdate();
}

void QNmeaSimulatedReader::processNextSentence()
{
    // find the next update with a valid time (as long as the time is valid,
    // we can calculate when the update should be emitted)
    QWhereaboutsUpdate update;
    QWhereaboutsUpdate::PositionFixStatus fixStatus;
    int numSatellites;
    while (!update.updateTime().isValid()) {
        if (m_proxy->m_source->bytesAvailable() <= 0)
            return;
        char buf[1024];
        if (m_proxy->m_source->readLine(buf, sizeof(buf)) <= 0)
            continue;
        update = QWhereaboutsUpdate::fromNmea(QByteArray(buf), &fixStatus, &numSatellites);
    }

    // see when it should be emitted (i.e. time from last update to this one)
    int timeToNextUpdate = 0;
    if (m_pendingUpdatesInfo.size() > 0)
        timeToNextUpdate = m_pendingUpdatesInfo.dequeue().update.updateTime().msecsTo(update.updateTime());
    if (timeToNextUpdate < 0)
        timeToNextUpdate = 0;

    QWhereaboutsUpdateInfo info;
    info.update = update;
    info.status = fixStatus;
    info.numSatellites = numSatellites;
    m_pendingUpdatesInfo.enqueue(info);
    m_currTimerId = startTimer(timeToNextUpdate);
}


//============================================================


QNmeaWhereaboutsPrivate::QNmeaWhereaboutsPrivate(QNmeaWhereabouts *parent)
    : QObject(parent),
      m_whereabouts(parent),
      m_invokedStart(false),
      m_nmeaReader(0),
      m_updateTimer(0),
      m_requestedUpdate(false)
{
}

QNmeaWhereaboutsPrivate::~QNmeaWhereaboutsPrivate()
{
    delete m_nmeaReader;
    delete m_updateTimer;
}

bool QNmeaWhereaboutsPrivate::openSourceDevice()
{
    if (!m_source) {
        qWarning("QNmeaWhereabouts: invalid QIODevice data source");
        return false;
    }

    if (!m_source->isOpen() && !m_source->open(QIODevice::ReadOnly)) {
        qWarning("QNmeaWhereabouts: cannot open QIODevice data source");
        return false;
    }

    connect(m_source, SIGNAL(aboutToClose()), SLOT(sourceDataClosed()));
    connect(m_source, SIGNAL(readChannelFinished()), SLOT(sourceDataClosed()));
    connect(m_source, SIGNAL(destroyed()), SLOT(sourceDataClosed()));

    return true;
}

void QNmeaWhereaboutsPrivate::sourceDataClosed()
{
    if (m_nmeaReader && m_source && m_source->bytesAvailable())
        m_nmeaReader->sourceReadyRead();
    m_whereabouts->setState(QWhereabouts::NotAvailable);
}

void QNmeaWhereaboutsPrivate::readyRead()
{
    if (m_whereabouts->state() == QWhereabouts::NotAvailable)
        m_whereabouts->setState(QWhereabouts::Available);

    if (m_nmeaReader)
        m_nmeaReader->sourceReadyRead();
}

bool QNmeaWhereaboutsPrivate::initialize()
{
    if (m_nmeaReader)
        return true;    // already initialized

    m_whereabouts->setState(QWhereabouts::Initializing);

    if (m_updateMode == QNmeaWhereabouts::InvalidMode
                || !openSourceDevice()) {
        m_whereabouts->setState(QWhereabouts::NotAvailable);
        return false;
    }

    m_whereabouts->setState(QWhereabouts::Available);

    if (m_updateMode == QNmeaWhereabouts::RealTimeMode) {
        m_nmeaReader = new QNmeaRealTimeReader(this);
    } else {
        m_nmeaReader = new QNmeaSimulatedReader(this);
    }

    return true;
}

void QNmeaWhereaboutsPrivate::prepareSourceDevice()
{
    // some data may already be available
    if (m_updateMode == QNmeaWhereabouts::SimulationMode) {
        if (m_nmeaReader && m_source->bytesAvailable())
            m_nmeaReader->sourceReadyRead();
    }

    connect(m_source, SIGNAL(readyRead()), SLOT(readyRead()));
}

void QNmeaWhereaboutsPrivate::startUpdates()
{
    m_invokedStart = true;
    m_pendingUpdate.clear();

    bool initialized = initialize();
    if (!initialized)
        return;

    // skip over any buffered data - we only want the newest data
    if (m_updateMode == QNmeaWhereabouts::RealTimeMode) {
        if (m_source->bytesAvailable()) {
            if (m_source->isSequential())
                m_source->readAll();
            else
                m_source->seek(m_source->bytesAvailable());
        }
    }

    if (m_updateTimer)
        m_updateTimer->stop();

    if (m_whereabouts->updateInterval() > 0) {
        if (!m_updateTimer)
            m_updateTimer = new QBasicTimer;
        m_updateTimer->start(m_whereabouts->updateInterval(), this);
    }

    if (initialized)
        prepareSourceDevice();
}

void QNmeaWhereaboutsPrivate::stopUpdates()
{
    m_invokedStart = false;
    if (m_updateTimer)
        m_updateTimer->stop();
    m_pendingUpdate.clear();
}

void QNmeaWhereaboutsPrivate::requestUpdate()
{
    bool initialized = initialize();
    if (!initialized)
        return;

    m_requestedUpdate = true;

    if (initialized)
        prepareSourceDevice();
}

void QNmeaWhereaboutsPrivate::notifyNewUpdate(QWhereaboutsUpdate *update, QWhereaboutsUpdate::PositionFixStatus fixStatus, int numSatellites)
{
    QDate date = update->updateDate();
    if (date.isValid()) {
        m_currentDate = date;
    } else {
        // some sentence have time but no date
        QTime time = update->updateTime();
        if (time.isValid() && m_currentDate.isValid())
            update->setUpdateDateTime(QDateTime(m_currentDate, time, Qt::UTC));
    }

    switch (fixStatus) {
        case QWhereaboutsUpdate::FixNotAcquired:
            m_whereabouts->setState(QWhereabouts::Available, numSatellites);
            break;
        case QWhereaboutsUpdate::FixAcquired:
            m_whereabouts->setState(QWhereabouts::PositionFixAcquired, numSatellites);
            break;
        default:
            break;
    }

    if (fixStatus != QWhereaboutsUpdate::FixNotAcquired) {
        if (update->updateDateTime().isValid()
                && update->coordinate().type() != QWhereaboutsCoordinate::InvalidCoordinate) {
            if (m_invokedStart) {
                if (m_updateTimer && m_updateTimer->isActive()) {
                    // for periodic updates, only want the most recent update
                    m_pendingUpdate = *update;
                } else {
                    m_whereabouts->emitUpdated(*update);
                }
            }

            if (m_requestedUpdate)
                m_whereabouts->emitUpdated(*update);
        }
    }

    // cancels update request if no fix available at the moment
    m_requestedUpdate = false;
}

void QNmeaWhereaboutsPrivate::timerEvent(QTimerEvent *)
{
    emitPendingUpdate();
}

void QNmeaWhereaboutsPrivate::emitPendingUpdate()
{
    if (!m_pendingUpdate.isNull()) {
        m_whereabouts->emitUpdated(m_pendingUpdate);
        m_pendingUpdate.clear();
    }
}

void QNmeaWhereaboutsPrivate::notifyReachedEndOfFile()
{
    if (m_whereabouts->state() == QWhereabouts::PositionFixAcquired)
        m_whereabouts->setState(QWhereabouts::Available);
}


//=========================================================

/*!
    \class QNmeaWhereabouts
    \inpublicgroup QtLocationModule
    \ingroup whereabouts
    \brief The QNmeaWhereabouts class provides positional information using a NMEA data source.

    NMEA is a commonly used protocol for the specification of one's global
    position at a certain point in time. The QNmeaWhereabouts class reads NMEA
    data and uses it to provide positional data in the form of
    QWhereaboutsUpdate objects.

    A QNmeaWhereabouts instance operates in either \l {RealTimeMode} or
    \l {SimulationMode}. These modes allow NMEA data to be read from either a
    live source of positional data, or replayed for simulation purposes from
    previously recorded NMEA data.

    Use setUpdateMode() to define the update mode, and setSourceDevice() to
    set the source of NMEA data.

    Use startUpdates() to receive regular position updates through the updated()
    signal, and stopUpdates() to stop these updates. If you only require
    updates occasionally, you can call requestUpdate() as required, instead
    of startUpdates() and stopUpdates().
*/


/*!
    \enum QNmeaWhereabouts::UpdateMode
    Defines the available update modes.

    \value InvalidMode Updates cannot be provided in this mode. This is the default mode.
    \value RealTimeMode Positional data is read and distributed from the data source as it becomes available. Use this mode if you are using a live source of positional data (for example, a GPS hardware device).
    \value SimulationMode The data and time information in the NMEA source data is used to provide positional updates at the rate at which the data was originally recorded. if the data source contains previously recorded NMEA data and you want to replay the data for simulation purposes.
*/


/*!
    Constructs a QNmeaWhereabouts instance with the given \a parent
    with the update mode set to QNmeaWhereabouts::InvalidMode.
*/
QNmeaWhereabouts::QNmeaWhereabouts(QObject *parent)
    : QWhereabouts(TerminalBasedUpdate, parent),
      d(new QNmeaWhereaboutsPrivate(this))
{
    d->m_updateMode = InvalidMode;
    d->m_source = 0;
}

/*!
    Constructs a QNmeaWhereabouts instance with the given \a parent
    and \a updateMode.
*/
QNmeaWhereabouts::QNmeaWhereabouts(UpdateMode updateMode, QObject *parent)
    : QWhereabouts(TerminalBasedUpdate, parent),
      d(new QNmeaWhereaboutsPrivate(this))
{
    d->m_updateMode = updateMode;
    d->m_source = 0;
}

/*!
    Destroys the whereabouts instance.
*/
QNmeaWhereabouts::~QNmeaWhereabouts()
{
    delete d;
}

/*!
    Sets the update mode to \a mode.

    The update mode can only be set once and must be set before calling
    startUpdates() or requestUpdate().
*/
void QNmeaWhereabouts::setUpdateMode(UpdateMode mode)
{
    if (mode != d->m_updateMode) {
        if (d->m_updateMode == InvalidMode)
            d->m_updateMode = mode;
        else
            qWarning("QNmeaWhereabouts: update mode has already been set");
    }
}

/*!
    Returns the update mode. The default mode is
    QNmeaWhereabouts::InvalidMode.
*/
QNmeaWhereabouts::UpdateMode QNmeaWhereabouts::updateMode() const
{
    return d->m_updateMode;
}

/*!
    Sets the NMEA data source to \a source. If the device is not open, it
    will be opened in QIODevice::ReadOnly mode.

    The source device can only be set once and must be set before calling
    startUpdates() or requestUpdate().

    \bold {Note:} If \a source does not emit QIODevice::readyRead()
    \unicode {0x2014} for example, if it is a QFile object
    \unicode {0x2014} you must call newDataAvailable() when data
    is available for reading.
*/
void QNmeaWhereabouts::setSourceDevice(QIODevice *source)
{
    if (source != d->m_source) {
        if (!d->m_source)
            d->m_source = source;
        else
            qWarning("QNmeaWhereabouts: source device has already been set");
    }
}

/*!
    Returns the NMEA data source.
*/
QIODevice *QNmeaWhereabouts::sourceDevice() const
{
    return d->m_source;
}

/*!
    Starts emitting updated() with the interval specified by updateInterval(),
    or less frequently if updates are not available at a particular time.
    If updateInterval() is 0, updated() is emitted as soon as a valid update
    becomes available.

    If startUpdates() has already been called, this restarts with updateInterval()
    as the new update interval.

    If the state is currently QWhereabouts::NotAvailable, the object will
    first be initialized and will begin to provide updates once it is in the
    QWhereabouts::PositionFixAcquired state.
*/
void QNmeaWhereabouts::startUpdates()
{
    d->startUpdates();
}

/*!
    Stops emitting updated() at regular intervals.

    \sa startUpdates()
*/
void QNmeaWhereabouts::stopUpdates()
{
    d->stopUpdates();
}

/*!
    Requests that updated() be emitted with the current whereabouts if
    possible. This can be called regardless of whether startUpdates() has already
    been called.

    This is useful if you need to retrieve the current whereabouts but you
    do not need the periodic updates offered by startUpdates(). Calling this
    method will result in only a single updated() signal.

    If the state is currently QWhereabouts::NotAvailable, the object will
    first be initialized and will then provide an update when it is in the
    QWhereabouts::PositionFixAcquired state.

    \sa startUpdates(), lastUpdate()
*/
void QNmeaWhereabouts::requestUpdate()
{
    d->requestUpdate();
}

/*!
    Notifies the object that new data is available for reading from the
    source device.

    Usually you do not need to call this function. It is only necessary
    if the source device does not emit QIODevice::readyRead().

    \sa setSourceDevice(), QSocketNotifier
*/
void QNmeaWhereabouts::newDataAvailable()
{
    if (d->m_source)
        d->readyRead();
}

#include "qnmeawhereabouts.moc"
