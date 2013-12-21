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

#ifndef QTOPIANAMESPACE_H
#define QTOPIANAMESPACE_H

#include <qtopiaglobal.h>

#include <QApplication>
#include <QMessageBox>
#include <QStringList>
#include <QString>
#include <QDir>
#include <QRegExp>
#include <QList>
#include <QTranslator>
#include <QSettings>

class QContentSet;
class QDawg;

// syncqtopia header Qtopia
namespace Qtopia
{
    /*

    Global functions

    */

    QTOPIABASE_EXPORT QStringList installPaths();
    QTOPIABASE_EXPORT QString updateDir();
    QTOPIABASE_EXPORT QString qtopiaDir();
    QTOPIABASE_EXPORT QString packagePath();
    QTOPIABASE_EXPORT QString documentDir();
    QTOPIABASE_EXPORT QString defaultButtonsFile();
    QTOPIABASE_EXPORT QString homePath();
    QTOPIABASE_EXPORT QStringList helpPaths();

    QTOPIABASE_EXPORT bool truncateFile(QFile &f, int size);
    QTOPIABASE_EXPORT QString tempDir( );
    QTOPIABASE_EXPORT QString tempName(const QString &filename);

    QTOPIABASE_EXPORT QString sandboxDir();
    QTOPIABASE_EXPORT QString applicationFileName(const QString& appname, const QString& filename);

    QTOPIABASE_EXPORT bool isDocumentFileName(const QString& file);

    enum Lockflags {LockShare = 1, LockWrite = 2, LockBlock = 4};
    QTOPIABASE_EXPORT bool lockFile(QFile &f, int flags = -1);
    QTOPIABASE_EXPORT bool unlockFile(QFile &f);
    QTOPIABASE_EXPORT bool isFileLocked(QFile &f, int flags = -1);

    QTOPIABASE_EXPORT int writeFd(int fd, const char *buf, int n, int okRes = 1, int errRes = 0);
    QTOPIABASE_EXPORT int writeFile(const char * path, const char * buf, int n, bool warnOnError = true, int okRes = 1, int errRes = 0);
    
    QTOPIABASE_EXPORT QByteArray readFile(const char *path);
    QTOPIABASE_EXPORT int readFd(int fd, char *buf, int bufLen, int errRes = 0);
    QTOPIABASE_EXPORT int readFile(const char *path, char *buf, int bufLen, bool warnOnError, int errRes);
    QTOPIABASE_EXPORT int readSysfsStr(const char *path, char *buf, int bufLen, bool warnOnError = true, int errRes = 0);
    QTOPIABASE_EXPORT int readSysfsInt(const char *path, bool warnOnError = true, int errRes = 0);

    // Convenient macros (shorter name), writeFile for writing string constant
    #define qWriteFile(path, buf) Qtopia::writeFile(path, buf, sizeof(buf) - 1)
    #define qReadFile(path) Qtopia::readFile(path)
    #define qReadSysfsStr(path, buf) Qtopia::readSysfsStr(path, buf, sizeof(buf))
    #define qReadSysfsInt(path) Qtopia::readSysfsInt(path)
    
    QTOPIABASE_EXPORT bool mousePreferred();
    QTOPIABASE_EXPORT bool hasKey(int key);

#ifndef QTOPIA_CONTENT_INSTALLER
    QTOPIABASE_EXPORT void execute(const QString &exec, const QString &document=QString());
#endif

    /*

    User interface segment

    */

    QTOPIABASE_EXPORT bool confirmDelete(QWidget *parent, const QString &caption, const QString &object);
    QTOPIABASE_EXPORT void actionConfirmation(const QPixmap &pix, const QString &text);
    QTOPIABASE_EXPORT void soundAlarm();
    QTOPIABASE_EXPORT void statusMessage(const QString&);

    /*

    Localization functions

    */

    QTOPIABASE_EXPORT QString translate(const QString& key, const QString& c, const QString& str);
    QTOPIABASE_EXPORT QStringList languageList();

    QTOPIABASE_EXPORT bool weekStartsOnMonday();
    QTOPIABASE_EXPORT void setWeekStartsOnMonday(bool );

    QTOPIABASE_EXPORT QVariant findDisplayFont(const QString &s);

    /*

    Real-time clock functions

    */
    QTOPIABASE_EXPORT void addAlarm ( QDateTime when, const QString& channel, const QString& msg, int data=0);
    QTOPIABASE_EXPORT void deleteAlarm (QDateTime when, const QString& channel, const QString& msg, int data=0);
    QTOPIABASE_EXPORT void writeHWClock();
    QTOPIABASE_EXPORT QDateTime rtcNow();


    /*

    String manipulation utility functions

    */

    QTOPIABASE_EXPORT QString simplifyMultiLineSpace( const QString &multiLine );
    QTOPIABASE_EXPORT QString dehyphenate(const QString&);

    QTOPIABASE_EXPORT QString shellQuote(const QString& s);
    QTOPIABASE_EXPORT QString stringQuote(const QString& s);

    // System independant sleep
    QTOPIABASE_EXPORT void sleep(unsigned long secs);
    QTOPIABASE_EXPORT void msleep(unsigned long msecs);
    QTOPIABASE_EXPORT void usleep(unsigned long usecs);

    QTOPIABASE_EXPORT QString version();
    QTOPIABASE_EXPORT QString architecture();
    QTOPIABASE_EXPORT QString deviceId();
    QTOPIABASE_EXPORT QString ownerName();

    /*

    Dictionary  functions

    */

    // Dictionaries
    QTOPIABASE_EXPORT const QDawg& fixedDawg();
    QTOPIABASE_EXPORT const QDawg& addedDawg();
    QTOPIABASE_EXPORT const QDawg& dawg(const QString& name, const QString& language = QString());

    QTOPIABASE_EXPORT bool isWord(const QString& word);
    QTOPIABASE_EXPORT void addWords(const QStringList& words);
    QTOPIABASE_EXPORT void addWords(const QString& dictname, const QStringList& words);
    QTOPIABASE_EXPORT void removeWords(const QStringList& words);
    QTOPIABASE_EXPORT void removeWords(const QString& dictname, const QStringList& words);
    QTOPIABASE_EXPORT void qtopiaReloadWords( const QString& dictname );

    /*

    Qt Extended enums

    */
    enum ItemDataRole {
        AdditionalDecorationRole = 1000,
        UserRole = 2000
    };

    /*

    Special Qt Extended keys.

    */
    static const Qt::Key Key_Headset = Qt::Key_F5;
    static const Qt::Key Key_Speaker = Qt::Key_F6;
    static const Qt::Key Key_Hook = Qt::Key_F27;
    static const Qt::Key Key_HeadsetButton = Qt::Key_F28;
    static const Qt::Key Key_Lock = Qt::Key_F29;

}
#endif
