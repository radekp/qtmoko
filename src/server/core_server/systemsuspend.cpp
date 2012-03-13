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

#include "systemsuspend.h"
#include "qtopiainputevents.h"
#include "qtopialog.h"
#include <QList>

/*!
  \class SystemSuspend
    \inpublicgroup QtBaseModule
  \brief The SystemSuspend class manages entering and leaving system suspend.
  \ingroup QtopiaServer::Task

  The SystemSuspend provides a Qt Extended Server Task.  Qt Extended Server Tasks are
  documented in full in the QtopiaServerApplication class documentation.

  \table
  \row \o Task Name \o SystemSuspend
  \row \o Interfaces \o SystemSuspend
  \row \o Services \o Suspend
  \endtable

  Server components may directly use the SystemSuspend interface to enter and 
  monitor the suspend state.  Non-server based applications should use the 
  SuspendService to do the same.

  The system suspend state is expected to be a very low, but non-destructive,
  power saving state.  As some hardware devices may need to be shutdown before 
  or reinitialized after entry into the suspend state, integrators can provide 
  objects that implement the SystemSuspendHandler interface that will be called
  before the system enters suspend and after the system leaves it.

  As SystemSuspend is a server task, and not a class, components within the 
  server must access it as such.  For example, for a server component to 
  put the device into suspend,

  \code
  SystemSuspend *suspend = qtopiaTask<SystemSuspend>();
  qWarning() << "About to suspend!";
  if(suspend->suspendSystem())
    qWarning() << "Resumed from suspend!";
  else
    qWarning() << "Suspend failed";
  \endcode

  As applications cannot access server tasks directly, they may use the Suspend
  service.

  \code
  QtopiaServiceRequest req("Suspend", "suspend()");
  req.send();
  \endcode

  This class is part of the Qt Extended server and cannot be used by other Qt Extended applications.
  \sa SystemSuspendHandler, SuspendService
 */

/*!
  \fn SystemSuspend::SystemSuspend(QObject *parent = 0)

  Construct a new SystemSuspend instance with the given \a parent.
  \internal
 */

/*!
  \fn bool SystemSuspend::suspendSystem()

  Request that the system be suspended.  The system will block in this method
  until execution is resumed.  The method returns true if the suspension was
  successful, and false otherwise.

  When called, all tasks in the system that implement the SystemSuspendHandler
  are instantiated.  If any of these tasks returns false from the
  SystemSuspendHandler::canSuspend() method, the suspend is canceled and the
  systemSuspendCanceled() signal is emitted.  Otherwise, the
  SystemSuspendHandler::suspend() methods are called on the tasks in reverse
  order.  That is, the task with the highest interface priority is called last.
  It is assumed that this last task will actually perform the hardware suspend -
  possibly using the standard "apm --suspend" system call.

  Once all the tasks have successfully completed the suspend, the
  SystemSuspendHandler::wake() method is invoked on each in-order.  This is done
  immediately after the last invokation of the SystemSuspendHandler::suspend()
  method, so it is important that this last handler actually suspends the device
  or, from the users perspective, the device will suspend and immediately
  resume.
 */

/*!
  \fn void SystemSuspend::systemSuspending()

  Emitted whenever the system begins suspending.  This is emitted after the
  SystemSuspendHandlers have all reported that the system is in a state to
  suspend.

  Following the systemSuspending() signal, a systemWaking() and systemActive()
  signals are guarenteed.
 */

/*!
  \fn void SystemSuspend::systemWaking()

  Emitted whenever the system is in the process of resuming from suspend.
 */

/*!
  \fn void SystemSuspend::systemActive()

  Emitted whenever the system has completed resuming from suspend.
 */

/*!
  \fn void SystemSuspend::systemSuspendCanceled()

  Emitted whenever a system suspend has been requested but a
  SystemSuspendHandler reported that it was not in a state to suspend.
  The suspendSystem() call that requested the suspend will return false.
 */

/*!
  \class SystemSuspendHandler
    \inpublicgroup QtBaseModule
  \brief The SystemSuspendHandler class provides an interface for tasks that
         provide system suspension or resumption functionality.
  \ingroup QtopiaServer::Task::Interfaces

  The SystemSuspendHandler provides a Qt Extended Server Task interface.  Qt Extended Server Tasks are documented in full in the QtopiaServerApplication class 
  documentation.

  Server components can use the SystemSuspendHandler to integrate into the
  suspend mechanism.

  Tasks that provide the SystemSuspendHandler interface will be called whenever
  a system suspend is requested through the SystemSuspend class.  More
  information on how a system suspend proceeds, including the order in which
  SystemSuspendHandler implementers are called, is available in the 
  documentation for that class.

  This class is part of the Qt Extended server and cannot be used by other Qt Extended applications.
  \sa SystemSuspend
 */

/*!
  \fn SystemSuspendHandler::SystemSuspendHandler(QObject *parent = 0)

  Construct the handler with the given \a parent.
 */

/*!
  \fn bool SystemSuspendHandler::canSuspend() const

  Returns true if the handler is capable of suspending at the time it is called.
  Suspend handlers can return false from this call to prevent the system from
  entering a suspended state.  This might be useful to prevent a suspend during
  a device synchronization or other scenarios where it may be advantageous
  not to enter such a state.
 */

/*!
  \fn bool SystemSuspendHandler::suspend()

  Perform the handler's suspend action.  If this method returns false, the
  suspend process will pause - entring the Qt event loop - until the handler
  emits the operationCompleted() signal.  This can be used to perform
  asynchronous actions in the handler.

  Normally this method will return true.
 */

/*!
  \fn bool SystemSuspendHandler::wake()

  Perform the handler's wake action.  If this method returns false, the
  wake process will pause - entering the Qt event loop - until the handler
  emits the operationCompleted() signal.  This can be used to perform
  asynchronous actions in the handler.

  Normally this method will return true.
 */

/*!
  \fn void SystemSuspendHandler::operationCompleted()

  Emitted to indicate that the suspend or wake process can continue.  This
  should only be emitted after previously returning false from suspend() or
  wake().
 */

// declare SystemSuspendPrivate
class SystemSuspendPrivate : public SystemSuspend,
#ifdef Q_WS_QWS
                             public QtopiaServerApplication::QWSEventFilter,
#endif
                             public QtopiaKeyboardFilter
{
Q_OBJECT
public:
    SystemSuspendPrivate(QObject *parent = 0);
    ~SystemSuspendPrivate();

    virtual bool suspendSystem();

    bool filter(int, int, int, bool, bool);
#ifdef Q_WS_QWS
    bool qwsEventFilter(QWSEvent *e);
#endif

private:
    bool handlersValid;
    QList<SystemSuspendHandler *> handlers;

    SystemSuspendHandler *waitingOn;

    bool inputEvent;

private slots:
    void operationCompleted();
};
QTOPIA_TASK(SystemSuspend, SystemSuspendPrivate);
QTOPIA_TASK_PROVIDES(SystemSuspend, SystemSuspend);

// define SystemSuspendPrivate
SystemSuspendPrivate::SystemSuspendPrivate(QObject *parent)
: SystemSuspend(parent), handlersValid(false), waitingOn(0)
{
    SuspendService *s = new SuspendService(this);
    QObject::connect(s, SIGNAL(doSuspend()), this, SLOT(suspendSystem()));

    QtopiaInputEvents::addKeyboardFilter(this);
#ifdef Q_WS_QWS
    QtopiaServerApplication::instance()->installQWSEventFilter(this);
#endif
}

SystemSuspendPrivate::~SystemSuspendPrivate()
{
#ifdef Q_WS_QWS
    QtopiaServerApplication::instance()->removeQWSEventFilter(this);
#endif
}

void SystemSuspendPrivate::operationCompleted()
{
    if(sender() == waitingOn) {
        qLog(PowerManagement) << " ...operationCompleted for" << waitingOn;
        waitingOn = 0;
    }
}

bool SystemSuspendPrivate::suspendSystem()
{
    if(!handlersValid) {
        handlers = qtopiaTasks<SystemSuspendHandler>();
        handlersValid = true;
        for(int ii = 0; ii < handlers.count(); ++ii) {
            QObject::connect(handlers.at(ii), SIGNAL(operationCompleted()),
                             this, SLOT(operationCompleted()));
        }
    }

    // Check can suspend
    for(int ii = handlers.count(); ii > 0; --ii) {
        qLog(PowerManagement) << handlers.at(ii - 1) << "checking whether canSuspend...";
        if(!handlers.at(ii - 1)->canSuspend()) {
            qLog(PowerManagement) << " ...refused, canceling suspend";
            emit systemSuspendCanceled();
            return false;
        }
    }

    // abort suspension if we see an input event
    inputEvent = false;

    // Do suspend
    emit systemSuspending();
    for(int ii = handlers.count(); ii > 0; --ii) {
        waitingOn = handlers.at(ii - 1);
        qLog(PowerManagement) << waitingOn << "suspending...";
        if(!waitingOn->suspend()) {
            qLog(PowerManagement) << " ...waiting for completion...";
            while(waitingOn) {
                QApplication::instance()->processEvents(QEventLoop::AllEvents |
                                                        QEventLoop::WaitForMoreEvents, 100);
                //qLog(PowerManagement) << "  ...processEvents done";
            }
            if (inputEvent) {
                qLog(PowerManagement) << "canceling suspend on inputevent";
                break;
            }
        } else {
            waitingOn = 0;
        }
    }

    // Do wakeup
    emit systemWaking();

    for(int ii = 0; ii < handlers.count(); ++ii) {
        waitingOn = handlers.at(ii);
        qLog(PowerManagement) << handlers.at(ii) << "waking up...";
        if(!waitingOn->wake()) {
            qLog(PowerManagement) << " ...waiting for completion...";
            while(waitingOn)
                QApplication::instance()->processEvents(QEventLoop::AllEvents |
                                                        QEventLoop::WaitForMoreEvents, 100);
        } else {
            waitingOn = 0;
        }
    }

    // Done
    emit systemActive();

    return true;
}

bool SystemSuspendPrivate::filter(int, int, int, bool, bool)
{
    inputEvent = true;
    return false;
}

#ifdef Q_WS_QWS
bool SystemSuspendPrivate::qwsEventFilter(QWSEvent *e)
{
    if (e->type == QWSEvent::Mouse)
        inputEvent = true;

    return false;
}
#endif

// define SuspendService
/*!
  \service SuspendService Suspend
    \inpublicgroup QtBaseModule
  \brief The SuspendService class provides the Suspend service.

  The \i Suspend service allows applications request that the device enter its
  suspend state.  The suspend state is likely to be a very low, but
  non-destructive, power state.
 */

/*!
  \internal
 */
SuspendService::SuspendService(QObject *parent)
: QtopiaAbstractService("Suspend", parent)
{
    publishAll();
}

/*!
  Enter the suspend state.

  This slot corresponds to the QCop service message \c {Suspend::suspend()}.
 */
void SuspendService::suspend()
{
    emit doSuspend();
}

/*!
  \fn void SuspendService::doSuspend()
  \internal
 */

#include "systemsuspend.moc"
