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

#include "qgpsdwhereabouts.h"

#include <gps.h>
#include <errno.h>
#include <unistd.h>

#include <QBasicTimer>
#include <QDateTime>
#include <QTimerEvent>
#include <QDebug>

//#define DEBUG
/* just for convenience. You can easily switch off all output */
#ifdef DEBUG
	#define debugprintf(x, y...) qDebug("qGpsdWhereabouts : " x , ##y )
#else
	#define debugprintf(x, y...) while(0)
#endif
#define errorprintf(x, y...) qDebug(x , ##y )

/* This is the polling time for our polling timer. */
#define POLL_MS 300

/*!
    \internal
    \class QGpsdWhereabouts
    \inpublicgroup QtLocationModule
    \ingroup whereabouts
    \brief The QGpsdWhereabouts class reads and distributes the positional data received from a GPSd daemon.
    GPSd is a service daemon that connects to a GPS device and then serves the
    data to clients over TCP. It is available at http://gpsd.berlios.de.
*/

/*!
    Constructs a QGpsdWhereabouts instance with the given \a parent that
    connects to the GPSd daemon on the given \a addr and \a port.
*/
QGpsdWhereabouts::QGpsdWhereabouts(QObject *parent, const QHostAddress &addr, quint16 port)
    : QWhereabouts(QWhereabouts::TerminalBasedUpdate, parent),
      m_address(addr),
      m_port(port),
      m_queryTimer(0)
{
    active = false;
    emit_frequent = false;
    emit_oneshot = false;
    elapsed_time = 0;
    setState(QWhereabouts::NotAvailable);
}

/*!
    Destroys the instance.
*/
QGpsdWhereabouts::~QGpsdWhereabouts()
{
    debugprintf("In ~QGpsdWhereabouts");
    deactivateGps();
}

/*!
    \reimp
*/
void QGpsdWhereabouts::startUpdates()
{
    setState(QWhereabouts::Initializing);
    activateGps();

    emit_frequent = true;
}

/*!
    \reimp
*/
void QGpsdWhereabouts::stopUpdates()
{
    emit_frequent = false;
    emit_oneshot = false;
    deactivateGps();
}

/*!
    \reimp
*/
void QGpsdWhereabouts::requestUpdate()
{
    debugprintf( "requestUpdate called");
    setState(QWhereabouts::Initializing);
    activateGps();
    emit_oneshot = true;
}


/*!
    \internal
*/
void QGpsdWhereabouts::timerEvent(QTimerEvent *event)
{
    Q_UNUSED(event);
    if(!active) {
        debugprintf("timerEvent but no GPSD");
        return;
    }

    /* Always start at 0 when emit_frequent get's enabled */
    if(!emit_frequent)
       elapsed_time = 0;
    else
       elapsed_time += POLL_MS;

#if GPSD_API_MAJOR_VERSION >= 5
    if(!gps_waiting(gps_data,1)) {
        //debugprintf( "timerEvent, but no data avilable");
        return;
    }
#endif    

    if(gps_read(gps_data) == -1) {
        errorprintf("timerEvent: read error");
    }
    else {
        debugprintf("Got set: %x", (unsigned int)gpsdata.set);
        parseFix(gps_data);
    }
}

/*!
    Parses one gps data collection from libgps and emits update if activated
    and data is sufficient.
*/
void QGpsdWhereabouts::parseFix(struct gps_data_t *fix)
{
    int satellites_used = -1;
    QWhereaboutsUpdate update;

    /* Get number of sats used if we have that */
    if(fix->set & SATELLITE_SET) {
        satellites_used = fix->satellites_used;
	debugprintf("DOP_SET: got %d sattelites", satellites_used);
    }

    /* update state if we got it */
    if(fix->set & MODE_SET) {
        switch(fix->fix.mode) {
	    case 1:
		setState(QWhereabouts::Available, satellites_used);
                debugprintf("MODE_SET: mode Available");
	        break;
	    case 2:
	    case 3:
		setState(QWhereabouts::PositionFixAcquired, satellites_used);
                debugprintf("MODE_SET: mode PositionFixAquired");
	        break;
	    default:
                setState(QWhereabouts::Available);
                debugprintf("MODE_SET: mode Available");
	        break;
        }
    }

    /* update time */
    if(fix->set & TIME_SET) {
        QDateTime dt = QDateTime::fromTime_t(fix->fix.time);
        dt = dt.toUTC();
        update.setUpdateDateTime(dt);
	debugprintf("TIME_SET: time %s", dt.toString().toUtf8().constData());
    }

    /* update coordinates */
    if(fix->set & (LATLON_SET | ALTITUDE_SET))
    {
        QWhereaboutsCoordinate coord;
        if(fix->set & LATLON_SET) {
	    coord.setLatitude(fix->fix.latitude);
	    coord.setLongitude(fix->fix.longitude);
	    debugprintf("LATLON_SET: lat = %lf lon = %lf", fix->fix.latitude,
	                                                fix->fix.longitude);
	}
	if(fix->set & ALTITUDE_SET) {
            coord.setAltitude(fix->fix.altitude);
	    debugprintf("LATLON_SET: alt = %lf", fix->fix.altitude);
	}
        update.setCoordinate(coord);
    }

    /* update enhanced position values */
    if(fix->set & TRACK_SET) {
        update.setCourse(fix->fix.track);
    }
    if(fix->set & SPEED_SET) {
        update.setGroundSpeed(fix->fix.speed);
    }
    if(fix->set & CLIMB_SET) {
        update.setVerticalSpeed(fix->fix.climb);
    }

    /* update accuracy */
    if(fix->set & VERR_SET) {
        update.setVerticalAccuracy(fix->fix.epy);
    }
    if(fix->set & HERR_SET) {
        update.setHorizontalAccuracy(fix->fix.epx);
    }
    if(fix->set & TIMERR_SET) {
	update.setUpdateTimeAccuracy(fix->fix.ept);
    }
    if(fix->set & SPEEDERR_SET) {
        update.setGroundSpeedAccuracy(fix->fix.eps);
    }
    if(fix->set & TRACKERR_SET) {
        update.setCourseAccuracy(fix->fix.epd);
    }
    if(fix->set & CLIMBERR_SET) {
        update.setVerticalSpeedAccuracy(fix->fix.epc);
    }

    /* Shall we emit at all */
    if(emit_oneshot || (emit_frequent && elapsed_time >= updateInterval()))
    {
        /* Check if this package has enough data to be a valid update */
        if((fix->set & TIME_SET) && (fix->set & LATLON_SET)) {
	    emit_oneshot = false;
	    elapsed_time = 0; /* 0 ms since last update */
            debugprintf("Emit updated");
            if (!update.isNull())
                 emitUpdated(update);
        }
    }

}

/*!
    Connect with gpsd via libgps and start periodic timer polling for updates
    from libpgps.
*/
bool QGpsdWhereabouts :: activateGps()
{
    int ret;
    if(active)
       return true;
    QString s_port = QString::number(m_port);
    
#if GPSD_API_MAJOR_VERSION >= 5
    gps_data = &gpsdata;

    /* gps_open returns 0 on success */
    ret = gps_open(m_address.toString().toUtf8().constData(),
                   s_port.toUtf8().constData(), gps_data);
#else
    gps_data = gps_open(m_address.toString().toUtf8().constData(),
                        s_port.toUtf8().constData());
    ret = (gps_data == 0);
#endif
    
    if(ret != 0)
    {
        /* Sh*t. Seem there is no gpsd */
        errorprintf("No GPSD running. %s", gps_errstr(errno));
	active = false;
        setState(QWhereabouts::NotAvailable);
	return false;
    }
    active = true;

    /* start polling timer */
    m_queryTimer = new QBasicTimer;
    /* poll with 300 msec */
    m_queryTimer->start(POLL_MS, this);

    /* Tell gpsd we want updates */
    ret = gps_stream(gps_data, WATCH_ENABLE|WATCH_JSON, NULL);
    if(ret != 0){
        errorprintf("No GPSD running.");
    }
    return true;
}

/*!
    Disconnects from libgps and destroys update timer.
*/
void QGpsdWhereabouts :: deactivateGps()
{
    if (m_queryTimer)
    {
        m_queryTimer->stop();
        delete m_queryTimer;
    }
    m_queryTimer = NULL;

    if(active) {
        debugprintf("Shut down GPSD.");
        gps_stream(gps_data, WATCH_DISABLE, NULL);
	sleep(2); /* give gpsd some time to shut down */
        gps_close(gps_data);
    }
    setState(QWhereabouts::NotAvailable);
    active = false;
}

