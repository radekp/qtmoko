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

#include <qtopianamespace.h>
#include <version.h>
#ifndef QTOPIA_HOST
#include <custom.h>
#include "qactionconfirm_p.h"
#endif
#if !defined(QTOPIA_HOST) || defined(QTOPIA_CONTENT_INSTALLER)
#include <qstorage.h>
#endif
#include <stdlib.h>
#include <errno.h>
#include <math.h>
#include <float.h>

#include <qtopiaipcenvelope.h>
#include <qtopiaservices.h>

#include <QFontDatabase>
#include <QFontMetrics>

#include <qdawg.h>

#include <QMap>
#include <QDir>
#include <QFileInfo>
#include <QUuid>
#include <QSettings>
#include <QLibraryInfo>
#include <QMutex>
#include <QDebug>
#include <QDateTime>

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <mntent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef QTOPIA_HOST

#ifdef Q_WS_X11
#include <qx11info_x11.h>
#include <X11/Xlib.h>
#undef Unsorted
#endif

int qtopia_display_id()
{
#if defined(Q_WS_QWS)
    extern int qws_display_id;
    return qws_display_id;
#elif defined(Q_WS_X11)
    Display *dpy = QX11Info::display();
    QString name;
    if ( dpy ) {
        name = QString( DisplayString( dpy ) );
    } else {
        const char *d = getenv("DISPLAY");
        if ( !d )
            return 0;
        name = QString( d );
    }
    int index = name.indexOf(QChar(':'));
    if ( index >= 0 )
        return name.mid(index + 1).toInt();
    else
        return 0;
#else
    return 0;
#endif
}
#endif

class QtopiaPathHelper
{
public:
    static QtopiaPathHelper *instance();
    QStringList installPaths;
    QString packagePath;
    QString updatePath;
    QString prefixPath;
private:
    void setPackagePath();
    void setUpdatePath();
    QString mountPointPath(const QString &mountPoint);
};

/*
  Cache all the path look-up in a singleton so it only has to be done once
  per process.  Previous implementations also did this, but lets make it
  thread-safe while we're on the job.
*/
QtopiaPathHelper *QtopiaPathHelper::instance()
{
    static QtopiaPathHelper ph;
    static QAtomicInt initialized(0);

    if ( initialized.testAndSetOrdered( 0, 1 ))
    {
        QChar sl = QDir::separator();
        const char *d = getenv("QTOPIA_PATH");
        if ( d ) {
            ph.installPaths = QString(d).split(":");
            for (QStringList::Iterator it=ph.installPaths.begin(); it!=ph.installPaths.end(); ++it) {
                if ( (*it)[(*it).length()-1] != sl )
                    (*it) += sl;
            }
        }

        // The installation directory is always searched before QTOPIA_PATH
        ph.prefixPath = QLibraryInfo::location(QLibraryInfo::PrefixPath);
        if ( ph.prefixPath[ph.prefixPath.length()-1] != sl )
            ph.prefixPath += sl;
        ph.installPaths.prepend(ph.prefixPath);

        // System update directory is always searched first
        ph.setUpdatePath();
        QString up = ph.updatePath;
        if ( !up.isEmpty() )
            ph.installPaths.prepend(up);

        // Package paths are last
        ph.setPackagePath();
        QString pp = ph.packagePath;
        if ( !pp.isEmpty() )
            ph.installPaths.append( pp );
        QDir::root().mkpath( pp + "/pics" );
        QDir::root().mkpath( pp + "/sounds" );

        initialized = 2;
    }
    else
    {
        while ( initialized != 2 )
            Qtopia::msleep( 5 );
    }

    return &ph;
}

/*
  Find the package path from the Storage.conf file

  This code duplicates somewhat code already in qstorage.cpp, but there's
  objections to using that when a QApplication object hasn't been constructed.

  This should only ever be called from the instance method above.
*/
void QtopiaPathHelper::setPackagePath()
{
    QSettings storage( QLatin1String("Trolltech"), QLatin1String("Storage") );
    QString mountPoint = "HOME";
    QString pp = "packages/";
    if ( storage.childGroups().contains( "Packages" ))
    {
        storage.beginGroup( "Packages" );
        mountPoint = storage.value( QLatin1String("MountPoint") ).toString();
        pp = storage.value( QLatin1String("Path") ).toString();
        storage.endGroup();
    }

    if ( pp.right(1) != "/" )
        pp.append("/");
    if ( pp.left(1) != "/" )
        pp.prepend("/");

    QString mountPath = mountPointPath(mountPoint);
    if (!mountPath.isEmpty())
        packagePath = pp.prepend(mountPath);
}

/*
  Find the system update path from the Storage.conf file

  This code duplicates somewhat code already in qstorage.cpp, but there's
  objections to using that when a QApplication object hasn't been constructed.

  This should only ever be called from the instance method above.
*/
void QtopiaPathHelper::setUpdatePath()
{
    QSettings storage( QLatin1String("Trolltech"), QLatin1String("Storage") );
    QString mountPoint = "HOME";
    QString up = "updates/";
    if ( storage.childGroups().contains( "Updates" ))
    {
        storage.beginGroup( "Updates" );
        mountPoint = storage.value( QLatin1String("MountPoint") ).toString();
        up = storage.value( QLatin1String("Path") ).toString();
        storage.endGroup();
    }

    if ( up.right(1) != "/" )
        up.append("/");
    if ( up.left(1) != "/" )
        up.prepend("/");

    QString mountPath = mountPointPath(mountPoint);
    if (!mountPath.isEmpty())
        updatePath = up.prepend(mountPath);
}

/*
  Find the real path of the of \a mountPoint from the Storage.conf file

  This code duplicates somewhat code already in qstorage.cpp, but there's
  objections to using that when a QApplication object hasn't been constructed.

  This should only ever be called from the instance method above.
*/
QString QtopiaPathHelper::mountPointPath(const QString &mountPoint)
{
    if ( mountPoint == "HOME" )
        return Qtopia::homePath();

    QSettings storage( QLatin1String("Trolltech"), QLatin1String("Storage") );
    storage.beginGroup( mountPoint );
    QString mountPath = storage.value( QLatin1String("Path") ).toString();
    storage.endGroup();
    if ( mountPoint.isEmpty() || mountPath.isEmpty() ) {
        qWarning("Could not find mount point %s in Storage.conf", qPrintable( mountPoint ));
        return QString();
    }

#if !defined(QT_LSB)
    mntent *me;
    FILE *mntfp = NULL;
    mntfp = setmntent( "/proc/mounts", "r" );
    me = getmntent(mntfp);
    while ( me != NULL )
    {
        if ( mountPath == me->mnt_fsname )
            return me->mnt_dir;
        me = getmntent(mntfp);
    }
    endmntent(mntfp);
#endif

    qWarning( "Could not found filesystem name %s in /proc/mounts - bad Storage.conf",
            qPrintable( mountPath ));
    return QString();
}

/*!
    \namespace Qtopia
    \inpublicgroup QtBaseModule

    \brief The Qtopia namespace contains miscellaneous functionality.

    \section1 Word list related

    A list of words relevant to the current locale is maintained by the
    system. The list is held in a QDawg.
    This list is used, for example, by
    some input methods.

    The global QDawg is returned by fixedDawg(); this cannot be updated.

    The addedDawg() contains words that have been added by addWords(); this dawg
    is also referred to as dawg("local").

    The dawg("deleted") contains words that are in the fixedDawg() but which
    the user has explicitly deleted, using removeWords().

    The dawg("preferred") contains words that are in either fixedDawg() or
    dawg("local") and which the user has explicitly marked as preferred
    to other words, for example, for the purpose of some input methods.

    Applications may have their own word lists stored in \l{QDawg}s
    which are returned by dawg(). Use addWords() to add words to the
    modifiable copy of the global QDawg or to named application
    \l{QDawg}s. Applications should consider the namespace of the
    word lists, prefixing with the application name where appropriate.

    \section1 Path related

    These methods return the authoritative values of run-time paths used
    by various components of Qtopia.

    In general do not use these methods.  Instead use the QContent
    API, or the \l {Qt Extended Resource System}.  Qt Extended is designed so that
    searching or scanning the file-system should not be necessary.

    When it is necessary to determine locations on the file-system,
    use homePath() to find the location for user settings, documentDir()
    for user documents, and helpPaths() to locate all help files.

    Use installPaths() to find all possible install paths, for example to
    locate a binary resource.  Use qtopiaDir() for the default location for
    resources, and packagePath() as the location for third-party resources.

    \quotefromfile appviewer/appviewer.cpp
    \skipto installPaths
    \printuntil return info

    The \l installPaths() method gives the complete list of paths known to
    the Qt Extended run-time system.  The updateDir(), qtopiaDir() and packagePath()
    directories are stored as the first, second and last components of this list.

    The order of the paths returned by \l installPaths() and where the path
    is defined is
    \list
        \o System update path - \l{Document Storage}{Storage.conf}
        \o Default resource location - \l qtopiaDir()
        \o Qt Extended Path environment variable
        \o Third-party package location - \l packagePath()
    \endlist

    In fact the paths are determined by compile time configuration options.

    See the documentation on building Qt Extended for the -prefix and other
    relevant options:
    \list
        \o \l {Building and Compiling}
        \o \l {Guide to Configuring and Building Qt Extended}
    \endlist

    \ingroup misc
    */

/*!
   \fn QStringList Qtopia::installPaths()

   Returns the list of base directories containing Qt Extended software
   and resources.

   Some directories in the list, or their sub-directories,
   may not be writable.

   Internally the second element is equal to [qt_prefix] and additional values
   are set by the colon-separated environment variable QTOPIA_PATH.

   The Qt Extended algorithm for locating resources searches the paths in order
   so that for example downloaded binaries or resources cannot overwrite
   system resources.

   Note that qtopiaDir(), the default location for resources and binaries
   is second on this list, and packagePath() is last.

   \sa qtopiaDir(), packagePath()
*/

QStringList Qtopia::installPaths()
{
    return QtopiaPathHelper::instance()->installPaths;
}

/*!
   \fn QString Qtopia::packagePath()

   Returns the base directory in which downloaded 3rd party packages,
   including their binaries and resources, are located.

   This is returned as the last item in the installPaths() list.

   The value of packagePath() is determined by the [Packages] group
   in the Storage.conf file.

   The directory must be writable, and is used by the Software Installer
   for installation of downloaded software.

   \sa installPaths()
*/
QString Qtopia::packagePath()
{
    return QtopiaPathHelper::instance()->packagePath;
}

/*!
   \fn QString Qtopia::updateDir()

   Returns the directory in which Qt Extended system updates are installed to.

   Binaries and resources located in this location are used in preference
   to those located in the directory returned by qtopiaDir().

   This is returned as the first item in the installPaths() list.

   The value of updateDir() is determinted by the [Updates] group in the
   Storage.conf file.

   The directory must be writable, and is used by the Software Installer
   for installation of downloaded system updates.
       
   This is the first item in the installPaths() list, and as such
   is the first location searched by the Qt Extended resource system, and
   the algorithm for locating binaries for execution.

   \sa qtopiaDir(), installPaths()
 */
QString Qtopia::updateDir()
{
    return QtopiaPathHelper::instance()->updatePath;
}

/*!
   \fn QString Qtopia::qtopiaDir()

   Returns the base directory in which Qt Extended system binaries
   and resources are located.

   This is the default location for all Qt Extended configuration and other
   data.

   This directory may not be writable.

   This is the second item in the installPaths() list.

   \sa updateDir(), installPaths()
 */
QString Qtopia::qtopiaDir()
{
    return QtopiaPathHelper::instance()->prefixPath;
}

/*!
  \fn QString Qtopia::documentDir()

  Returns the user's current Document directory, with a trailing "/" included.
*/
QString Qtopia::documentDir()
{
#if !defined(QTOPIA_HOST) || defined(QTOPIA_CONTENT_INSTALLER)
    return QFileSystem::documentsFileSystem().documentsPath() + '/';
#else
    return QString();
#endif
}

/*!
  \fn QString Qtopia::homePath()

  Returns the name of the directory to be used as the current
  users home directory.

  \sa QDir::homePath()
*/
QString Qtopia::homePath()
{
    return QDir::homePath();
}


/*!
  \fn QString Qtopia::defaultButtonsFile()
  \internal

  Return the name of the defaultbuttons.conf file.
  This allows Qt Extended to use a QVFb-supplied defaultbuttons.conf file
  (if one exists).
*/
QString Qtopia::defaultButtonsFile()
{
    QString r;
#if !defined(QTOPIA_HOST) && !defined(QT_NO_QWS_VFB)
    r = QString("/tmp/qtembedded-%1/defaultbuttons.conf").arg(qtopia_display_id());
    if ( QFileInfo(r).exists() )
        return r;
#endif
    r = updateDir() + "etc/defaultbuttons.conf";
    if (QFileInfo(r).exists())
        return r;

    return qtopiaDir()+"etc/defaultbuttons.conf";
}

/*!
  \fn QStringList Qtopia::helpPaths()

  Returns a list of directory names where help files are found.
*/
QStringList Qtopia::helpPaths()
{
    QStringList path;
    QStringList langs = languageList();
    QStringList qpepaths = installPaths();
    for (QStringList::ConstIterator qit = qpepaths.begin(); qit!=qpepaths.end(); ++qit) {
        QStringListIterator it(langs);
        it.toBack();
        while (it.hasPrevious()) {
            QString lang = it.previous();
            if ( !lang.isEmpty() )
                path += *qit + "help/" + lang + "/html";
        }
        path += *qit + "pics";
        path += *qit + "help/html";
        path += *qit + "docs";
    }
    return path;
}

/*!
    Shows a dialog with a \a caption to confirm deletion of the described
    \a object with the user.  This is a convenience function that makes
    confirming deletions easier. For example:

    \code
    if (Qtopia::confirmDelete(this, tr("Delete Contact"), contact->name()))
        delete contact;
    \endcode

    If \a parent is 0, the dialog becomes an application-global modal dialog
    box. If \a parent is a widget, the dialog becomes modal relative to
    \a parent.

    Returns true if the user agrees to deleting \a object; otherwise returns false.

    \sa QMessageBox::warning()
*/
bool Qtopia::confirmDelete( QWidget *parent, const QString & caption, const QString & object )
{
    QString msg = "<qt>" + qApp->translate( "Qtopia::confirmDelete", "Are you sure you want to delete: %1?").arg( object ) + "</qt>";
    int r = QMessageBox::warning( parent, caption, msg, QMessageBox::Yes, QMessageBox::No|QMessageBox::Default| QMessageBox::Escape, 0 );
    return r == QMessageBox::Yes;
}

/*!
    \fn void Qtopia::actionConfirmation(const QPixmap &pixmap, const QString &text)

    Displays a message dialog containing the specified \a pixmap and \a text
    for a short time.
*/
void Qtopia::actionConfirmation(const QPixmap &pixmap, const QString &text)
{
#ifndef QTOPIA_HOST
    QActionConfirm::display(pixmap, text);
#else
    Q_UNUSED(pixmap);
    Q_UNUSED(text);
#endif
}

/*!
    Sounds the audible system alarm. This is used for applications such
    as Calendar when it needs to inform the user of an event.

    This call is equivalent to:
    \code
        QtopiaServiceRequest e( "Alert", "soundAlert" );
        e.send();
    \endcode

    \sa AlertService
*/
void Qtopia::soundAlarm()
{
#ifndef QTOPIA_HOST
    QtopiaServiceRequest e( "Alert", "soundAlert()" );
    e.send();
#endif
}

/*!
  \fn void Qtopia::statusMessage(const QString& message)
  \obsolete
 
  Displays a status \a message to the user. This usually appears
  in the task bar for a short amount of time, then disappears.

  This method is related to the PDA Edition which no longer exists. Using it will
  not cause a failure but nothing will happen when it is called.
*/
void Qtopia::statusMessage(const QString& message)
{
    Q_UNUSED( message )
}


/*

   Translation and language functions

 */

/*!
  \fn QStringList Qtopia::languageList()

  Returns the list of language identifiers for currently selected language.
  The first string in the list is the identifier for user's primary choice of language.
  The second string in the list, if any, is more generic language, providing fall-back policy.

  For example, "en_US", "en" is returned when American English is used.
  If there is no translation in "en_US", more generic translation "en" will be used.

  \sa Internationalization
*/
QStringList Qtopia::languageList()
{
    QString lang = getenv("LANG");
    QStringList langs;

    int i = lang.indexOf(QLatin1Char('.'));
    if ( i > 0 )
        lang = lang.left( i );
    langs.append(lang);
    i = lang.indexOf(QLatin1Char('_'));
    if ( i > 0 )
        langs.append(lang.left(i));

    return langs;
}

static QMap<QString,QList<QTranslator*>* > *trmap=0;

static void clean_trmap()
{
    if ( trmap ) {
        QList<QString> keys = trmap->keys();
        foreach ( QString k, keys ) {
            QList<QTranslator*> *l = trmap->take( k );
            if ( l ) {
                while ( !l->isEmpty() )
                    delete l->takeLast();
                delete l;
            }
        }
        delete trmap;
        trmap = 0;
    }
}

static QList<QTranslator*>* qLocalTranslations(const QString& key)
{

    if ( !trmap ) {
        trmap = new QMap<QString,QList<QTranslator*>* >;
        qAddPostRoutine( clean_trmap );
    }

    QList<QTranslator*> *l = 0;
    if (trmap->contains(key)) {
        l = trmap->value(key);
        return l;
    }

    l = new QList<QTranslator*>;

    QStringList langs = Qtopia::languageList();

    QStringList qpepaths = Qtopia::installPaths();
    for (QStringList::ConstIterator qit = qpepaths.begin(); qit!=qpepaths.end(); ++qit) {
        for (QStringList::ConstIterator it = langs.begin(); it!=langs.end(); ++it) {
            QString lang = *it;

            QString d = *qit + "i18n/" + lang + "/";
            QDir dir(d, key + ".qm", QDir::Unsorted, QDir::Files);
            if (!dir.exists())
                continue;
            for ( int i=0; i<(int)dir.count(); i++ ) {
                QTranslator * trans = new QTranslator();
                if ( trans->load( d+dir[i] ))
                    l->append(trans);
                else
                    delete trans;
            }
        }
    }

    trmap->insert(key,l);

    return l;
}

/*!
Translate \a str in the context \a c, using only the translation files defined by \a key.
The \a key may be either a single name, such as "QtopiaSettings", or it may be a
'wildcard', such as "Categories-*". The \a key thus defines the set of translation files
in which to search for a translation.
*/
QString Qtopia::translate(const QString& key, const QString& c, const QString& str)
{
    static QMutex translationMutex;
    QMutexLocker locker( &translationMutex );
    QList<QTranslator*>* l = qLocalTranslations(key);
    if ( !l || l->isEmpty() ) {
        return str;
    }
    for (QList<QTranslator*>::ConstIterator it = l->begin(); it!=l->end(); ++it) {
        QString msg = (*it)->translate(c.toLatin1(),str.toUtf8());
        if ( !msg.isEmpty() )
            return msg;
    }
    return str;
}

/*

String manipulation functions

*/

/*!
  \fn QString Qtopia::simplifyMultiLineSpace( const QString &multiLine )

  Returns the result of using QString::simplified() on \a multiLine, but with
  line breaks preserved.
*/
QString Qtopia::simplifyMultiLineSpace( const QString &multiLine )
{
    QString result;
    QStringList lines =  multiLine.split('\n');
    for ( QStringList::Iterator it = lines.begin(); it != lines.end(); ++it ) {
        if ( it != lines.begin() )
            result += "\n";
        result += (*it).simplified();
    }
    return result;
}

/*!
  \fn QString Qtopia::dehyphenate(const QString& s)

  Returns the string equivalent to \a s, but with any soft-hyphens removed.
*/
QString Qtopia::dehyphenate(const QString& s)
{
    QChar shy(0x00ad);
    const QChar *data = s.data();
    int i=0;
    while (i<(int)s.length() && *data!=shy) {
        i++;
        data++;
    }
    if ( i==(int)s.length() )
        return s;
    QString r = s.left(i);
    for (; i<(int)s.length(); ++i) {
        if ( s[i]!=shy )
            r += s[i];
    }
    return r;
}

/*!
  \fn void Qtopia::sleep( unsigned long secs )

  Suspends the current process for \a secs seconds.

  Note that this function should be avoided where possible as it will freeze the user interface.
  Use of QTimer and QObject::startTimer() allow for delayed processing without freezing the user
  interface at the cost of being unable to trigger accurately.

  \sa QThread::sleep()
 */
void Qtopia::sleep( unsigned long secs )
{
    ::sleep( secs );
}

/*!
  \fn void Qtopia::msleep( unsigned long msecs )

  Suspends the current process for \a msecs milliseconds.

  Note that this function should be avoided where possible as it will freeze the user interface.
  Use of QTimer and QObject::startTimer() allow for delayed processing without freezing the user
  interface at the cost of being unable to trigger accurately.

  \sa QThread::msleep()
 */
void Qtopia::msleep( unsigned long msecs )
{
    usleep( msecs * 1000 );
}

/*!
  \fn void Qtopia::usleep( unsigned long usecs )

  Suspends the current process for \a usecs microseconds.

  Note that this function should be avoided where possible as it will freeze the user interface.
  Use of QTimer and QObject::startTimer() allow for delayed processing without freezing the user
  interface at the cost of being unable to trigger accurately.

  \sa QThread::usleep()
 */
void Qtopia::usleep( unsigned long usecs )
{
    if ( usecs >= 1000000 )
        ::sleep( usecs / 1000000 );
    ::usleep( usecs % 1000000 );
}

/*!
  \fn QString Qtopia::version()

  Returns the QtMoko version string, specified by \i QTMOKO_VERSION_STR. This is of the form:
  \i{major} .\i{minor} .\i{patchlevel}  (eg. "1.2.3"),
  possibly followed by a space and special information
  (eg. "1.2.3 beta4"). Usually all except \i{major} is ommited.
*/
QString Qtopia::version()
{
    return QTMOKO_VERSION_STR;
}

/*!
  \fn QString Qtopia::architecture()

  Returns the device architecture string specified by QPE_ARCHITECTURE. This is a sequence
  of identifiers separated by "/", from most general to most specific (eg. "IBM/PC").

  If a value has not been set for QPE_ARCHITECTURE the string \c{"Uncusomized Device"} is returned.
*/
QString Qtopia::architecture()
{
#ifndef QPE_ARCHITECTURE
#define QPE_ARCHITECTURE "Uncustomized Device"
#endif
    return QPE_ARCHITECTURE;
}

/*!
  \fn QString Qtopia::deviceId()

  Returns a unique ID for this device. The value can change, if
  for example, the device is reset.
*/
QString Qtopia::deviceId()
{
    QSettings cfg("Trolltech","Security");
    cfg.beginGroup("Sync");
    QString r=cfg.value("serverid").toString();
    if ( r.isEmpty() ) {
        QUuid uuid = QUuid::createUuid();
        cfg.setValue("serverid",(r = uuid.toString()));
    }
    return r;
}

/*!
  \fn QString Qtopia::ownerName()

  Returns the name of the owner of the device.
*/
QString Qtopia::ownerName()
{
    QSettings cfg("Trolltech","Security");
    cfg.beginGroup("Sync");
    QString r=cfg.value("ownername").toString();
    return r;
}

/*!
  Returns true if able to truncate file to size specified
  \a f must be an open file
  \a size must be a positive value; otherwise returns false.
 */
bool Qtopia::truncateFile(QFile &f, int size)
{
    if (!f.isOpen())
        return false;

    return ::ftruncate(f.handle(), size) != -1;
}

/*!
  \fn QString Qtopia::tempDir()

  Returns the default system path for storing temporary files. The path is
  unique to the display to which the application is connected. The path has
  a trailing directory separator character.

  The returned directory is created if it does not exist.

  \sa QDir::tempPath()
 */
QString Qtopia::tempDir()
{
    QString result;

#ifndef QTOPIA_HOST
        result = QString("/tmp/qtopia-%1/").arg(QString::number(qtopia_display_id()));
#else
    result = QDir::tempPath();
#endif

    QDir d( result );
    if ( !d.exists() ) {
#ifndef Q_OS_WIN
        mkdir(result.toLatin1(), 0700);
#else
        d.mkdir(result);
#endif
    }

    return result;
}

/*!
  Returns a file name suitable for use as a temporary file using \a fname
  as a base. Currently returns the file name with "_new" inserted prior to
  the extension.

  No attempt is made to check that the file does not already exist.
*/
QString Qtopia::tempName(const QString &fname)
{
    QFileInfo fileInfo( fname );
    return fileInfo.absolutePath() + "/" + fileInfo.baseName() +
            "_new." + fileInfo.completeSuffix(); // No tr
}

/*!
  \fn QString Qtopia::applicationFileName(const QString& appname, const QString& filename)

  Returns the application data path for the application specific data
  file \a filename and the application executable named \a appname.

  Application specific data is stored in a separate directory per
  application.

  The application specific directory will be created if if does not exist.
  It is a requirement that the user has write permissions for \l Qtopia::homePath().

  In the case of a package installed under SXE, then typically that package
  will not have access to \l Qtopia::homePath().  Instead the package is
  sandboxed into an application specific directory given by Qtopia::sandboxDir().
  In that case this method returns a path under the sandbox directory.

  If \a filename contains "/", it is the caller's responsibility to
  ensure that those directories exist.
*/
QString Qtopia::applicationFileName(const QString& appname, const QString& filename)
{
    QDir d;
    QString r = Qtopia::homePath() + "/Applications/";
#ifndef QT_NO_SXE
    QString sandbox = Qtopia::sandboxDir();
    if ( !sandbox.isEmpty() )
        r = sandbox;
#endif
    if ( !QFile::exists( r ) )
        if ( d.mkdir(r) == false ) {
            qFatal( "Cannot write to %s", r.toLatin1().constData() );
            return QString();
        }
    r += appname;
    if ( !QFile::exists( r ) )
        if ( d.mkdir(r) == false ) {
            qFatal( "Cannot write to %s", r.toLatin1().constData() );
            return QString();
        }
    r += "/"; r += filename;

    return r;
}

/*!
  Find the full path to the directory into which the current process is
  sandboxed under SXE.  The current process will have full read and write
  permissions in this directory.

  \image package.png

  This diagram shows a typical layout for the sandbox resulting from an
  SXE package install.

  In this case the result returned by sandboxDir() would be
  \code
    /opt/Qtopia.user/packages/a5b25e67a57f14de56
  \endcode

  If SXE is disabled, this function returns the empty string
*/
QString Qtopia::sandboxDir()
{
#ifndef QT_NO_SXE
    QString appPath = QCoreApplication::applicationFilePath();
    if ( appPath.startsWith( Qtopia::packagePath() ) )
        return appPath.left( Qtopia::packagePath().length() + 32 )
                + QLatin1String("/"); //32 is the md5sum length
    else
        return QString();
#else
    return QString();
#endif
}


/*!
  \fn bool Qtopia::isDocumentFileName(const QString& file)

  Returns true if \a file is the file name of a document ie, it resides under the locations
    described in storage.conf marked with both Documents=true and possibly with an extra
    document path for the location.
*/
bool Qtopia::isDocumentFileName(const QString& file)
{
#if !defined(QTOPIA_HOST) || defined(QTOPIA_CONTENT_INSTALLER)
    if ( file.right(1) == "/" )
        return false;
    const QFileSystem *fs=QStorageMetaInfo::instance()->fileSystemOf(file, true);
    if(fs != NULL && fs->documents() == true)
    {
        if(fs->documentsPath() != fs->path())
        {
            if(file.startsWith(fs->documentsPath()))
                return true;
        }
        else
            return true;
    }
#else
    Q_UNUSED( file );
#endif
    return false;
}

/*!  Returns true if keypad navigation is disabled and the application can expect the user to
  be able to easily produce \l {QMouseEvent}{mouse events}.

  Returns false if keypad navigation is enabled and the user \i cannot produce mouse events,
  or chooses not to (eg. does not plug in a mouse).

  Applications may use this to tune interactions.

  Note that you should only call this function after an instance of
  QtopiaApplication has been created. This function will return an undefined
  value if called prior to this.

  \sa QApplication::keypadNavigationEnabled()
*/
bool Qtopia::mousePreferred()
{
#ifdef QT_KEYPAD_NAVIGATION
    return !qApp->keypadNavigationEnabled(); //keypad phone
#else
    return true;
#endif
}

/*!
  Returns true if \a key is available on the device.  The buttons
  may be either fixed function buttons, such as Key_Menu, or user
  mappable buttons, such as Key_F1.

  \code
        if ( Qtopia::hasKey( Qt::Key_Flip ) ) {
            ... // add flip key function
        }
  \endcode

  \sa Qt::Key, QSoftMenuBar, QDeviceButtonManager
 */
bool Qtopia::hasKey(int key)
{
    static QList<int> *buttons = 0;

    if (!buttons) {
        buttons = new QList<int>;
        QSettings cfg(Qtopia::defaultButtonsFile(), QSettings::IniFormat);
        cfg.beginGroup("SystemButtons");
        if (cfg.contains("Count")) {
            int count = cfg.value("Count", 0).toInt();
            if (count) {
                for (int i = 0; i < count; i++) {
                    QString is = QString::number(i);
                    buttons->append(QKeySequence(cfg.value("Key"+is).toString())[0]);
                }
            }
        }
    }

    return (*buttons).contains(key);
}

/*!
    \obsolete
    Executes the application identified by \a app, passing \a document if it isn't null.

    QContent should be used to execute applications instead.

    A specific application may be launched by calling the \l {QContent::execute()}{execute()}
    member of a QContent object representing the application.

    \code
    QContent content(app);
    content.execute();
    \endcode

    To open a document using a specific application the file name of the document should be passed
    as the first argument of QContent::execute().

    \code
    QContent content(app);
    content.execute(QStringList() << document);
    \endcode

    A document may be opened in the default application for its type by calling the
    \l {QContent::execute()}{execute()} member of a QContent object representing the document.

    \code
    QContent content(document);
    content.execute();
    \endcode

    \sa QContent::execute()
*/
#ifndef QTOPIA_HOST
void Qtopia::execute( const QString &app, const QString& document )
{
    if ( document.isNull() ) {
        QtopiaServiceRequest e( "Launcher", "execute(QString)" );
        e << app;
        e.send();
    } else {
        QtopiaServiceRequest e( "Launcher", "execute(QString,QString)" );
        e << app << document;
        e.send();
    }
}
#endif

/*!
  Returns the string \a s with the characters '\', '"', and '$' quoted
  by a preceding '\', and enclosed by double-quotes (").

  \sa stringQuote()
*/
QString Qtopia::shellQuote(const QString& s)
{
    QString r="\"";
    for (int i=0; i<(int)s.length(); i++) {
        char c = s[i].toLatin1();
        switch (c) {
            case '\\': case '"': case '$':
                r+="\\";
        }
    r += s[i];
    }
    r += "\"";
    return r;
}

/*!
  Returns the string \a s with the characters '\' and '"' quoted by a
  preceding '\'.

  \sa shellQuote()
*/
QString Qtopia::stringQuote(const QString& s)
{
    QString r="\"";
    for (int i=0; i<(int)s.length(); i++) {
        char c = s[i].toLatin1();
        switch (c) {
            case '\\': case '"':
                r+="\\";
        }
        r += s[i];
    }
    r += "\"";
    return r;
}


/*!
  Returns true if the user regards their week as starting on Monday.
  Returns false if the user regards their week as starting on Sunday.
*/
bool Qtopia::weekStartsOnMonday()
{
    QSettings config("Trolltech","qpe");
    config.beginGroup( "Time" );
    return config.value("MONDAY", true).toBool();
}

/*!
  Sets the day the user regards their week starting on.
  If \a v is true, then the week begins on Monday.
  If \a v is false, then the week begins on Sunday.
*/
void Qtopia::setWeekStartsOnMonday(bool v)
{
    QSettings config("Trolltech","qpe");
    config.beginGroup( "Time" );
    config.setValue( "MONDAY", v );
}



static bool fontCanDisplayString(const QString &s, const QFont &f)
{
    QFontMetrics metrics(f);
    for (int i=0; i<s.size(); i++) {
        if (!metrics.inFont(s.at(i)))
            return false;
    }
    return true;
}

/*!
  Finds a font that can be used to display the string \a s and returns
  a variant with this font value. If the string cannot be displayed by any of
  the installed fonts, an invalid variant is returned.

  If \a s is empty, this returns a variant with the default font.
*/
QVariant Qtopia::findDisplayFont(const QString &s)
{
    QFont defaultFont;

    if (s.isEmpty())
        return QVariant(defaultFont);

    // check the default font first
    if (fontCanDisplayString(s, defaultFont))
        return QVariant(defaultFont);

    QString defaultFamily = defaultFont.family();
    QFontDatabase fontDb;
    QStringList families = fontDb.families();
    for (int i=0; i<families.size(); i++) {
        QString family = families.at(i);
        QFont currentFont(family);
        if (family != defaultFamily && fontCanDisplayString(s, currentFont)) {
            // use this font
            return QVariant(currentFont);
        }
    }

    return QVariant();
}



extern bool mkdirRecursive( QString path );

#ifndef QTOPIA_HOST

static bool docDirCreated = false;
static QDawg* fixed_dawg = 0;
static QMap<QString,QDawg*> *named_dawg = 0;

static void clean_fixed_dawg()
{
    if ( fixed_dawg ) {
        delete fixed_dawg;
        fixed_dawg = 0;
    }
}

static void clean_named_dawg()
{
    if ( named_dawg ) {
        QList<QString> keys = named_dawg->keys();
        foreach ( QString k, keys ) {
            QDawg* dawg = named_dawg->take( k );
            if ( dawg )
                delete dawg;
        }
        delete named_dawg;
        named_dawg = 0;
    }
}

static QString dictDir()
{
    // Directory for fixed dawgs
    return Qtopia::qtopiaDir() + "etc/dict";
}

void qtopia_load_fixedwords(QDawg* dawg, const QString& dictname, const QString& l)
{
    QString words_lang;
    QString basename = dictDir() + '/';
    //non-translatable dictionaries
    QString dawgfilename = basename + dictname + ".dawg";

    if ( l.isEmpty() ) {
        QStringList langs = Qtopia::languageList();
        foreach( QString lang, langs ) {
            words_lang = basename + lang + '/' + dictname;
            QString dawgfilename_lang = words_lang + ".dawg";
            if ( QFile::exists(dawgfilename_lang) ||
                    QFile::exists(words_lang) ) {
                dawgfilename = dawgfilename_lang;
                break;
            }
        }
    } else {
        words_lang = basename + l + '/' + dictname;
        QString dawgfilename_lang = words_lang + ".dawg";
        if ( QFile::exists(dawgfilename_lang) ||
                QFile::exists(words_lang) ) {
            dawgfilename = dawgfilename_lang;
        }
    }
    if ( dawgfilename.isEmpty() || !QFile::exists(dawgfilename) ) {
        // Not recommended to generate the dawgs from the word lists
        // on a device (slow), but we put it here so eg. SDK can easily
        // generate dawgs as required.
        QString fn = QFile::exists(words_lang) ? words_lang : QString();
        if ( fn.isEmpty() )
            return;
        qWarning("Generating '%s' dawg from word list.", fn.toLatin1().constData());
        QFile in(fn);
        QFile dawgfile(fn+".dawg");
        if ( in.open(QIODevice::ReadOnly) && dawgfile.open(QIODevice::WriteOnly) ) {
            dawg->createFromWords(&in);
            dawg->write(&dawgfile);
            dawgfile.close();
        }
    } else {
        dawg->readFile(dawgfilename);
    }
}

void qtopia_createDocDir()
{
    if ( !docDirCreated ) {
        QDir d;
        if (!d.exists(Qtopia::documentDir().toLatin1())){
            docDirCreated = true;
            mkdir( Qtopia::documentDir().toLatin1(), 0755 );
        }else{
            docDirCreated = true;
        }
    }
}

static void setDawgWords(const QString& dictname, const QStringList& words)
{
    QDawg& d = (QDawg&)Qtopia::dawg(dictname);
    d.createFromWords(words);

    QString dawgfilename = Qtopia::applicationFileName("Dictionary", dictname) + ".dawg"; // No tr
    QString dawgfilenamenew = dawgfilename + ".new";
    QFile dawgfile(dawgfilenamenew);
    if ( dawgfile.open(QIODevice::WriteOnly) ) {
        d.write(&dawgfile);
        dawgfile.close();
        ::rename(QFile::encodeName(dawgfilenamenew), QFile::encodeName(dawgfilename)); // Cannot use QFile::rename() here as it wont overwrite dawgfilename
    }

    // Signal *other* processes to re-read.
    QtopiaIpcEnvelope e( "QPE/System", "wordsChanged(QString,int)" );
    e << dictname << (int)::getpid();
}


/*!
  Returns the unchangeable QDawg that contains general
  words for the current locale.

  \sa addedDawg()
*/
const QDawg& Qtopia::fixedDawg()
{
    if ( !fixed_dawg ) {
        if ( !docDirCreated )
            qtopia_createDocDir();

        fixed_dawg = new QDawg;
        qAddPostRoutine( clean_fixed_dawg );
        qtopia_load_fixedwords(fixed_dawg,"words", QString());
    }

    return *fixed_dawg;
}

/*!
  Returns the changeable QDawg that contains general
  words for the current locale.

  \sa fixedDawg(), addWords(), removeWords()
*/
const QDawg& Qtopia::addedDawg()
{
    return dawg("local"); // No tr
}

/*!
  Returns the QDawg with the given \a name.
  This is an application-specific word list.

  \a name should not contain "/". If \a name starts
  with "_", it is a read-only system word list.

  \a language allows the specific selection of a dictionary for
  a particular language. This option applies to read-only system
  word list only and is ignored otherwise. If no language is
  given the dictionary of the current language is loaded.

*/
const QDawg& Qtopia::dawg(const QString& name, const QString& language)
{
    QString augmentedName;
    if ( name[0] == '_' ) {
        augmentedName=name+QChar('.')+language;
    } else {
        augmentedName = name;
    };

    if ( !named_dawg ) {
        named_dawg = new QMap<QString,QDawg*>;
        qAddPostRoutine( clean_named_dawg );
    }
    QDawg* r = named_dawg->value(augmentedName);

    if ( !r ) {
        qtopia_createDocDir();
        r = new QDawg;
        (*named_dawg)[augmentedName] = r;

        if ( !r->root() ) {
            if ( augmentedName[0] == '_' ) {
                QString n = name.mid(1);
                qtopia_load_fixedwords(r, n, language);
            } else {
                Qtopia::qtopiaReloadWords(augmentedName);
            }
        }
    }

    return *r;
}

/*!
  Returns whether \a word is in the fixedDawg() or the addedDawg(), but not in
  \link dawg() dawg("deleted")\endlink.
*/
bool Qtopia::isWord(const QString& word)
{
    return !dawg("deleted").contains(word)
        && (fixedDawg().contains(word) ||
            addedDawg().contains(word));
}

/*!
  Adds \a wordlist to the addedDawg(). Words that are already in the "local"
  list are not added. Words that are in the "deleted" list are
  removed from that list.

  Note that the addition of words persists between program executions
  (they are saved in the dictionary files), so you should confirm the
  words with the user before adding them.

  This is a slow operation. Call it once with a large list rather than
  multiple times with a small list.
*/
void Qtopia::addWords(const QStringList& wordlist)
{
    QStringList toadd;
    QStringList toundel;
    QDawg& del = (QDawg&)dawg("deleted"); // No tr
    for (QStringList::ConstIterator it=wordlist.begin(); it!=wordlist.end(); ++it) {
        if ( del.contains(*it) )
            toundel.append(*it);
        else
            toadd.append(*it);
    }
    addWords("local",toadd); // No tr
    removeWords("deleted",toundel); // No tr
}

/*!
  Adds \a wordlist to the dawg() named \a dictname.

  Note that the addition of words persists between program executions
  (they are saved in the dictionary files), so you should confirm the
  words with the user before adding them.

  This is a slow operation. Call it once with a large list rather than
  multiple times with a small list.
*/
void Qtopia::addWords(const QString& dictname, const QStringList& wordlist)
{
    if ( wordlist.isEmpty() )
        return;
    QDawg& d = (QDawg&)dawg(dictname);
    QStringList all = d.allWords() + wordlist;
    setDawgWords(dictname,all);
}

/*!
  Removes \a wordlist from the addedDawg(). If the words are in
  the fixed dictionary, they are added to the "deleted" dictionary.

  This is a slow operation. Call it once with a large list rather than
  multiple times with a small list.
*/
void Qtopia::removeWords(const QStringList& wordlist)
{
    if ( wordlist.isEmpty() )
        return;
    QDawg& d = (QDawg&)dawg("local");
    QStringList loc = d.allWords();
    int nloc = loc.count();
    QStringList del;
    for (QStringList::ConstIterator it=wordlist.begin(); it!=wordlist.end(); ++it) {
        loc.removeAll(*it);
        if ( fixedDawg().contains(*it) )
            del.append(*it);
    }
    if ( nloc != (int)loc.count() )
        setDawgWords("local",loc);
    addWords("deleted",del);
}

/*!
  Removes \a wordlist from the dawg() named \a dictname.

  This is a slow operation. Call it once with a large list rather than
  multiple times with a small list.
*/
void Qtopia::removeWords(const QString& dictname, const QStringList& wordlist)
{
    QDawg& d = (QDawg&)dawg(dictname);
    QStringList all = d.allWords();
    for (QStringList::ConstIterator it=wordlist.begin(); it!=wordlist.end(); ++it)
        all.removeAll(*it);
    setDawgWords(dictname,all);
}

/*!
  Reloads the QDawg named called \a dictname. This must be called if
  the underlying dawg files are changed, otherwise previous versions will be used.

  Not normally used.

  \sa dawg()
*/
void Qtopia::qtopiaReloadWords(const QString& dictname)
{
    // Reload dictname, if we have it loaded.
    if ( named_dawg ) {
        QDawg* r = named_dawg->value(dictname);
        if ( r ) {
            QString dawgfilename = Qtopia::applicationFileName("Dictionary", dictname) + ".dawg"; // No tr
            QFile dawgfile(dawgfilename);
            if ( dawgfile.open(QIODevice::ReadOnly) )
                r->readFile(dawgfilename);
        }
    }
}

/*!
  \enum Qtopia::ItemDataRole

  Each item in the model has a set of data elements associated with it,
  each with its own role. The roles are used by the view to indicate to
  the model which type of data it needs.

  Qt Extended adds the following roles:

  \value AdditionalDecorationRole An additional decoration to display status, for example.
  \value UserRole The first role that can be used for application-specific purposes.

  \sa Qt::ItemDataRole
*/


/*!
  Schedules an alarm to go off at (or soon after) time \a when. When
  the alarm goes off, the \l {Qt Extended IPC Layer}{Qt Extended IPC} \a message will
  be sent to \a channel, with \a data as a parameter.

  If \a channel does not contain the \c{/} character, it will be interpreted
  as a QCop service name.  For example, specifying \c{Calendar} as \a channel
  will direct the alarm to the configured calendar program.

  Note that these alarms fire regardless of whether the application that
  created them is running, indeed for some hardware, even if the device
  is not powered on.

  If this function is called with exactly the same data as a previous
  call the subsequent call is ignored, so there is only ever one alarm
  with a given set of parameters.

  \sa deleteAlarm()
*/
void Qtopia::addAlarm ( QDateTime when, const QString& channel,
                             const QString& message, int data)
{
    QtopiaIpcEnvelope e( "QPE/AlarmServer", "addAlarm(QDateTime,QString,QString,int)" );
    e << when << channel << message << data;
}


/*!
  Deletes previously scheduled alarms which match \a when, \a channel,
  \a message, and \a data.

  Passing null values for \a when, \a channel, or for the \l {Qt Extended IPC Layer}{Qt Extended IPC} \a message, acts as a wildcard meaning "any".
  Similarly, passing -1 for \a data indicates "any".

  If there is no matching alarm, nothing happens.

  \sa addAlarm()

*/
void Qtopia::deleteAlarm (QDateTime when, const QString& channel, const QString& message, int data)
{
    QtopiaIpcEnvelope e( "QPE/AlarmServer", "deleteAlarm(QDateTime,QString,QString,int)" );
    e << when << channel << message << data;
}

/*!
  Writes the system clock to the hardware clock.
*/
void Qtopia::writeHWClock()
{
    system("/sbin/hwclock -w"); // ##### UTC?
}

/*!
  Returns current real time clock time (RTC). This function is implemented by
  reading /sys/class/rtc/rtc0/since_epoch file.
  */
QDateTime Qtopia::rtcNow()
{
    QByteArray secsStr = readFile("/sys/class/rtc/rtc0/since_epoch").trimmed();
    uint secs = secsStr.toUInt();
    return QDateTime::fromTime_t(secs);
}

/*!
  Write n bytes from \a buf to file descriptor \a fd. On success returns
  \a okRes on error return \a errRes. You can check errno in this case.
  */
int Qtopia::writeFd(int fd, const char *buf, int n, int okRes, int errRes)
{
    for (;;) {
        int count = write(fd, buf, n);
        if (count <= 0) {
            if(errno == EAGAIN) {
                usleep(1000);
                continue;
            }
            return errRes;
        }
        buf += count;
        n -= count;
        if (n <= 0)
            return okRes;
    }
}

/*!
  Writes \a n bytes from \a buf to file specified by \a path. In case of error
  if \a warnOnError is true qWarning() is used to print the error message. On
  success returns \a okRes on errror return \a errRes.
  */
int Qtopia::writeFile(const char * path, const char * buf, int n, bool warnOnError, int okRes, int errRes)
{
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 00644);
    if (fd >= 0 && writeFd(fd, buf, n)) {
        close(fd);
        return okRes;
    }
    if(warnOnError)
        qWarning() << "writeFile failed " << path << ":" << strerror(errno);

    if(fd >= 0)
        close(fd);
    
    return errRes;
}

/*!
  Reads contents of file specified by \a path.
  */
QByteArray Qtopia::readFile(const char *path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning() << "readFile failed" << path << ":" << f.errorString();
        return QByteArray();
    }
    QByteArray content = f.readAll();
    f.close();
    return content;
}

#endif //QTOPIA_HOST

#include "qtopianamespace_lock.cpp"

