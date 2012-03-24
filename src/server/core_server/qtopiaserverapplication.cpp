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

#include "qtopiaserverapplication.h"

#include <QLabel>
#include <QTimer>
#include <QDesktopWidget>
#include <QtopiaFeatures>
#include <qvaluespace.h>
#include <qtopialog.h>
#include <QStringList>
#include <QSet>
#include <QString>
#include <QFile>
#include <QMap>
#include <QByteArray>
#include <QValueSpaceObject>
#include <qtopianamespace.h>
#include <qconstcstring.h>
#include <qtopialog.h>
#include <QPluginManager>
#include <ServerTaskPlugin>

#include <idletaskstartup.h>
#include <qtopiaservertasks_p.h>

#include <unistd.h>

Q_GLOBAL_STATIC(QtopiaServerTasksPrivate, qtopiaServerTasks);


/*!
  \class QtopiaServerApplication
    \inpublicgroup QtBaseModule
  \ingroup QtopiaServer
  \ingroup QtopiaServer::Task
  \brief The QtopiaServerApplication class provides additional QtopiaApplication
         functionality.

  \section1 Qt Extended Server Tasks

  The QtopiaServerApplication class acts as a QtopiaApplication instance in
  Qt Extended Server.  QtopiaServerApplication is primarily responsible for bringing
  up and shutting down the Qt Extended server and acts as the "core" controller in
  the system. This class is part of the Qt Extended server and cannot be used by
  other Qt Extended applications.

  The Qt Extended server is structured as a collection of largely independent
  \i tasks that are responsible for performing a small, well defined portion
  of work or functionality which often form the "backend" to other system
  capabilities.  For example, the network management APIs ultimately
  communicate with the QtopiaNetworkServer task, other tasks may operate more
  independently.  Tasks can be thought of as the building blocks that form the
  Qt Extended server, when arranged appropriately.

  Tasks are QObjects and may work together by exporting C++ interfaces.
  Other tasks or modules within the server may request tasks that support a
  particular interface.  For example, when the system is shutting down, all the
  tasks that provide the SystemShutdownHandler interface are invoked to perform
  all the necessary cleanup.

  The order in which the task objects are instantiated is configurable.  Both
  the \i configurator and the task developer has a degree of control over the
  instantiation order.  The \i configurator controls the start up order through
  the \c {$QPEDIR/etc/Tasks.cfg} file which has the following simple syntax:

  \code
  # Sample comment for TaskGroup1.  Comments may appear anywhere, as long as
  # the first character in the line is a '#'
  [TaskGroup1]
  TaskName1
  +TaskGroup2
  # Likewise, blank lines, as well as leading and trailing white space is
  # ignored

  [TaskGroup2]
  TaskName2
  TaskName3

  \endcode

  Task groups are free-form descriptors used to collect related classes.
  Task group names may only contain alphanumeric characters and must not
  \i {not} contain spaces.  A task name is the name given
  to the task when it is declared in code. It consists of alphanumeric characters and cannot contain
  spaces.  Groups may be nested hierarchically as shown in the example.  Nesting
  one group in another is \i {exactly} the same as pasting the body of the
  nested group into the parent group.  The above \c {Tasks.cfg} file is
  equivalent to:

  \code
  [TaskGroup1]
  TaskName1
  TaskName2
  TaskName3

  [TaskGroup2]
  TaskName2
  TaskName3
  \endcode

\section2 Reserved task Group Names
The following group names are reserved and have a special purpose:
\table
  \header \o Group Name \o Description
\row
    \o prestartup  \o  The \c {prestartup} contains tasks that will be started immediately after
                       Qt Extended is executed.
\row
    \o startup \o The \c {startup} group contains tasks to be launched at startup.
\row
    \o idle \o  The \c {idle} group contains tasks which will be launched after the UI is shown
                while the system is idle.
\row
    \o exclude \o The \c {exclude} group contains tasks that, while present in the
            server, will \c {never} be created.  Adding a task to the \c {exclude}
            group  is equivalent to removing it from the server.
\row
    \o All \o  The \c {All} group is a catch all tasks whose startup preference is not
            otherwise specified.
\endtable

How these groups interact will be covered shortly.


\section2 Supported Task Types

Tasks are constructed in one of two ways: preemptively or on-demand.
\table
\header \o Type \o Description
\row
    \o Preemptive Tasks \o  Preemptive tasks are those started by the system
        during startup, regardless of whether or not any other task has asked for it.
        Some preemptive tasks can have their construction delayed by being placed
        in the \i {idle} group.
\row
    \o On-demand Tasks \o On-demand tasks are those whose creation is deferred
        until another task requests it be started.  Tasks in the \i {startup}
        group in \c {Tasks.cfg} are the only tasks that are created preemptively.
        All other tasks (with the exception of those in the \i {exclude} group
        are started on demand.
\endtable

  While it is possible to instantiate an on-demand task by name, it is generally
  not advisable as doing so often creates unnecessary coupling within the
  system.  Instead, requesters ask the system to return a task that
  supports a given interface.  Doing so allows a particular implementation to
  be switched out without any code changes to the requestor.


  \section2 Marking Preemptive Tasks for Delayed Construction

  By default, preemptive tasks are created before the user interface is displayed.
  This means a task which is expensive to construct delays showing the user
  interface.
  To avoid this, tasks in the \c {Tasks.cfg} file can be marked as suitable for
  delayed construction by placing them in the \i {idle} group.

  Tasks in this group will be started shortly after the UI is displayed
  if the system is idle.  If the user interacts with the system, their construction
  may be further delayed.
  Tasks which provide background services without any user-visible components are
  suitable for this group.


  \section2 Marking Tasks to be Started on Demand

  Tasks in the \c {Tasks.cfg} file can be marked as demand started tasks by appending the
  \c {:demand} to their task name.  Similarly, group names can have \c {:demand}
  appended, which is equivalent to adding the designator to each of their
  containing task or sub-group names.  Demand tasks will never be picked by the
  \c {All} catch all task, but are otherwise subject to all ordering primatives.


  \section2 Tasks Startup Order

  The qtopiaTask() and qtopiaTasks() templates are used to request a task
  interface.  qtopiaTask() returns the first task that implements the interface
  and qtopiaTasks() returns all the tasks that implement the interface.  Each of
  these calls takes an optional boolean parameter to indicate whether the system
  should instantiate a task to satisfy the request (if needed) or only return
  tasks that have previously been instantiated.

  The order of tasks returned by qtopiaTask() and qtopiaTasks() is controllable
  through the order in which they appear in the \c {Tasks.cfg} file.  Consider
  the \c {Tasks.cfg} flattened so that it consists of a list of all groups in the order
  in which they appear.  Each task in this imaginary
  task list implements zero or more interfaces.  The order tasks will be
  returned when requested by interface is the same order as those tasks appear
  in this list, with duplicates removed.

  As a special "catch-all", primarily to prevent incorrect configuration, the special
  \c {All} task can be added to the \c {startup} or \c {idle} group.  This has the effect
  of inserting all tasks not otherwise assigned to a group or explicitly marked as demand
  started tasks.


  While the QtopiaServerApplication class itself is not strictly a task,
  it is instantiated during startup by the task system in the same way as other
  tasks under the special task name \c {QtopiaApplication}.  The
  \c {QtopiaApplication} task should be instantiated immediately after any
  environment setup or cleanup type tasks as many other tasks have an implicit
  reliance on its existence.

  The order that the system will try and start preemptive tasks and the order
  in which tasks will be given interface preference can be read from the value
  space immediately following the \c {QtopiaApplication} task executing.  The
  exact schema is:

  \table
  \header \o Item \o Type \o Description
  \row \o \c {/System/Tasks/<TaskName>/Order/Static} \o int \o The
    preemptive order that the system determined it would launch tasks.
    This may be different from the actual order if one task demand loads
    another that would otherwise be loaded later.
  \row \o \c {/System/Tasks/<TaskName>/Order/Launch} \o int \o The
    order that the tasks were actually launched in.  This key will only
    exist if the task is active.
  \row \o \c {/System/Tasks/<TaskName>/Order/Interface} \o int \o The
   order that the task will be given when determining interface
   associations.
  \row \o \c {/System/Tasks/<TaskName>/State} \o String \o "Disable"
   if the task was in the \c {exclude} list, "Active" if the task is
   running, or "Inactive" if not.
  \endtable
  
  A tutorial on how to develop new server tasks can be found in 
  the \l{Integration guide#Server Tasks}{Device Integration guide}.

  \section2 Server task plug-ins
  
  The above server tasks are linked into the Qt Extended server at
  build time. To increase the flexibility server tasks can be provided
  via a plug-in mechanism. This allows the addition of new tasks after
  the deployment of the server binary. Server task plugins must
  implement the ServerTaskPlugin interface in order to be recognized
  by the system.

  For more information on how to develop plug-in based server tasks
  refer to the \l {Tutorial: Writing server task plugin}{Server task
  plug-in tutorial}.

  \section1 Qt Extended Server Widgets

  There are many cases of Qt widgets being used throughout the Qt Extended server
  that may need to be customized to achieve a desired look and feel.  For
  example, while it supports customization through theming, a customer that
  wants to replace the Qt Extended phone dialer with a "rotary dial" style dialer
  would need to replace the entire dialer widget.

  To simplify the task and minimize the code changes needed to replace visual
  components of the Qt Extended Server, the concept of Qt Extended Server Widgets exists.
  Qt Extended Server Widgets splits the definition of a visual component - or server
  widget - into two parts: the server widget interface (hereafter referred to as
  the AbstractWidget) and the concrete server widget implementation
  (ConcreteWidget).  While not technically necessary, the AbstractWidget is
  generally an abstract interface that derives directly from QWidget.

  Rather than using the regular \c {new ClassName(parent, flags)} syntax for
  instantiating a widget, the special qtopiaWidget() template method,
  parameterized on AbstractWidget, is provided.  This method uses the server
  widgets replacement system to look up the ConcreteWidget instance type it
  should return.

  Developers use the  QTOPIA_REPLACE_WIDGET(),  QTOPIA_REPLACE_WIDGET_WHEN() and
   QTOPIA_REPLACE_WIDGET_OVERRIDE() macros to provide ConcreteWidget for a
  particular AbstractWidget.  The server widget replacement system then resolves
  which ConcreteWidget to return by executing the following set of rules in
  order until a ConcreteWidget is determined.

  \table
  \header \o Rule \o Description
  \row \o Explicitly specified \o The \c {Trolltech/ServerWidgets} configuration
  file is read to determine if the ConcreteWidget to use has been explicitly
  specified.  The configuration file contains one group, \c {Mapping} that
  contains mappings between \i {AbstractName} and \i {ConcreteName}.  How these
  two names are determined is discussed below.  For example,

  \code
  [Mapping]
  BrowserScreen=Wheel
  DialerScreen=Rotary
  \endcode

  Unless the specified widget does not exist, the mapping is always honored.
  That is, using  QTOPIA_REPLACE_WIDGET_WHEN() can not override explicitly
  specified widgets.  The special name \c None will disable the widget, causing
  qtopiaWidget() to always return null.

  \row \o Primary Default \o The primary default is the value of the
  \c {Mapping/Default} key, or \c Default if not specified. The primary default
  name is tried as the \i {ConcreteName}.  Feature dependencies are honored.
  \row \o Other Defaults \o All \i {ConcreteName}'s beginning with the primary
  default name are tried.  Feature dependencies are honored.
  \row \o All Replacements \o All available ConcreteWidgets are tried.  Feature
  dependencies are honored.
  \endtable

  The \i {AbstractName} and \i {ConcreteName} names used to identify a server
  widget are derived from the widget class names.  \i {AbstractName} is
  set to the AbstractWidget's full name, unless the name begins with
  \c {QAbstract} in which case it is set to the widget class name, minus the
  \c {QAbstract} prefix.  The \i {ConcreteName} is set to the class name of
  the ConcreteWidget, unless the class name ends with the \i {AbstractName} of
  the widget it is replacing, in which case it is set to the widget class name
  minus the \i {AbstractName} suffix.  These rules are designed to simplify
  writing mapping files - a class \c WheelBrowserScreen replacing
  \c QAbstractBrowserScreen is written as \c {BrowserScreen=Wheel} rather than
  \c {QAbstractBrowserScreen=WheelBrowserScreen}.

  The following image also demonstrates the widget selection process:

  \image WidgetSelectionRules.png "Selecting the correct Server Widget"

  \section2 Singleton pattern

    Usually each call to qtopiaWidget() returns a new server widget instance. In some cases this
    behaviour is not desired. If it is necessary to return a reference to an already existing instance
    of a server widget Qt's meta system class info should be used to mark an
    abstract server widget as singleton. This means that every concrete implementation of
    this abstract widget will follow the singleton pattern. The \c SingletonServerWidget string is reserved
    to enable this feature. If the class info tag is missing Qt Extended assumes that the class
    does not follow the singleton pattern.

    For example:
    \code
    //qabstracthomescreen.h
    class QAbstractHomeScreen : public QWidget
    {
       QOBJECT
       Q_CLASSINFO("SingletonServerWidget", "true");
       // ...
    };
    \endcode

    The following code segment demonstrates the difference:

    \code
    QAbstractHomeScreen* ref1 = qtopiaWidget<QAbstractHomeScreen>();
    QAbstractHomeScreen* ref2 = qtopiaWidget<QAbstractHomeScreen>();

    if ( ref1 == ref2 ) {
        ...
        //The SingletonServerWidget class info is set to true for
        //QAbstractHomeScreen (as seen in above example).
    } else {
        ...
        //The SingletonServerWidget class info is set to false
        //or not defined at all.";
    }
    \endcode

    A tutorial on how to develop new server widgets can be found in the QAbstractServerInterface
    class documentation and the \l{integration-guide.html#server-widgets}{Device Integration guide}.
 */

/*!
  \macro QTOPIA_TASK(TaskName, Object)
  \relates QtopiaServerApplication

  Mark the \a Object as task \a TaskName.  Only QObject derived types may be
  tasks.

  As the  QTOPIA_TASK() macro defines symbols, it should appear only in the
  implementation file of a task, and not in the header file.
 */

 /*!
  \macro QTOPIA_DEMAND_TASK(TaskName, Object)
  \relates QtopiaServerApplication

  Mark the \a Object as task \a TaskName.  Only QObject derived types may be
  tasks.   QTOPIA_DEMAND_TASK() differs from  QTOPIA_TASK() in that tasks
  installed using this macro are automatically marked as "demand" tasks, unless
  specifically overridden in the \c {Tasks.cfg} file.  That is, tasks installed
  like this will only be instantiated on request, not during server startup.

  As the QTOPIA_DEMAND_TASK() macro defines symbols, it should appear only in
  the implementation file of a task, and not in the header file.
 */

 /*!
  \macro QTOPIA_STATIC_TASK(TaskName, Function)
  \relates QtopiaServerApplication

  Install a functional task.  A functional task is one that does not consist of
  an instantiable object, but is, instead, a simple function.  Static tasks can
  obviously not provide interfaces.

  \a TaskName should be set to the name of the task, and \a Function to the
  static function to call to run the task.

  As the QTOPIA_STATIC_TASK() macro defines symbols, it should appear only in
  the implementation file of a task, and not in the header file.
 */


/*!
  \macro QTOPIA_TASK_QINTERFACE(InterfaceName)
  \relates QtopiaServerApplication

  Mark the specified \a InterfaceName as a task interface.  Any
  class interface that is required to support the qtopiaTask<>() or
  qtopiaTasks<>() request mechanism must be marked as a task interface.

  The QTOPIA_TASK_!INTERFACE() macro must be used in a header file, immediately
  following the interface's declaration.

  \a InterfaceName should be a Qt Q_INTERFACE style interface.  If you want to
  use a QObject derived type as an interface, use the QTOPIA_TASK_INTERFACE()
  macro instead.
 */

/*!
  \macro QTOPIA_TASK_INTERFACE(ClassName)
  \relates QtopiaServerApplication

  Mark the specified QObject-derived \a ClassName as a task interface.  Any
  class interface that is required to support the qtopiaTask<>() or
  qtopiaTasks<>() request mechanism must be marked as a task interface.

  The QTOPIA_TASK_INTERFACE() macro must be used in a header file, immediately
  following the interface's declaration.

  If the interface is not a QObject type, but rather a Qt Q_INTERFACE() style
  interface, use the QTOPIA_TASK_QINTERFACE() macro instead.

 */

/*!
  \macro QTOPIA_TASK_PROVIDES(TaskName, Interface)
  \relates QtopiaServerApplication

  Indicate that the task \a TaskName provides \a Interface, as previously
  declared by QTOPIA_TASK_INTERFACE.  A task can provide more than one
  interface.

  For a task to support the interface, it must either inherit directly from the
  interface or aggregate itself with an object that does during its
  construction.   The QtopiaServerApplication::addAggregateObject() method can
  be used to aggregate a task with another object.
 */

/*!
  \fn T *qtopiaTask(bool onlyActive = false)
  \relates QtopiaServerApplication

  Return a task instance that supports the T interface.  If \a onlyActive is
  true the system will only select from tasks that are already active.
  Otherwise, if needed, the system will instantiate tasks to satisfy the
  request.
 */

/*!
  \fn QList<T *> qtopiaTasks(bool onlyActive = false)
  \relates QtopiaServerApplication

  Returns all task instances that support the T interface.  The instances are in
  the intended instantiation order.  If \a onlyActive is true the system will
  only select from tasks that are already active.  Otherwise, if needed, the
  system will instantiate tasks to satisfy the request.
 */

/*!
    \enum QtopiaServerApplication::ShutdownType

    \value NoShutdown No shutdown has been requested.
    \value RebootSystem The server terminates and initiates the reboot of the
                        system.
    \value RestartDesktop The server terminates and requests its restart by the
                          calling environment.
    \value ShutdownSystem The server terminates and initiates the shutdown of
                          the system.
    \value TerminateDesktop The server terminates only.
*/

/*!
    \enum QtopiaServerApplication::StartupType

    This enum determines how tasks are started when QtopiaServerApplication::startup()
    is called.

    \value ImmediateStartup Requested tasks are started immediately.  The call to startup()
                            will block until all requested tasks have been started.
    \value IdleStartup Requested tasks are queued to be started when the server is idle.
                       The call to startup() is non-blocking.
*/

/*!
  \fn QtopiaServerApplication::shutdownRequested()

  Emitted whenever the user or an application requests that the system shutdown.
  A system shutdown is requested by sending a \c {shutdown()} message to the
  \c {QPE/System} QCop channel.

  Generally a UI will be connected to the shutdownRequested() signal to ask
  the user what they want to do.  This UI should then invoke the shutdown()
  method to perform the appropriate action.
*/

/*!
  \macro QTOPIA_REPLACE_WIDGET(AbstractWidget, ConcreteWidget)
  \relates QtopiaServerApplication

  Mark \a ConcreteWidget as a replacement for \a AbstractWidget.  While
  \a AbstractWidget does not actually have to be abstract, it is generally
  an interface that \a ConcreteWidget implements.  This macro should appear as
  part of the definition of \a ConcreteWidget.

  For example,
  \code
  // wheelbrowserscreen.h
  class WheelBrowserScreen : public QAbstractBrowserScreen
  {
      // ...
  };

  // wheelbrowserscreen.cpp
  QTOPIA_REPLACE_WIDGET(QAbstractBrowserScreen, WheelBrowserScreen);
  \endcode
*/

 /*!
  \macro QTOPIA_REPLACE_WIDGET_WHEN(AbstractWidget, ConcreteWidget, Feature)
  \relates QtopiaServerApplication

  Mark \a ConcreteWidget as a replacement for \a AbstractWidget.  While
  \a AbstractWidget does not actually have to be abstract, it is generally
  an interface that \a ConcreteWidget implements.  This macro should appear as
  part of the definition of \a ConcreteWidget.

  Unless explicitly specified in the \c {Trolltech/ServerWidgets} file, a
  widget specified in this fashion if \a Feature is currently provided by
  Qtopia, as returned by the QtopiaFeatures::hasFeature() method.

  For example,
  \code
  // touchscreendialer.h
  class TouchscreenDialerScreen : public QAbstractDialerScreen
  {
      // ...
  };

  // touchscreendialer.cpp
  QTOPIA_REPLACE_WIDGET_WHEN(QAbstractDialerScreen, TouchscreenDialerScreen, Touchscreen);
  \endcode
*/

/*!
  \macro QTOPIA_REPLACE_WIDGET_OVERRIDE(AbstractWidget, ConcreteWidget)
  \relates QtopiaServerApplication

  Set \a ConcreteWidget as the \bold {static} replacement for \a AbstractWidget.

  While as efficient as possible, use of the widget replacement system instead
  of explicitly instantiating concrete classes does introduce some indirection
  costs.  For shipping software where this flexibility is not necessary,
  QTOPIA_REPLACE_WIDGET_OVERRIDE() can be used to force the selection of a
  specified \a ConcreteWidget.  This eliminates the cost of the widget
  replacement system for the particular \a AbstractWidget by bypassing the
  replacement system.  The macro should appear inline with the \a AbstractWidget
  declaration, and must have visibility of the \a ConcreteWidget's declaration.


  For example,
  \code
  // qabstractbrowserscreen.h
  class QAbstractBrowserScreen : public QWidget
  {
      // ...
  };

  #include "wheelbrowserscreen.h"
  QTOPIA_REPLACE_WIDGET_OVERRIDE(QAbstractBrowserScreen, WheelBrowserScreen);
  \endcode
  
  Note that any server widget that uses this marco will not work with 
  singleton server widgets. Calling qtopiaWidget() on such widgets will always return 
  a new instance.
*/

/*!
  \fn T *qtopiaWidget(QWidget *parent = 0, Qt::WFlags flags = 0)
  \relates QtopiaServerApplication

  Returns a concrete implementation of T, with the specified \a parent and
  \a flags.  Concrete implementations are provided with the
  QTOPIA_REPLACE_WIDGET(), QTOPIA_REPLACE_WIDGET_WHEN() and
  QTOPIA_REPLACE_WIDGET_OVERRIDE() macros and selected as described in the
  \l {Qt Extended Server Widgets} overview.

  Each call to this function returns a new instance of the requested server widget unless
  the server widget has been marked as singleton widget via

  \code
   Q_CLASSINFO("SingletonServerWidget", "true");
  \endcode
*/

static QValueSpaceItem* serverWidgetVsi = 0;

// define QtopiaServerApplication
QtopiaServerApplication *QtopiaServerApplication::m_instance = 0;

/*!  \internal */
QtopiaServerApplication::QtopiaServerApplication(int& argc, char **argv)
: QtopiaApplication(argc, argv, QtopiaApplication::GuiServer)
{
    Q_ASSERT(!m_instance);
    m_instance = this;
    QValueSpace::initValuespaceManager();
    qtopiaServerTasks()->enableTaskReporting();

    serverWidget_vso = new QValueSpaceObject("/System/ServerWidgets/Initialized", this);
    serverWidget_vso->setAttribute( QByteArray(), false );
    serverWidgetVsi = new QValueSpaceItem( "/System/ServerWidgets" );
    connect( serverWidgetVsi, SIGNAL(contentsChanged()),
             this, SLOT(serverWidgetVsChanged()) );

    QtopiaServerApplication::excludeFromTaskCleanup(this, true);

#ifdef Q_WS_QWS
    // Check that the display size is specified
    QString dispSpec = getenv("QWS_DISPLAY");
    bool dispArg = false;
    for (int i = 0; i < argc; ++i) {
        QString arg(argv[i]);
        if (dispArg) {
            dispSpec = arg;
            break;
        }
        if (arg == QLatin1String("-display")) {
            dispArg = true;
            continue;
        }
    }
    if (!dispSpec.contains(QLatin1String("mmheight"), Qt::CaseInsensitive))
        qWarning() << "Warning: Display size not set.  Using default DPI";
#endif
}

/*! \internal */
QtopiaServerApplication::~QtopiaServerApplication()
{
    Q_ASSERT(m_instance);
    m_instance = 0;
}

#ifdef Q_WS_QWS

/*!
  \class QtopiaServerApplication::QWSEventFilter
    \inpublicgroup QtBaseModule
  \ingroup QtopiaServer
  \brief The QWSEventFilter class provides an interface for filtering Qt Window System events.
  
  This class is part of the Qt Extended server and cannot be used by other Qt Extended applications.
 */

/*!
  \fn QtopiaServerApplication::QWSEventFilter::~QWSEventFilter()
  \internal
 */

/*!
  \fn bool QtopiaServerApplication::QWSEventFilter::qwsEventFilter(QWSEvent *event)

  Called when a QWS \a event is received.  Return true to filter the event, or
  false to allow the event to continue propagation.
  */

/*!
  Install the \a filter for QWS events.  Installing an event filter is
  equivalent to deriving from QtopiaApplication directly and overriding the
  QtopiaApplication::qwsEventFilter() method.

  Multiple QWS event filters may be installed simultaneously.  In this case,
  each event filter is queried sequentially in the order it was installed.  If
  any filter filters the event, subsequent filters will not be called.
 */
void QtopiaServerApplication::installQWSEventFilter(QWSEventFilter *filter)
{
    Q_ASSERT(filter);
    m_filters.append(filter);
}

/*!
  Remove all instances of \a filter from the list of event filters.  This method
  should be called when an event filter is destroyed.  No automatic cleanup is
  performed.
 */
void QtopiaServerApplication::removeQWSEventFilter(QWSEventFilter *filter)
{
    Q_ASSERT(filter);
    QList<QWSEventFilter *>::Iterator iter = m_filters.begin();
    while(iter != m_filters.end()) {
        if(*iter == filter) {
            iter = m_filters.erase(iter);
        } else {
            ++iter;
        }
    }
}

#endif // Q_WS_QWS

/*!
  Return the instantiated QtopiaServerApplication instance.  An instance of
  the class \bold {must} have been constructed prior to calling this method.
 */
QtopiaServerApplication *QtopiaServerApplication::instance()
{
    Q_ASSERT(m_instance);
    return m_instance;
}

#ifdef Q_WS_QWS

/*!
  \internal
 */
bool QtopiaServerApplication::qwsEventFilter(QWSEvent *e)
{
    for(int ii = 0; ii < m_filters.count(); ++ii)
        if(m_filters.at(ii)->qwsEventFilter(e)) {
            return true;
        }

    return QtopiaApplication::qwsEventFilter(e);
}

/*!
  \reimp
  */
bool QtopiaServerApplication::notify( QObject* o, QEvent* e )
{
    QtopiaApplication::notify(o, e);
    static bool mainWidgetInitPending = true; 
    if ( mainWidgetInitPending ) {
        if ( !mainWidgetName.isEmpty() && e->type() == QEvent::Show && o->isWidgetType() ) {
            if ( mainWidgetName == o->metaObject()->className() ) {
                qLog(UI) << "Idle screen widget " << mainWidgetName << " initialized";
                mainWidgetInitPending = false;
            } else if ( QWidget* focus = qobject_cast<QWidget*>(o)) {
                QWidget* w = focus;
                while ( (w=w->nextInFocusChain()) != focus )
                    if ( w->isVisible() && w->metaObject()->className() == mainWidgetName ) {
                        qLog(UI) << "Idle screen widget " << mainWidgetName << " initialized";
                        mainWidgetInitPending = false;
                        break;
                    }
            }

            if ( !mainWidgetInitPending ) {
                //notify interested parties that the main idle screen is up and visible 
                serverWidget_vso->setAttribute(QByteArray(), true );
                if ( serverWidgetVsi ) {
                    serverWidgetVsi->disconnect( this );
                    serverWidgetVsi->deleteLater();
                    serverWidgetVsi = 0;
                }
            }
        }
    }
    return false;
}

#endif

/*! \internal */
void QtopiaServerApplication::shutdown()
{
    emit shutdownRequested();
}

/*!
  Initiates a system shutdown of the specified \a type.
 */
void QtopiaServerApplication::shutdown( QtopiaServerApplication::ShutdownType type)
{
    switch ( type ) {
        case QtopiaServerApplication::ShutdownSystem:
            qLog(Support) << "QtopiaServerApplication::ShutdownSystem";
            _shutdown(type);
            execlp("shutdown", "shutdown", "-h", "now", (void*)0); // No tr
            break;

        case QtopiaServerApplication::RebootSystem:
            qLog(Support) << "QtopiaServerApplication::RebootSystem";
            _shutdown(type);
            execlp("shutdown", "shutdown", "-r", "now", (void*)0); // No tr
            break;

        case QtopiaServerApplication::RestartDesktop:
            qLog(Support) << "QtopiaServerApplication::RestartDesktop";
            _shutdown(type);
            break;

        case QtopiaServerApplication::TerminateDesktop:
            qLog(Support) << "QtopiaServerApplication::TerminateDesktop";
            _shutdown(type);
            break;

        default:
            break;
    }
}

/*!
  \internal
  Called whenever a system restart is requested.
 */
void QtopiaServerApplication::restart()
{
    shutdown(RestartDesktop);
}

QTOPIA_TASK(QtopiaApplication,
            QtopiaServerApplication(QtopiaServerApplication::argc(),
                                    QtopiaServerApplication::argv()));

/*!
  \internal
  */
void QtopiaServerApplication::serverWidgetVsChanged()
{
    if ( !serverWidgetVsi )
        return;

    QByteArray idleScreen = serverWidgetVsi->value("QAbstractHomeScreen", "").toByteArray();
    if ( !idleScreen.isEmpty() ) {
        qLog(UI) << "Using" << idleScreen << "as idle widget";
        mainWidgetName = idleScreen;
        serverWidgetVsi->disconnect( this );
        serverWidgetVsi->deleteLater();
        serverWidgetVsi = 0;
    } else if ( mainWidgetName.isEmpty() ){
        //fall back -> the server interace must always exist
        mainWidgetName = serverWidgetVsi->value("QAbstractServerInterface", "").toByteArray();
        if ( qLogEnabled(UI) && !mainWidgetName.isEmpty() )
            qLog(UI) << "Using" << mainWidgetName << "as idle widget";
    }
}

/*!
  \typedef QtopiaServerApplication::CreateTaskFunc
  \internal
  */


// define QtopiaServerTasksPrivate
QString QtopiaServerTasksPrivate::taskConfigFile()
{
    return Qtopia::qtopiaDir() + "/etc/Tasks.cfg";
}

/*!
  Returns the task system's configuration file.
 */
QString QtopiaServerApplication::taskConfigFile()
{
    return qtopiaServerTasks()->taskConfigFile();
}

void QtopiaServerTasksPrivate::enableTaskReporting()
{
    Q_ASSERT(!taskList);
    taskList = new QValueSpaceObject("/System/Tasks/");

    // The story so far...
    for(QMap<QConstCString, Task *>::ConstIterator iter = m_availableTasks.begin();
            iter != m_availableTasks.end();
            ++iter) {

        Task *t = iter.value();
        QByteArray name = t->name.toByteArray();
        taskList->setAttribute(name + "/Order/Static", t->staticOrder);
        if(t->excluded) {
            taskList->setAttribute(name + "/State", "Disabled");
        } else {
            taskList->setAttribute(name + "/Order/Interface", t->interfaceOrder);
            if(t->launchOrder > 0) {
                taskList->setAttribute(name + "/State", "Active");
                taskList->setAttribute(name + "/Order/Launch",
                        t->launchOrder);
            } else {
                taskList->setAttribute(name + "/State", "Inactive");
            }
        }

    }
}

void QtopiaServerTasksPrivate::setupInterfaceList(const QList<Task *> &startupOrder)
{
    unsigned int interfaceOrder = 1;
    for(int ii = 0; ii < startupOrder.count(); ++ii) {
        Task * t = startupOrder.at(ii);
        t->interfaceOrder = interfaceOrder++;
        qLog(QtopiaServer) << "Interface task" << t->interfaceOrder
                               << "-" << t->name.data();
        for(int jj = 0; jj < t->interfaces.count(); ++jj) {
            qLog(QtopiaServer) << t->interfaces.at(jj).toByteArray() << "provided by" << t->name.toByteArray();
            m_availableInterfaces[t->interfaces.at(jj)].append(t);
        }
    }

    for(QMap<QConstCString, Task *>::ConstIterator iter =
            m_availableTasks.begin(); iter != m_availableTasks.end(); ++iter) {
        Task * t = *iter;
        if(!t->excluded && !t->interfaceOrder) {
            t->interfaceOrder = interfaceOrder++;
            qLog(QtopiaServer) << "Interface task" << t->interfaceOrder
                               << "-" << t->name.data();
            for(int jj = 0; jj < t->interfaces.count(); ++jj) {
                qLog(QtopiaServer) << t->interfaces.at(jj).toByteArray() << "provided by" << t->name.toByteArray();
                m_availableInterfaces[t->interfaces.at(jj)].append(t);
            }
        }
    }
}

struct TaskLine {
    QByteArray name;
    bool demand;
    bool subGroup;
};

struct TaskStartupInfo {
    QList<QByteArray> startupTasks;
    int allStartupPos;
    QList<QByteArray> orderedTasks;

    QSet<QByteArray> seenTasks;
    QSet<QByteArray> allExcludes;

    QMap<QByteArray, QList<TaskLine> > groups;
};



void QtopiaServerTasksPrivate::determineStartupOrder(const QByteArray &group,
                                                     bool demand, bool startup,
                                                     TaskStartupInfo &info)
{
    QMap<QByteArray, QList<TaskLine> >::Iterator iter =
        info.groups.find(group);
    if(iter == info.groups.end())
        return;

    for(int ii = 0; ii < iter->count(); ++ii) {
        const TaskLine &line = iter->at(ii);
        bool isDemand = demand || line.demand;

        if(line.subGroup) {

            determineStartupOrder(line.name, demand, startup, info);

        } else if("All" == line.name) {

            if(startup && -1 == info.allStartupPos ) {
                info.allStartupPos = info.startupTasks.count();
            }

        } else if(!info.seenTasks.contains(line.name)) {
            if(!isDemand && startup) {
                info.startupTasks.append(line.name);
                info.allExcludes.insert(line.name);
            }

            if(isDemand)
                info.allExcludes.insert(line.name);

            if(!startup) {
                info.orderedTasks.append(line.name);
            }

            info.seenTasks.insert(line.name);
        }
    }
}

bool QtopiaServerTasksPrivate::determineStartupOrder(const QList<QByteArray> &startupGroups, QList<Task *> &startupOrder)
{
    // Parse config file
    QFile tasks(taskConfigFile());
    if(!tasks.open(QFile::ReadOnly)) {
        qLog(QtopiaServer) << "ERROR Cannot open task configuration.";
        return false;
    }
    int lineNo = 0;

    QMap<QByteArray, QList<TaskLine> > groups;
    QList<QByteArray> groupOrder;
    QByteArray group;
    bool demandGroup = false;
    QMap<QByteArray, QList<TaskLine> >::Iterator currentGroup = groups.end();

    while(!tasks.atEnd()) {

        QByteArray line = tasks.readLine(1024).trimmed();
        ++lineNo;
        if(line.startsWith('#') || line.isEmpty())
            continue;

        if(line.startsWith('[') && line.endsWith(']')) {
            group = line.mid(1, line.length() - 2);
            if(group.endsWith(":demand")) {
                group = group.left(group.length() - 7/* ::strlen(":demand") */);
                demandGroup = true;
            } else {
                demandGroup = false;
            }

            currentGroup = groups.find(group);
            if(currentGroup == groups.end()) {
                currentGroup = groups.insert(group, QList<TaskLine>());
                groupOrder.append(group);
            }
            continue;
        }

        if(currentGroup == groups.end()) {
            qLog(QtopiaServer) << "ERROR Unable to parse task line" << lineNo << "(Task outside group)";
            return false;
        }

        bool subGroup = false;
        bool demandTask = false;
        QByteArray taskName = line;
        if(taskName.startsWith('+')) {
            subGroup = true;
            taskName = taskName.mid(1);
        }
        if(taskName.endsWith(":demand")) {
            demandTask = true;
            taskName =
                taskName.left(taskName.length() - 7/* ::strlen(":demand") */);
        }
        if(demandGroup) {
            demandTask = true;
        }

        TaskLine tl = {taskName, demandTask, subGroup};
        currentGroup->append(tl);
    }

    tasks.close();

    TaskStartupInfo startupInfo;
    startupInfo.groups = groups;
    startupInfo.allStartupPos = -1;

    // Setup excludes
    currentGroup = groups.find("exclude");
    if(currentGroup != groups.end()) {
        for(int ii = 0; ii < currentGroup->count(); ++ii) {
            QConstCString name(currentGroup->at(ii).name.constData(),
                               currentGroup->at(ii).name.length());
            QMap<QConstCString, Task *>::ConstIterator titer =
                m_availableTasks.find(name);
            if(titer != m_availableTasks.end()) {
                (*titer)->excluded = true;
                startupInfo.seenTasks.insert(currentGroup->at(ii).name);
                startupInfo.allExcludes.insert(currentGroup->at(ii).name);
            }
        }
    }

    // Setup orders
    for (int ii = 0; ii < startupGroups.count(); ++ii)
        determineStartupOrder(startupGroups.at(ii), false, true, startupInfo);
    startupInfo.seenTasks.clear();

    for(int ii = 0; ii < groupOrder.count(); ++ii)
        if("exclude" != groupOrder.at(ii))
            determineStartupOrder(groupOrder.at(ii), false, false, startupInfo);

    // Create the "All" list if requested
    if(-1 != startupInfo.allStartupPos) {
        QList<QByteArray> allList;
        for(QMap<QConstCString, Task *>::ConstIterator iter =
                m_availableTasks.begin();
                iter != m_availableTasks.end();
                ++iter) {

            QByteArray name = iter.key().toByteArray();
            if(!startupInfo.allExcludes.contains(name) &&
               !iter.value()->demand) {
                allList.append(name);
            }
        }

        // Insert to all list
        for(int ii = 0; ii < allList.count(); ++ii) {
            startupInfo.startupTasks.insert(startupInfo.allStartupPos,
                                            allList.at(ii));
        }
    }

    if (m_availableInterfaces.isEmpty()) {
        // Transform ordered list into actual list
        QList<Task *> interfaceOrder;
        for(int ii = 0; ii < startupInfo.orderedTasks.count(); ++ii) {
            QConstCString name(startupInfo.orderedTasks.at(ii).constData(),
                               startupInfo.orderedTasks.at(ii).length());

            QMap<QConstCString, Task *>::ConstIterator iter =
                m_availableTasks.find(name);

            if(iter != m_availableTasks.end()) {
                interfaceOrder.append(*iter);
            }
        }

        setupInterfaceList(interfaceOrder);
    }

    // Transform the startup order into a return list
    for(int ii = 0; ii < startupInfo.startupTasks.count(); ++ii) {
        QConstCString name(startupInfo.startupTasks.at(ii).constData(),
                           startupInfo.startupTasks.at(ii).length());

        QMap<QConstCString, Task *>::ConstIterator iter =
            m_availableTasks.find(name);
        if(iter != m_availableTasks.end()) {
            startupOrder.append(*iter);
            iter.value()->staticOrder = startupOrder.count();
            qLog(QtopiaServer) << "Ordered task" << iter.value()->staticOrder
                               << "-" << startupInfo.startupTasks.at(ii);
        }
    }

    return true;
}

QObject* QtopiaServerTasksPrivate::taskForInterface(Task *task, const QConstCString &iface, bool onlyActive)
{
    QObject *obj = startTask(task, onlyActive);
    if(!obj) return 0;

    if(obj->inherits(iface.data())) return obj;

    for(int ii = 0; ii < task->aggregates.count(); ++ii)
        if(task->aggregates.at(ii)->inherits(iface.data()))
            return task->aggregates.at(ii);

    return 0;
}

QObject* QtopiaServerTasksPrivate::startTask(Task *task, bool onlyActive)
{
    if(task->launchOrder > 0)
        return task->object;

    if(task->excluded)
        return 0;

    if(onlyActive)
        return 0;

    if(task->create || task->plugin) {
        bool wasConstructing = m_constructingTask;
        m_constructingTask = true;
        if ( task->create )
            task->object = task->create(task->createArg);
        else
            task->object = task->plugin->initTask(task->createArg);
        if(task->object) {
            QMap<QObject *, QList<QObject *> >::Iterator iter =
                m_constructingAggregates.find(task->object);
            if(iter != m_constructingAggregates.end()) {
                task->aggregates += *iter;
                m_constructingAggregates.erase(iter);
            }
        }
        m_constructingTask = wasConstructing;
        Q_ASSERT(m_constructingTask || m_constructingAggregates.isEmpty());
    }

    if(task->object) {
        m_activeTasks.insert(task->object, task);
        QPointer<QObject> pointer = task->object;
        m_startedTaskObjects.append(pointer);
    }

    if(!application && QConstCString("QtopiaApplication", 17) == task->name)
        application = qobject_cast<QtopiaApplication *>(task->object);

    static unsigned int tasksStarted = 0;
    ++tasksStarted;
    task->launchOrder = tasksStarted;
    if(taskList) {
        QByteArray name = task->name.toByteArray();
        taskList->setAttribute(name + "/State", "Active");
        taskList->setAttribute(name + "/Order/Launch", task->launchOrder);
    }

    return task->object;
}

void QtopiaServerTasksPrivate::cleanupTasks()
{
    //clean tasks in reverse startup order
    while (m_startedTaskObjects.count()) {
        QObject* o = m_startedTaskObjects.takeLast();
        if (!o) continue;
        if (!m_excludedManagement.contains(o)) {
            qLog(QtopiaServer) << "Deleting task" << o->metaObject()->className();
            o->deleteLater();
        } else {
            qLog(QtopiaServer) << o->metaObject()->className() << "excluded from task cleanup";
        }
    }
}

void QtopiaServerTasksPrivate::setShutdownObjects(const QList<SystemShutdownHandler *> &objs)
{
    Q_ASSERT(m_shutdownObjects.isEmpty());
    m_shutdownObjects = objs;
}

void QtopiaServerTasksPrivate::ackShutdownObject(SystemShutdownHandler *obj)
{
    m_shutdownObjects.removeAll(obj);
}

void QtopiaServerTasksPrivate::shutdownProceed()
{
    ackShutdownObject(static_cast<SystemShutdownHandler *>(sender()));
}


/*!
  Returns the path to an object in the value space that the task \a taskName can
  use to store status information.  Will return a null byte array if the value
  space has not been initialized.  This method is for task implementors only.
 */
QByteArray QtopiaServerApplication::taskValueSpaceObject(const QByteArray &taskName)
{
    QtopiaServerTasksPrivate *qst = qtopiaServerTasks();
    if(!qst || !qst->taskList) return QByteArray();
    return QByteArray("/System/Tasks/") + taskName;
}

/*!
  Sets a value space \a attribute for task \a taskName to \a value.  This is
  a convenience method to simplify the usage of the value space for a task.
  For more complex usage, tasks should use the taskValueSpaceObject() to request
  a path, and manually create a QValueSpaceObject for their own use.
  Returns true upon success; otherwise returns false.

  The value remains in the value space until the task is unloaded.
  */
bool QtopiaServerApplication::taskValueSpaceSetAttribute(const QByteArray &taskName,
                                                         const QByteArray &attribute,
                                                         const QVariant &value)
{
    QtopiaServerTasksPrivate *qst = qtopiaServerTasks();
    if(!qst || !qst->taskList) return false;
    qst->taskList->setAttribute(taskName + "/" + attribute, value);
    return true;
}

/*!
  Returns the argc reference passed to startup().
 */
int &QtopiaServerApplication::argc()
{
    Q_ASSERT(qtopiaServerTasks()->argc);
    return *qtopiaServerTasks()->argc;
}

/*!
  Returns the argv reference passed to startup().
 */
char **QtopiaServerApplication::argv()
{
    Q_ASSERT(qtopiaServerTasks()->argc);
    return qtopiaServerTasks()->argv;
}

/*!
    During the server shutdown all object based server tasks are deleted. However some tasks may not require
    such management as they install themselves into some form of backend functionality
    which may take care of the tasks life time. Setting \a exclude to true excludes \a task
    from the task system's memory management.

    An example is the QtopiaPowerManager and its sub class which installs itself as the
    screensaver. The QWS server owns the screen saver object and therefore takes already care
    of the objects life time.
*/
void QtopiaServerApplication::excludeFromTaskCleanup(QObject* task, bool exclude)
{
    QtopiaServerTasksPrivate *qst = qtopiaServerTasks();
    if (!qst) return;

    if (exclude)
        qst->m_excludedManagement.insert(task);
    else
        qst->m_excludedManagement.remove(task);
}

/*!
  Launches server tasks.  This method is used to start the Qt Extended server and
  should never be called from other code.  Returns true on success,
  false on failure.

  \a argc and \a argv should be the values passed to the main() function.
  \a startupGroups contains a list of Task groups to start.
  \a type determines the behavior for launching the tasks.
 */
bool QtopiaServerApplication::startup(int &argc, char **argv, const QList<QByteArray> &startupGroups,
    QtopiaServerApplication::StartupType type)
{
    QtopiaServerTasksPrivate *qst = qtopiaServerTasks();
    Q_ASSERT(qst);

    qLog(QtopiaServer) << "Starting tasks...";

    if ( !qst->m_taskPluginLoader ) {
        loadTaskPlugins();
    }

    qst->argc = &argc;
    qst->argv = argv;

    QList<QtopiaServerTasksPrivate::Task *> startupOrder;
    // Determine startup order
    if(!qst->determineStartupOrder(startupGroups, startupOrder))
        return false;

    if (!startupOrder.count())
        return true;

    if (type == ImmediateStartup) {
        // Actually start the tasks
        for(int ii = 0; ii < startupOrder.count(); ++ii) {
            QtopiaServerTasksPrivate::Task* task = startupOrder.at(ii);
            if (task->launchOrder > 0) {
                qLog(QtopiaServer) << "Would start task, but already started:" << task->name.toByteArray();
            } else {
                qLog(QtopiaServer) << "Starting task" << task->name.toByteArray();
                qst->startTask(task, false);
            }
        }
    }

    else if (type == IdleStartup) {
        IdleTaskStartup* idler = new IdleTaskStartup(qst, startupOrder);

        // Delete the task starter when it completes its duty.
        if (!connect(idler, SIGNAL(finished()), idler, SLOT(deleteLater())))
            Q_ASSERT(0);

        // Start the task starter once we get to the event loop.
        QTimer::singleShot(0, idler, SLOT(start()));
    }

    else {
        qWarning() << "Invalid type parameter to QtopiaServerApplication::startup:" << type;
        return false;
    }

    return true;
}


void QtopiaServerApplication::loadTaskPlugins()
{
    QtopiaServerTasksPrivate *qst = qtopiaServerTasks();
    if(!qst) return;

    QObject* o = 0;
    ServerTaskPlugin* plugin = 0;
    qst->m_taskPluginLoader = new QPluginManager( "servertask" );
    qLog(QtopiaServer) << "Loading server task plugins";
    foreach ( const QString name, qst->m_taskPluginLoader->list() )
    {
        o = qst->m_taskPluginLoader->instance( name );
        if ( !o ) continue;
        plugin = qobject_cast<ServerTaskPlugin*>( o );
        if ( !plugin )  { delete o; continue; }

        QConstCString t( plugin->name().constData(), plugin->name().length() );
        qLog(QtopiaServer) << "Found plugin based task:" << t.data();
        QMap<QConstCString, QtopiaServerTasksPrivate::Task *>::Iterator iter =
            qst->m_availableTasks.find(t);
        if(qst->m_availableTasks.end() == iter) {
            QtopiaServerTasksPrivate::Task *newTask =
                new QtopiaServerTasksPrivate::Task(t);
            //create a permanent refernece to plugin->name() that can be used by QConstCString
            newTask->taskPluginName = plugin->name();
            QConstCString task( newTask->taskPluginName.constData() );
            newTask->name = task;
            newTask->demand = plugin->demand();
            iter = qst->m_availableTasks.insert(task, newTask);
        }

        Q_ASSERT(0 == (*iter)->plugin);
        (*iter)->plugin = plugin;
        (*iter)->createArg = 0;
    }
}

/*!
  Return the task with name \a taskName.  If \a onlyRunning is true, the
  task will be returned only if it has already been instantiated, otherwise
  it will be instantiated and returned.
 */
QObject *QtopiaServerApplication::qtopiaTask(const QByteArray &taskName,
                                             bool onlyRunning)
{
    QtopiaServerTasksPrivate *qst = qtopiaServerTasks();
    if(!qst) return 0;

    QConstCString task(taskName);
    QMap<QConstCString, QtopiaServerTasksPrivate::Task *>::ConstIterator iter =
        qst->m_availableTasks.find(task);

    if(iter != qst->m_availableTasks.end()) {
        return qst->startTask(*iter, onlyRunning);
    } else {
        return 0;
    }
}

/*!
  Add an object \a them to \a me to create an "aggregate" task.  Aggregate tasks
  are designed to work around the limitations in Qt's support for multiple
  interface inheritance by allowing multiple objects to be "stuck together" to
  form a single object from the task system's perspective.

  For example, consider the following interfaces:

  \code
  class ApplicationTypeLauncher : public QObject {};
  QTOPIA_TASK_INTERFACE(ApplicationTypeLauncher);
  class SystemShutdownHandler : public QObject {};
  QTOPIA_TASK_INTERFACE(SystemShutdownHandler);
  \endcode

  Using direct inheritance a single task could not implement both interfaces.
  Instead, a task can create one object that implements the
  \c {ApplicationTypeLauncher} interface and one that implements the
  \c {SystemShutdownHandler} interface and aggregate them together.

  \code
  class MyLauncherShutdown : public SystemShutdownHandler
  {
  public:
      MyLauncherShutdown(MyLauncherType *);
  };

  class MyLauncherType : public ApplicationTypeLauncher
  {
  public:
      MyLauncherType()
      {
          MyLauncherShutdown *s = new MyLauncherShutdown(this);
          QtopiaServerApplication::addAggregateObject(this, s);
      }
  };
  QTOPIA_TASK(MyLauncherType, MyLauncherType);
  QTOPIA_TASK_PROVIDES(MyLauncherType, ApplicationTypeLauncher);
  QTOPIA_TASK_PROVIDES(MyLauncherType, SystemShutdownHandler);
  \endcode

  When the system attempts to cast a task to a particular interface, it first
  tries the task object itself, and then each of the aggregate objects for that
  task.
 */
void QtopiaServerApplication::addAggregateObject(QObject *me, QObject *them)
{
    if(!them || !me) return;

    QtopiaServerTasksPrivate *qst = qtopiaServerTasks();
    if(!qst) return;

    QMap<QObject *, QtopiaServerTasksPrivate::Task *>::Iterator iter = qst->m_activeTasks.find(me);
    if(iter == qst->m_activeTasks.end()) {

        if(qst->m_constructingTask)
            qst->m_constructingAggregates[me].append(them);

    } else {
        (*iter)->aggregates.append(them);
        qst->m_activeTasks.insert(them, *iter);
    }
}

/*! \internal */
void QtopiaServerApplication::addTask(const char *task,
                                      bool demand, CreateTaskFunc func,
                                      void *arg)
{
    QtopiaServerTasksPrivate *qst = qtopiaServerTasks();
    if(!qst) return;

    QConstCString t(task);
    QMap<QConstCString, QtopiaServerTasksPrivate::Task *>::Iterator iter =
        qst->m_availableTasks.find(t);
    if(qst->m_availableTasks.end() == iter) {
        QtopiaServerTasksPrivate::Task *newTask =
            new QtopiaServerTasksPrivate::Task(t);
        newTask->demand = demand;
        iter = qst->m_availableTasks.insert(t, newTask);
    }

    Q_ASSERT(0 == (*iter)->create);
    (*iter)->create = func;
    (*iter)->createArg = arg;
}

/*! \internal */
void QtopiaServerApplication::addTaskProvide(const char *task, const char *interface)
{
    if(!task || !interface) return;

    QtopiaServerTasksPrivate *qst = qtopiaServerTasks();
    if(!qst) return;

    QConstCString t(task);
    QMap<QConstCString, QtopiaServerTasksPrivate::Task *>::Iterator iter =
        qst->m_availableTasks.find(t);
    if(qst->m_availableTasks.end() == iter)
        iter = qst->m_availableTasks.insert(t, new QtopiaServerTasksPrivate::Task(t));

    (*iter)->interfaces.append(interface);
}

/*! 
  \internal 
 */
QObject *QtopiaServerApplication::_qtopiaTask(const char *_iface, bool onlyActive)
{
    if(!_iface) return 0;

    QtopiaServerTasksPrivate *qst = qtopiaServerTasks();
    if(!qst) return 0;

    QConstCString iface(_iface);
    QMap<QConstCString, QList<QtopiaServerTasksPrivate::Task *> >::ConstIterator iter =
        qst->m_availableInterfaces.find(iface);
    if(iter != qst->m_availableInterfaces.end()) {
        Q_ASSERT(!iter->isEmpty());
        return qst->taskForInterface(iter->at(0), iface, onlyActive);
    } else {
        QMap<QConstCString, QList<QPointer<QObject> > >::Iterator iter =
            qst->m_additionalInterfaces.find(iface);
        if(iter == qst->m_additionalInterfaces.end())
            return 0;

        for(int ii = 0; ii < iter->count();) {
            if(0 == iter->at(ii)) {
                iter->removeAt(ii);
            } else {
                return iter->at(ii);
            }
        }

        return 0;
    }
}

/*! \internal */
QList<QObject *> QtopiaServerApplication::_qtopiaTasks(const char *_iface,
                                                 bool onlyActive)
{
    QList<QObject *> rv;

    if(!_iface) return rv;

    QtopiaServerTasksPrivate *qst = qtopiaServerTasks();
    if(!qst) return rv;

    QConstCString iface(_iface);
    QMap<QConstCString, QList<QtopiaServerTasksPrivate::Task *> >::ConstIterator iter =
        qst->m_availableInterfaces.find(iface);
    if(iter != qst->m_availableInterfaces.end()) {
        Q_ASSERT(!iter->isEmpty());

        for(int ii = 0; ii < iter->count(); ++ii) {
            QObject *to =  qst->taskForInterface(iter->at(ii), iface,
                                                 onlyActive);
            if(to) rv.append(to);
        }
    }

    QMap<QConstCString, QList<QPointer<QObject> > >::Iterator aiter =
        qst->m_additionalInterfaces.find(iface);
    if(aiter != qst->m_additionalInterfaces.end()) {
        for(int ii = 0; ii < aiter->count();) {
            if(0 == aiter->at(ii)) {
                aiter->removeAt(ii);
            } else {
                rv.append(aiter->at(ii));
                ++ii;
            }
        }
    }

    return rv;
}

/*! \internal */
void QtopiaServerApplication::addTaskInterface(const char *_iface, QObject *obj)
{
    if(!_iface || !obj) return;

    QtopiaServerTasksPrivate *qst = qtopiaServerTasks();
    if(!qst) return;

    QConstCString iface(_iface);

    qst->m_additionalInterfaces[iface].append(obj);
}

/*!
  Returns the type of shutdown previously requested by calling shutdown(), or
  NoShutdown if no shutdown has been requested.
 */
QtopiaServerApplication::ShutdownType QtopiaServerApplication::shutdownType()
{
    QtopiaServerTasksPrivate *qst = qtopiaServerTasks();
    if(!qst) return NoShutdown;
    return qst->m_shutdown;
}

void QtopiaServerApplication::_shutdown(ShutdownType type)
{
    Q_ASSERT(NoShutdown != type);
    QtopiaServerTasksPrivate *qst = qtopiaServerTasks();
    if(!qst) return;

    QList<SystemShutdownHandler *> shutdown =
        ::qtopiaTasks<SystemShutdownHandler>();
    qst->setShutdownObjects(shutdown);
    qst->m_shutdown = type;
    for(int ii = 0; ii < shutdown.count(); ++ii) {
        QObject::connect(shutdown.at(ii), SIGNAL(proceed()),
                         qst, SLOT(shutdownProceed()));

        bool proceedImmediately = false;
        if(RestartDesktop == type) {
            proceedImmediately = shutdown.at(ii)->systemRestart();
        } else {
            proceedImmediately = shutdown.at(ii)->systemShutdown();
        }

        if(proceedImmediately)
            qst->ackShutdownObject(shutdown.at(ii));
    }

    QTime t;
    t.start();
    while(!qst->shutdownDone())
    {
        struct Remaining {
            static inline QString handlers(QtopiaServerTasksPrivate* qst) {
                QStringList ret;
                foreach (SystemShutdownHandler* h, qst->m_shutdownObjects) {
                    ret << QString("%1(0x%2)").arg(h->metaObject()->className()).arg(quintptr(h), 0, 16);
                }
                return ret.join(",");
            }
        };
        // If this assert fires, then some of the server shutdown tasks above
        // have failed to exit correctly, and require bug-fixing
        Q_ASSERT_X( t.elapsed() < SystemShutdownHandler::timeout() + 1000, "shutdown",
                qPrintable(QString("Shutdown handlers failed to complete: %1").arg(Remaining::handlers(qst)))
        );
        QApplication::instance()->processEvents();
    }

    //delete all running tasks as we are about to shutdown
    qst->cleanupTasks();
    QApplication::instance()->quit();
}

// declare ClassReplacement
struct ClassReplacement {
    QByteArray optionName;
    bool lastUsedSet;
    _ReplacementInstaller::CreateFunc lastUsed;

    typedef QHash<QByteArray, QPair<_ReplacementInstaller::CreateFunc, QByteArray> > CreateFuncs;
    CreateFuncs options;
    QPointer<QWidget> singletonInstance;
};

// declare Replacements
struct Replacements {
    Replacements() : m_configLoaded(false) {}

    typedef QHash<const QMetaObject *, ClassReplacement> Classes;
    Classes m_classes;

    typedef QHash<QByteArray, QByteArray> Configuration;
    Configuration m_config;
    QByteArray m_defaultName;
    bool m_configLoaded;

    void loadConfig();
    void add(const QMetaObject *you, const QMetaObject *them,
             const QByteArray &feature, _ReplacementInstaller::CreateFunc);
};
Q_GLOBAL_STATIC(Replacements, replacements);

// define Replacements
void Replacements::loadConfig()
{
    if(m_configLoaded) return;

    m_config.clear();
    m_defaultName = "Default";

    QSettings settings("Trolltech", "ServerWidgets");
    settings.beginGroup("Mapping");
    QStringList keys = settings.childKeys();
    for(int ii = 0; ii < keys.count(); ++ii) {
        const QString &key = keys.at(ii);
        QByteArray value = settings.value(key).toByteArray();

        if(key == "Default") {
            m_defaultName = value;
        } else if(!key.isEmpty() && !value.isEmpty()) {
            m_config.insert(key.toLatin1(), value);
        }
    }

    qLog(QtopiaServer) << "Default server widget name:" << m_defaultName;
    m_configLoaded = true;
}

void Replacements::add(const QMetaObject *you,
                       const QMetaObject *them,
                       const QByteArray &feature,
                       _ReplacementInstaller::CreateFunc create)
{
    Q_ASSERT(you);
    Q_ASSERT(them);
    Q_ASSERT(create);

    /* We discard the "QAbstract" part for the class name.  That way if you
       have a class named "QAbstractBrowserScreen", you actually configure the
       bind to it as "BrowserScreen=" */
    QByteArray themName(them->className());
    if(themName.startsWith("QAbstract"))
        themName = themName.mid(9 /* ::strlen("QAbstract") */);

    /* We discard the end of the name if it is the same as the them.  That way
       you can scope your classes.  For example, "QAbstractBrowserScreen"
       replaced with "WheelBrowserScreen" is configured as "BrowserScreen=Wheel"
     */
    QByteArray youName(you->className());
    if(youName.endsWith(themName))
        youName = youName.left(youName.length() - themName.length());

    Replacements::Classes::Iterator iter = m_classes.find(them);
    if(iter == m_classes.end()) {
        iter = m_classes.insert(them, ClassReplacement());
        iter->optionName = themName;
        iter->lastUsed = 0;
        iter->lastUsedSet = false;
        iter->singletonInstance = 0;
    }

    qLog(Component) <<  "Registering server widget:"
        << them->className() << "->" << you->className()
        << (!feature.isEmpty()
                ? ("(depends on " + feature + ")").constData()
                : QByteArray().constData());
    iter->options.insert(youName, qMakePair(create, feature));
}

// define _ReplacementInstaller
_ReplacementInstaller::_ReplacementInstaller(const QMetaObject *you,
                                             const QMetaObject *them,
                                             CreateFunc create)
{
    Replacements *r = replacements();
    if(!r) return;

    r->add(you, them, QByteArray(), create);
}

_ReplacementInstaller::_ReplacementInstaller(const QMetaObject *you,
                                             const QMetaObject *them,
                                             const char *feature,
                                             CreateFunc create)
{
    Replacements *r = replacements();
    if(!r) return;

    r->add(you, them, QByteArray(feature), create);
}

/*!
  Wrap a value space object around \a widget. This enables 
  the lookup of server widget mappings.
  */
QWidget *wrapValueSpace( QWidget* widget, const QMetaObject* them )
{
    if ( !widget )
        return 0;

    //qtopiaWidget<> creates a new instance each time it is called.
    //we are only interested in one
    QValueSpaceItem item( QLatin1String("/System/ServerWidgets/") );
    QStringList sub = item.subPaths();
    if ( sub.contains( them->className() ) )
        return widget;

    QValueSpaceObject* obj = new QValueSpaceObject( 
        QLatin1String("/System/ServerWidgets/")+ them->className(), widget );
    obj->setAttribute( QByteArray(), widget->metaObject()->className() );

    return widget;
}

QWidget *_ReplacementInstaller::widget(const QMetaObject *them,
                                       QWidget *parent,
                                       Qt::WFlags flags)
{
    Replacements *re = replacements();
    if(!re) return 0;

    Replacements::Classes::Iterator classIter = re->m_classes.find(them);
    if(classIter == re->m_classes.end()) {
        qLog(QtopiaServer) << "No replacements found for widget" << them->className();
        return 0;
    }

    int infoIndex = them->indexOfClassInfo( "SingletonServerWidget" );
    bool singleton = false;
    if ( infoIndex >= 0 && 0 == strcmp(them->classInfo(infoIndex).value(), "true") )
        singleton = true;

    if(classIter->lastUsedSet) {
        if(classIter->singletonInstance)
            return classIter->singletonInstance;
        if(classIter->lastUsed) {
            QWidget* w = wrapValueSpace(classIter->lastUsed(parent, flags), them);
            if ( singleton )
                classIter->singletonInstance = w;
            return w;
        } else {
            return 0;
        }
    }

    re->loadConfig();

    // Locate create func
    Replacements::Configuration::ConstIterator configIter =
        re->m_config.find(classIter->optionName);
    if(configIter != re->m_config.end()) {

        if("None" == *configIter) {
            classIter->lastUsedSet = true;
            classIter->lastUsed = 0;
            qLog(QtopiaServer) << "Server widget" << them->className() << "maps to None";
            return 0;
        }

        ClassReplacement::CreateFuncs::ConstIterator createIter =
            classIter->options.find(*configIter);
        if(createIter != classIter->options.end()) {
            classIter->lastUsed = createIter->first;
            classIter->lastUsedSet = true;
            qLog(QtopiaServer) << "Server widget mapping: "
                << classIter->optionName << "->" << *configIter;
        }
    }

    if(!classIter->lastUsedSet) {
        // Try default
        ClassReplacement::CreateFuncs::ConstIterator createIter =
            classIter->options.find(re->m_defaultName);
        if(createIter != classIter->options.end()) {
            if(createIter->second.isEmpty() ||
               QtopiaFeatures::hasFeature(createIter->second)) {
                classIter->lastUsed = createIter->first;
                classIter->lastUsedSet = true;
                qLog(QtopiaServer) << "Server widget mapping (based on default):"
                    << classIter->optionName << "->" << createIter.key();
            } else {
                qLog(QtopiaServer) << "Server widget" << them->className()
                    << ": can't use" << createIter.key() << "because it requires"
                    << createIter->second;
            }
        }
    }

    if(!classIter->lastUsedSet) {
        // Try all defaults
        for(ClassReplacement::CreateFuncs::ConstIterator createIter =
                classIter->options.begin();
                !classIter->lastUsedSet &&
                createIter != classIter->options.end();
                ++createIter) {

            if(createIter.key().startsWith(re->m_defaultName) &&
               (createIter->second.isEmpty() ||
               QtopiaFeatures::hasFeature(createIter->second))) {
                classIter->lastUsed = createIter->first;
                classIter->lastUsedSet = true;
                qLog(QtopiaServer) << "Server widget mapping (based on default name start):"
                    << classIter->optionName << "->" << createIter.key();
            }
        }
    }

    if(!classIter->lastUsedSet) {
        // Try any

        for(ClassReplacement::CreateFuncs::ConstIterator createIter =
                classIter->options.begin();
                !classIter->lastUsedSet &&
                createIter != classIter->options.end();
                ++createIter) {

            if(createIter->second.isEmpty() ||
               QtopiaFeatures::hasFeature(createIter->second)) {
                classIter->lastUsed = createIter->first;
                classIter->lastUsedSet = true;
                qLog(QtopiaServer) << "Server widget mapping (random pick):"
                    << classIter->optionName << "->" << createIter.key();
            } else {
                qLog(QtopiaServer) << "Server widget" << them->className()
                    << ": can't use" << createIter.key() << "because it requires"
                    << createIter->second;
            }
        }

    }

    classIter->lastUsedSet = true;
    if(classIter->lastUsed) {
        QWidget *w = wrapValueSpace(classIter->lastUsed(parent, flags), them);
        if ( singleton )
            classIter->singletonInstance = w;
        return w;
    }
    else
        return 0;
}

// define SystemShutdownHandler
/*!
  \class SystemShutdownHandler
    \inpublicgroup QtBaseModule
  \ingroup QtopiaServer::Task::Interfaces
  \ingroup QtopiaServer::AppLaunch
  \brief The SystemShutdownHandler class notifies tasks when the system is shutting down or restarting.

  The SystemShutdownHandler provides a Qt Extended Server Task interface.  Qt Extended Server Tasks are documented in full in the QtopiaServerApplication class
  documentation.

  Server components can use the SystemShutdownHandler to integrate into the
  shutdown mechanism.

  Tasks that provide the SystemShutdownHandler interface are called when the
  system is shutting down or restarting.  By providing this interface, tasks
  can ensure that they clean up appropriately before a shutdown or restart.

  During shutdown or restart, the systemShutdown() or systemRestart() methods
  are invoked respectively.  If a task cannot complete its cleanup synchronously
  in this invocation, it may return false which indicates that it has
  not completed.  Other handlers will continue to be invoked, but the final
  shutdown or restart will not occur until all such incomplete handlers have
  emitted the proceed() signal.

  All shutdown handlers must successfully complete shutdown within
  timeout() milliseconds.

  Shutdown is controlled through the QtopiaServerApplication::shutdown() method.

  This class is part of the Qt Extended server and cannot be used by other Qt Extended applications.
  \sa QtopiaServerApplication
 */

/*!
  \fn void SystemShutdownHandler::proceed()

  Emitted to indicate the completion of this instances portion of shutdown or
  restart.  This signal should only ever be emitted after the class has delayed
  the completion of a shutdown or restart by returning false from the
  systemRestart() or systemShutdown() methods.
 */

/*!
  \fn int SystemShutdownHandler::timeout()

  Returns an upper bound on the amount of time a shutdown handler may take to process
  a shutdown request, in milliseconds.
  If a shutdown handler has not finished in this time, the server may forcibly
  proceed with the shutdown.
*/

/*!
  Invoked whenever the system is restarting.  If the implementer returns
  true (the default), the system will continue to restart.  If it returns false,
  the system restart will not complete until the instance emits the proceed()
  signal.
 */
bool SystemShutdownHandler::systemRestart()
{
    return true;
}

/*!
  Invoked whenever the system is shutting down.  If the implementer returns
  true (the default), the system will continue to shut down.  If it returns
  false, the system shut down will not complete until the instance emits the
  proceed() signal.
 */
bool SystemShutdownHandler::systemShutdown()
{
    return true;
}

#include "qtopiaserverapplication.moc"
