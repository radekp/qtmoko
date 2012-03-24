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

#include "alarmcontrol.h"
#include "qabstractmessagebox.h"
#include "qtopiaserverapplication.h"
#include <QSettings>
#include <QtopiaIpcEnvelope>
#include <QtopiaServiceRequest>


#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

struct timerEventItem {
    uint UTCtime;
    QString channel, message;
    int data;
    bool operator==( const timerEventItem &right ) const
    {
        return ( UTCtime == right.UTCtime
                 && channel == right.channel
                 && message == right.message
                 && data == right.data );
    }
};

class TimerReceiverObject : public QObject
{
    Q_OBJECT
public:
    TimerReceiverObject() : timerId(0) { }
    ~TimerReceiverObject() { }

    void resetTimer();
    void setTimerEventItem();
    void deleteTimer();
    void killTimers() { if (timerId) killTimer(timerId); timerId = 0; }

protected:
    void timerEvent( QTimerEvent *te );
private:
    QString atfilename;
    int timerId;
};

TimerReceiverObject *timerEventReceiver = NULL;
QList<timerEventItem*> timerEventList;
timerEventItem *nearestTimerEvent = NULL;

static const char* atdir = "/var/spool/at/";

static bool triggerAtd()
{
    QFile trigger(QString(atdir) + "trigger"); // No tr
    if ( trigger.open(QIODevice::WriteOnly) ) {

        const char* data = "\n";
        int len = strlen(data);
        int total_written = trigger.write(data,len);
        if ( total_written != len ) {
            QAbstractMessageBox::critical( 0, qApp->translate( "AlarmServer",  "Out of Space" ),
                                   qApp->translate( "AlarmServer", "<qt>Unable to schedule alarm. Free some memory and try again.</qt>" ) );
            trigger.close();
            QFile::remove( trigger.fileName() );
            return false;
        }
        return true;
    }
    return false;
}


// set the timer to go off on the next event in the list
void setNearestTimerEvent()
{
    nearestTimerEvent = NULL;
    QList<timerEventItem*>::const_iterator it = timerEventList.begin();
    if ( it != timerEventList.end() )
        nearestTimerEvent = *it;
    for ( ; it != timerEventList.end(); ++it ) {
        if ( (*it)->UTCtime < nearestTimerEvent->UTCtime )
            nearestTimerEvent = *it;
    }
    if (nearestTimerEvent)
        timerEventReceiver->resetTimer();
    else
        timerEventReceiver->deleteTimer();
}


//store current state to file
//Simple implementation. Should run on a timer.
static void saveState()
{
    QString savefilename = Qtopia::applicationFileName( "AlarmServer", "saveFile" );
    if ( timerEventList.isEmpty() ) {
        unlink( savefilename.toAscii().constData() );
        return;
    }

    QFile savefile(savefilename+".new");
    if ( savefile.open(QIODevice::WriteOnly) ) {
        QDataStream ds( &savefile );

        //save
        timerEventItem *item;
        foreach (item, timerEventList) {
            ds << (quint32)item->UTCtime;
            ds << item->channel;
            ds << item->message;
            ds << item->data;
        }

        savefile.close();
        unlink( savefilename.toAscii().constData() );
        QDir d; d.rename(savefilename+".new",savefilename);
    }
}


void TimerReceiverObject::deleteTimer()
{
    if ( !atfilename.isEmpty() ) {
        unlink( atfilename.toAscii().constData() );
        atfilename = QString();
        triggerAtd();
    }
}

void TimerReceiverObject::resetTimer()
{
    const int maxsecs = 2147000;
    int total_written;
    QDateTime nearest;
    nearest.setTime_t(nearestTimerEvent->UTCtime);
    QDateTime now = QDateTime::currentDateTime();
    if ( nearest < now )
        nearest = now;
    int secs = now.secsTo(nearest);
    if ( secs > maxsecs ) {
        // too far for millisecond timing
        secs = maxsecs;
    }

    // System timer (needed so that we wake from deep sleep),
    // from the Epoch in seconds.
    //
    int at_secs = nearest.toTime_t();
    QString fn = atdir + QString::number(at_secs) + "."
                 + QString::number(::getpid());
    if ( fn != atfilename ) {
        QFile atfile(fn+".new");
        if ( atfile.open(QIODevice::WriteOnly) ) {
            // just wake up and delete the at file
            QString cmd = "#!/bin/sh\nrm " + fn;
            total_written = atfile.write(cmd.toLatin1(),cmd.length());
            if ( total_written != int(cmd.length()) ) {
                QAbstractMessageBox::critical( 0, tr("Out of Space"),
                                       tr("<qt>Unable to schedule alarm. "
                                          "Please free up space and try again</qt>"));
                atfile.close();
                QFile::remove( atfile.fileName() );
                return;
            }
            atfile.close();
            unlink( atfilename.toAscii().constData() );
            QDir d; d.rename(fn+".new",fn);
            chmod(fn.toLatin1(),0755);
            atfilename = fn;
            triggerAtd();
        } else {
	  qWarning("Cannot open atd file %s",fn.toLatin1().constData());
        }
    }
    // Qt timers (does the actual alarm)
    // from now in milliseconds
    //
    static bool startup = true;
    if (secs < 5 && startup)   // To delay the alarm when Qtopia first starts.
        secs = 5;
    timerId = startTimer( 1000 * secs + 500 );
    startup = false;
}

void TimerReceiverObject::timerEvent( QTimerEvent * )
{
    bool needSave = false;
    if (timerId){
        killTimer(timerId);
        timerId = 0;
    }
    if (nearestTimerEvent) {
        if ( nearestTimerEvent->UTCtime
             <= QDateTime::currentDateTime().toTime_t() ) {
            QDateTime time;
            time.setTime_t(nearestTimerEvent->UTCtime);
            QString channel = nearestTimerEvent->channel;
            if ( !channel.contains( QChar('/') ) ) {
                QtopiaServiceRequest e( channel, nearestTimerEvent->message );
                e << time << nearestTimerEvent->data;
                e.send();

            } else {
                QtopiaIpcEnvelope e( channel, nearestTimerEvent->message );
                e << time << nearestTimerEvent->data;
            }

            timerEventList.removeAll(nearestTimerEvent);
            delete nearestTimerEvent;
            nearestTimerEvent = 0;
            needSave = true;
        }
        setNearestTimerEvent();
    } else {
        resetTimer();
    }
    if ( needSave )
        saveState();
}


/*!
    \service AlarmServerService AlarmServer
    \inpublicgroup QtBaseModule
    \brief The AlarmServerService class provides the Qt Extended AlarmServer service.

    The \i AlarmServer service is used by Qt Extended applications to add and
    delete alarm events.  It is normally accessed through Qtopia::addAlarm()
    and Qtopia::deleteAlarm().

    Messages for the alarm server are sent on the \c{QPE/AlarmServer}
    QCop channel.  The standard alarm server implementation is provided by the
    AlarmControl server task.

    \sa Qtopia::addAlarm(), Qtopia::deleteAlarm(), AlarmControl
*/


/*
  \internal
  Sets up the alarm server. Restoring to previous state (session management).
 */
void AlarmServerService::initAlarmServer()
{
    // read autosave file and put events in timerEventList
    QString savefilename = Qtopia::applicationFileName( "AlarmServer", "saveFile" );

    QFile savefile(savefilename);
    if ( savefile.open(QIODevice::ReadOnly) ) {
        QDataStream ds( &savefile );
        while ( !ds.atEnd() ) {
            timerEventItem *newTimerEventItem = new timerEventItem;
            quint32 UTCtime;
            ds >> UTCtime;
            newTimerEventItem->UTCtime = UTCtime;
            ds >> newTimerEventItem->channel;
            ds >> newTimerEventItem->message;
            ds >> newTimerEventItem->data;
            timerEventList.append( newTimerEventItem );
        }
        savefile.close();
        if (!timerEventReceiver)
            timerEventReceiver = new TimerReceiverObject;
        setNearestTimerEvent();
    }
    /*if ( !alarmServer )
        alarmServer = new AlarmServerService;*/
}


/*!
  \internal
  Creates an AlarmServerService instance with the passed \a parent.
*/
AlarmServerService::AlarmServerService( QObject *parent )
    : QtopiaIpcAdaptor( "QPE/AlarmServer", parent ), ctrl(0)
{ 
    publishAll( Slots ); 
    initAlarmServer();
}

/*!
  \internal
  Destroys the AlarmServerService instance.
*/
AlarmServerService::~AlarmServerService()
{
}

/*!
  Schedules an alarm to go off at (or soon after) time \a when. When
  the alarm goes off, the \l {Qt Extended IPC Layer}{Qt Extended IPC} \a message will
  be sent to \a channel, with \a data as a parameter.

  This slot corresponds to the QCop message
  \c{addAlarm(QDateTime,QString,QString,int)} on the
  \c{QPE/AlarmServer} channel.

  \sa Qtopia::addAlarm()
*/
void AlarmServerService::addAlarm
    ( QDateTime when, const QString& channel, const QString& message, int data )
{
    bool needSave = false;
    // Here we are the server so either it has been directly called from
    // within the server or it has been sent to us from a client via QCop
    if (!timerEventReceiver)
        timerEventReceiver = new TimerReceiverObject;

    timerEventItem *newTimerEventItem = new timerEventItem;
    newTimerEventItem->UTCtime = when.toTime_t();
    newTimerEventItem->channel = channel;
    newTimerEventItem->message = message;
    newTimerEventItem->data = data;
    // explore the case of already having the event in here...
    foreach (timerEventItem *item, timerEventList) {
        if ( *item == *newTimerEventItem ) {
            delete newTimerEventItem;
            return;
        }
    }
    // if we made it here, it is okay to add the item...
    timerEventList.append( newTimerEventItem );
    needSave = true;
    // quicker than using setNearestTimerEvent()
    if ( nearestTimerEvent ) {
        if (newTimerEventItem->UTCtime < nearestTimerEvent->UTCtime) {
            nearestTimerEvent = newTimerEventItem;
            timerEventReceiver->killTimers();
            timerEventReceiver->resetTimer();
        }
    } else {
        nearestTimerEvent = newTimerEventItem;
        timerEventReceiver->resetTimer();
    }
    if ( needSave )
        saveState();
}

/*!
  Deletes previously scheduled alarms which match \a when, \a channel,
  \a message, and \a data. If \a when is null any scheduled alarm that matches
  \a channel, \a message and \a data is deleted.

  This slot corresponds to the QCop message
  \c{deleteAlarm(QDateTime,QString,QString,int)} on the
  \c{QPE/AlarmServer} channel.

  \sa Qtopia::deleteAlarm()
*/
void AlarmServerService::deleteAlarm
    ( QDateTime when, const QString& channel, const QString& message, int data )
{
    bool needSave = false;
    if ( timerEventReceiver ) {
        timerEventReceiver->killTimers();

        // iterate over the list of events
        QMutableListIterator<timerEventItem*> it( timerEventList );
        uint deleteTime = when.toTime_t();
        bool updatenearest = false;
        while ( it.hasNext() ) {
            timerEventItem *event = it.next();
            // if its a match, delete it
            if ( ( event->UTCtime == deleteTime || when.isNull() )
                && ( channel.isNull() || event->channel == channel )
                && ( message.isNull() || event->message == message )
                && ( data==-1 || event->data == data ) )
            {
                // if it's first, then we need to update the timer
                if ( event == nearestTimerEvent ) {
                    updatenearest = true;
                    nearestTimerEvent = 0;
                }
                it.remove();
                delete event;
                needSave = true;
            }
        }

        if ( updatenearest )
            setNearestTimerEvent();
        else if ( nearestTimerEvent )
            timerEventReceiver->resetTimer();
    }
    if ( needSave )
        saveState();
}

/*!
  Message that indicates when the clock application turns the daily alarm
  on or off, according to \a flag.

  This slot corresponds to the QCop message \c{dailyAlarmEnabled(bool)} on
  the \c{QPE/AlarmServer} channel.
*/
void AlarmServerService::dailyAlarmEnabled( bool flag )
{
    Q_ASSERT( ctrl );
    ctrl->alarmEnabled( flag );
}




/*!
  \class AlarmControl
    \inpublicgroup QtBaseModule
  \ingroup QtopiaServer::Task
  \brief The AlarmControl class maintains Qt Extended alarm system.
  
  The AlarmControl class is a server task and provides an implementation of the AlarmServerService. Applications can access the alarm server
  via Qtopia::addAlarm() and Qtopia::deleteAlarm().
 
  This class is part of the Qt Extended server and cannot be used by other Qt Extended applications.

  \sa AlarmServerService
 */


/*!
  Create an AlarmControl instance with the passed \a parent.
  */
AlarmControl::AlarmControl(QObject* parent )
: QObject( parent ) , alarmOn(false), alarmValueSpace("/UI/DailyAlarm")
{
    AlarmServerService* srv = new AlarmServerService( this );
    srv->ctrl = this;

    QSettings clockCfg("Trolltech","Clock");
    clockCfg.beginGroup( "Daily Alarm" );
    bool alarm = clockCfg.value("Enabled", false).toBool();
    alarmOn = !alarm;
    alarmEnabled(alarm);
}

/*!
  Sets the daily alarm flag to \a on.
  */
void AlarmControl::alarmEnabled(bool on)
{
    if(on != alarmOn) {
        alarmOn = on;
        alarmValueSpace.setAttribute("", alarmOn);
        emit alarmStateChanged(alarmOn);
    }
}

/*!
  \fn void AlarmControl::alarmStateChanged(bool alarmOn)

  Emitted whenever the daily alarm flag changes. \a alarmOn reflects the new state.

  \sa alarmEnabled()
  */

/*!
  Returns true if the daily alarm flag is on, otherwise false.

  \sa AlarmServerService::dailyAlarmEnabled()
  */
bool AlarmControl::alarmState() const
{
    return alarmOn;
}

QTOPIA_TASK(AlarmControl,AlarmControl);
#include "alarmcontrol.moc"
