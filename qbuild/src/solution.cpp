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

/*!
  \overview Solution Filesystem
  \brief Describes how the solution filesystem works.

  QBuild builds a single, virtual filesystem into which it maps one or more real locations.

  For example, it is typical to have a solution filesystem with mounts like this:
  \table
  \header \o Real path \o Solution path
  \row \o /home/user/build/qtopia/4.4 \o /
  \row \o /home/user/src/qtopia/4.4/devices/mydevice/src \o /src
  \row \o /home/user/src/qtopia/4.4 \o /
  \row \o /home/user/src/qtopia/4.4/qbuild/extensions \o /extensions
  \endtable

  Files in QBuild are specified using their solution path. Using a test like \c{exists(/src/foo)}
  searches for the file in the following real locations:
  \list
  \o /home/user/build/qtopia/4.4/src/foo
  \o /home/user/src/qtopia/4.4/devices/mydevice/src/foo
  \o /home/user/src/qtopia/4.4/src/foo
  \endlist

  The order of the mounts is important because files will be returned from the first location that they
  exist in. This means that "override" locations should appear before "fallback" locations. It also means
  that the "output" location is always listed first.

  Note that QBuild can reference files that do not live within the solution filesystem. These files are
  handled by the \l SolutionFile class.

  \sa qbuild.solution

*/

/*
   When qmake needs a file location, it asks the Solution object by calling
   the Solution::findFile() method.  The location request consists of three
   parts - the path as provided, the type of the location request and,
   optionally, the file that is requesting it.  The path as provided is simply
   the string, either absolute or relative, of the location requested.

   The type of the location request determines where the project file system
   searches for the location.  If Project, the location is assumed to be a
   read-only, non-generated (that is, exists prior to any build activity) file.
   Source files, for example, usually fall under this category.  The entire file
   hierarchy, including the build hierarchy, is searched for the file.
   Generated locations, on the other hand, are usually created during the build.
   Only the build hierarchy is used to generate the file location, the rest of
   the file hierarchy is NOT searched.

   The file that is requesting the path is further used to resolve relative
   locations and refine the search.  If the requesting file is not provided,
   relative paths are resolved against the current working directory.  If it is
   provided, the location of the requesting file (in the project filesystem) is
   used.  If the requesting file is from a relocated sub-project, the
   sub-project file system is first searched for the location as thought it was
   not relocated before the true project file system is searched.
 */

#include "solution.h"
#include "project.h"
#include "qoutput.h"
#include <QDir>
#include <QThread>
#include <QWaitCondition>
#include <QMutex>
#include <QCoreApplication>
#include <QSettings>
#include "functionprovider.h"
#include "qfastdir.h"
#include <fnmatch.h>
#include <QThread>
#include "options.h"
#include "qbuild.h"

/*!
  \class SolutionDir
  \brief The SolutionDir class represents a directory in the solution filesystem.
*/

// define PathIterator
PathIterator::PathIterator()
: _solution(0), _isValid(false)
{
}

PathIterator::PathIterator(const PathIterator &o)
: _absPath(o._absPath), _solution(o._solution), _isValid(o._isValid)
{
}

PathIterator &PathIterator::operator=(const PathIterator &o)
{
    _absPath = o._absPath;
    _solution = o._solution;
    _isValid = o._isValid;
    return *this;
}

PathIterator::PathIterator(const Solution *solution)
: _absPath("/"), _solution(solution), _isValid(true)
{
    Q_ASSERT(_solution);
}

PathIterator::PathIterator(const QString &absPath)
: _absPath(absPath), _solution(0), _isValid(true)
{
    if (!_absPath.endsWith('/'))
        _absPath.append('/');
    if (!QFastDir::isDir(_absPath)) {
        _absPath.clear();
        _isValid = false;
    }
}

Solution* PathIterator::solution() const
{
    return const_cast<Solution *>(_solution);
}

PathIterator::Files PathIterator::files(const QByteArray &_match) const
{
    if (!isValid())
        return Files();

    const char *match = 0;
    if (!_match.isEmpty())
        match = _match.constData();

    if (_solution) {
        PathIterator::Files rv;

        for (int ii = 0; ii < _solution->m_paths.count(); ++ii) {
            SolutionDir *sd = _solution->m_paths.at(ii);
            QString mappedDir = sd->mappedPath(_absPath);
            if (!mappedDir.isEmpty()) {
                QFastDir fd(mappedDir);
                QStringList files = fd.files();
                foreach(QString file, files) {
                    if (match && 0 != ::fnmatch(match, file.toLatin1().constData(), 0))
                        continue;

                    SolutionFile f;
                    f.m_fs = _solution;
                    f.m_path = mappedDir + "/" + file;
                    f.m_request = _absPath + file;

                    if (0 == ii && _solution->m_paths.count() > 1)
                        f.m_type = SolutionFile::Build;
                    else if (0 == ii)
                        f.m_type = (SolutionFile::Type)(SolutionFile::Project | SolutionFile::Build);
                    else
                        f.m_type = SolutionFile::Project;

                    rv << f;
                }
            }
        }

        return rv;
    } else {
        QFastDir fd(_absPath);
        PathIterator::Files rv;
        foreach(QString file, fd.files()) {
            if (match && 0 != ::fnmatch(match, file.toLatin1().constData(), 0))
                continue;
            SolutionFile f;
            f.m_type = SolutionFile::Absolute;
            f.m_fs = 0;
            f.m_request = _absPath + file;
            f.m_path = f.m_request;
            rv << f;
        }
        return rv;
    }
}

PathIterator::Files PathIterator::files() const
{
    return files(QByteArray());
}

QString PathIterator::path() const
{
    if (!isValid())
        return QString();

    return _absPath.left(_absPath.length() - 1); // remove trailing '/'
}

QStringList PathIterator::fsPath() const
{
    if (_solution) {
        QStringList rv;
        foreach(SolutionDir *sd, _solution->m_paths) {
            QString mappedDir = sd->mappedPath(_absPath);
            if (!mappedDir.isEmpty())
                rv << mappedDir;
        }
        return rv;
    } else {
        return QStringList() << path();
    }
}

QStringList PathIterator::paths(const QByteArray &_match) const
{
    if (!isValid())
        return QStringList();

    const char *match = 0;
    if (!_match.isEmpty())
        match = _match.constData();

    if (_solution) {
        QSet<QString> dirs;

        foreach(SolutionDir *sd, _solution->m_paths) {
            QString mappedDir = sd->mappedPath(_absPath);
            if (!mappedDir.isEmpty()) {
                QFastDir fd(mappedDir);
                foreach(QString dir, fd.dirs()) {
                    if (match && 0 != ::fnmatch(match, dir.toLatin1().constData(), 0))
                        continue;

                    dirs.insert(dir);
                }
            }
        }

        return dirs.toList();
    } else {
        QFastDir fd(_absPath);
        QStringList rv;
        foreach(QString dir, fd.dirs()) {
            if (match && 0 != ::fnmatch(match, dir.toLatin1().constData(), 0))
                continue;

            rv << dir;
        }

        return rv;
    }
}

QStringList PathIterator::paths() const
{
    return paths(QByteArray());
}

PathIterator PathIterator::up() const
{
    if (!isValid())
        return PathIterator();

    PathIterator rv = *this;
    QString path = _absPath;
    int idx = path.lastIndexOf('/', path.length() - 1);
    if (idx != -1)
        rv._absPath = path.left(idx + 1);
    return rv;
}

PathIterator PathIterator::advance(const QString &path) const
{
    if (!isValid())
        return PathIterator();

    QString _path = path;
    if (_path.startsWith('/'))
        _path = _path.mid(1);

    if (_solution) {
        PathIterator rv(_solution);
        rv._absPath = _absPath + _path;
        if (!rv._absPath.endsWith('/'))
            rv._absPath.append('/');
        return rv;

    } else {
        PathIterator iter = PathIterator(_absPath + _path);
        return iter;
    }
}

bool PathIterator::isValid() const
{
    return _isValid;
}

/*!
  \class SolutionFile
  \brief The SolutionFile class represents a file in the solution filesystem.
*/

/*!
  \enum SolutionFile::Type
  \value Project
  \value Absolute
  \value Build
*/

/*!
  \internal
*/
SolutionFile::SolutionFile()
: m_type(Absolute), m_fs(0), m_isDir(false)
{
}

/*!
  \internal
*/
SolutionFile::SolutionFile(const SolutionFile &other)
: m_type(other.m_type), m_fs(other.m_fs), m_request(other.m_request),
  m_path(other.m_path), m_isDir(other.m_isDir)
{
}

/*!
  \internal
*/
SolutionFile &SolutionFile::operator=(const SolutionFile &other)
{
    m_type = other.m_type;
    m_fs = other.m_fs;
    m_request = other.m_request;
    m_path = other.m_path;
    m_isDir = other.m_isDir;
    return *this;
}

/*!
  Returns true if the encapsulated file is valid, false otherwise.
*/
bool SolutionFile::isValid() const
{
    return !m_request.isEmpty();
}

/*!
  Returns the solution that this file came from.
*/
Solution *SolutionFile::solution() const
{
    return const_cast<Solution *>(m_fs);
}

/*!
  Returns true if the encapsulated file is a directory, false otherwise.
*/
bool SolutionFile::isDir() const
{
    return m_isDir;
}

/*!
  Returns the path type of the file.
*/
SolutionFile::Type SolutionFile::type() const
{
    return m_type;
}

/*!
  Returns the name of the encapsulated file.
*/
QString SolutionFile::name() const
{
    int idx = m_request.lastIndexOf('/');
    if (-1 == idx)
        return m_request;
    else
        return m_request.mid(idx + 1);
}

/*!
  Returns the solution path of the encapsulated file.
*/
QString SolutionFile::solutionPath() const
{
    return m_request;
}

/*!
  Returns the solution directory that the encapsulated file is in.
*/
QString SolutionFile::solutionDir() const
{
    if (isDir())
        return solutionPath();

    QString rv = m_request;
    int idx = rv.lastIndexOf('/');
    if (idx != -1)
        rv = rv.left(idx);
    return rv;
}

/*!
  Returns the filesystem directory that the encapsulated file is in.
*/
QString SolutionFile::fsDir() const
{
    if (isDir())
        return fsPath();

    QString rv = m_path;
    int idx = rv.lastIndexOf('/');
    if (idx != -1)
        rv = rv.left(idx);
    return rv;
}

/*!
  Returns the filesystem path to the encapsulated file.
*/
QString SolutionFile::fsPath() const
{
    return m_path;
}

/*!
  Returns a SolutionFile object that has the canonical path.
*/
SolutionFile SolutionFile::canonicalPath() const
{
    SolutionFile ret = (*this);
    QFileInfo file_info = QFileInfo(m_path);
    QString dir = file_info.canonicalPath();
    //if ( !dir.isEmpty() ) {
    if ( !dir.isEmpty() && (dir != ".") ) { // see https://bugreports.qt-project.org/browse/QTBUG-25537
        ret.m_path = dir+"/"+file_info.fileName();
        ret.m_request = solution()->fuzzyRealToSolution(ret.m_path).m_request;
    }
    return ret;
}

/*! \internal */
QDebug &operator<<(QDebug &d, const SolutionFile &file)
{
    if (file.isDir())
        d << "Directory(";
    else
        d << "File(";

    d << "Type: ";
    switch(file.type()) {
        case SolutionFile::Absolute:
            d << "Absolute";
            break;
        case SolutionFile::Project:
            d << "Project";
            break;
        case SolutionFile::Build:
            d << "Build";
            break;
    };
    d << ", Path:";
    d << file.solutionPath();
    d << ", Fs Path:";
    d << file.fsPath();
    d << ")";

    return d;
}
/*!
  \internal
*/
SolutionDir::SolutionDir(const QString &location, const QString &path)
: m_location(location), m_path(path)
{
    if (!m_location.isEmpty() && !m_location.startsWith(QLatin1String("/")))
        m_location.prepend(QChar('/'));
}

/*!
  \internal
*/
const QString &SolutionDir::location() const
{
    return m_location;
}

/*!
  \internal
*/
const QString &SolutionDir::path() const
{
    return m_path;
}

/*!
  \internal
*/
QStringList SolutionDir::paths(const QString &req) const
{
    QString mapped = mappedPath(req);
    if (!mapped.isEmpty()) {
        return QFastDir(mapped).dirs();
    } else {
        return QStringList();
    }
}

/*!
  \internal
*/
QStringList SolutionDir::files(const QString &req) const
{
    QString mapped = mappedPath(req);
    if (!mapped.isEmpty()) {
        return QFastDir(mapped).files();
    } else {
        return QStringList();
    }
}

/*!
  \internal
*/
QString SolutionDir::mappedPath(const QString &req) const
{
    if (m_location.isEmpty() ||
       (req.startsWith(m_location) && req.length() > m_location.length() &&
        req.at(m_location.length()) == QChar('/'))) {

        QString tmp_mappedPath = m_path + QChar('/') +
            (m_location.isEmpty()?req:req.mid(m_location.length()));

        return QDir::cleanPath(tmp_mappedPath);
    }

    return QString();
}

/*!
  \internal
*/
bool SolutionDir::findFile(const QString &req, QString &rv) const
{
    QString tmp_mappedPath = this->mappedPath(req);
    if (!tmp_mappedPath.isEmpty() && QFastDir::exists(tmp_mappedPath))
    {
        rv = tmp_mappedPath;
        return true;
    } else {
        return false;
    }
}

/*!
  \class Solution
  \brief The Solution class represents a solution.

  \sa {Solution Filesystem}
*/

/*!
  \enum Solution::FileType

  This enum is used to locate files in one of three specific locations.

  \value Generated
  \value Project
  \value Existing
*/

/*!
  \enum Solution::Type

  \value None
  \value _Project
  \value Absolute
  \value Wildcard
  \value All
*/

Solution *Solution::m_defaultSolution = 0;
QHash<QString, Solution *> Solution::m_solutions;

/*!
  Returns the solution with name \a name.
*/
Solution *Solution::solution(const QString &name)
{
    if ("default" == name)
        return defaultSolution();

    QHash<QString, Solution *>::Iterator iter = m_solutions.find(name);
    if (iter == m_solutions.end())
        return 0;
    else
        return *iter;
}

/*!
  Set the default solution to \a solution.
*/
void Solution::setDefaultSolution(Solution *solution)
{
    m_defaultSolution = solution;
}

/*!
  Create the default solution. Returns true.
*/
bool Solution::createDefaultSolution()
{
    SolutionDescriptor desc;
    Solution *defaultSolution = 0;
    if (SolutionDescriptor::findSolutionDescriptor(QDir::currentPath(), desc)) {

        Solution *actualDefault = 0;
        foreach(QString solution, desc.solutions()) {
            Solution *sol = new Solution(solution, desc);
            if (desc.defaultSolution() == solution)
                actualDefault = sol;
            else
                defaultSolution = sol;
        }
        if (actualDefault)
            defaultSolution = actualDefault;
    } else {
        defaultSolution = new Solution("default", QDir::currentPath());
    }

    m_defaultSolution = defaultSolution;

    return true;
}

/*!
  Returns the default solution.
*/
Solution *Solution::defaultSolution()
{
    return m_defaultSolution;
}

/*!
  Returns the names of the solutions.
*/
QStringList Solution::solutions()
{
    return m_solutions.keys();
}

/*!
  \internal
*/
Solution::Solution(const QString &name,
                   const SolutionDescriptor &desc)
: m_name(name), m_includesPrimed(false)
{
    Q_ASSERT(!name.isEmpty());
    Q_ASSERT(!m_solutions.contains(name));
    Q_ASSERT(desc.solutions().contains(name));

    QList<SolutionDescriptor::Path> paths = desc.paths(name);

    foreach(SolutionDescriptor::Path path, paths)
        addPath(path.solutionPath, path.filesystemPath);

    m_solutions.insert(name, this);
}

/*!
  \internal
*/
Solution::Solution(const QString &name,
                   const QString &rootPath)
: m_name(name), m_includesPrimed(false)
{
    Q_ASSERT(!name.isEmpty());
    Q_ASSERT(!m_solutions.contains(name));

    SolutionDescriptor descriptor(rootPath);
    if (descriptor.defaultSolution().isEmpty()) {

        addPath(QString(), rootPath);

    } else {
        QList<SolutionDescriptor::Path> paths =
            descriptor.paths(descriptor.defaultSolution());

        foreach(SolutionDescriptor::Path path, paths)
            addPath(path.solutionPath, path.filesystemPath);
    }

    m_solutions.insert(name, this);
}

/*!
  Map \a filesystemPath to \a solutionPath.
*/
void Solution::addPath(const QString &solutionPath,
                       const QString &filesystemPath)
{
    m_paths << new SolutionDir(solutionPath, filesystemPath);
}

/*!
  \internal
*/
Solution::~Solution()
{
    if (name() == "default") {
        Q_ASSERT(m_defaultSolution == this);
        m_defaultSolution = 0;
    } else {
        Q_ASSERT(m_solutions[name()] == this);
        m_solutions.remove(name());
    }
}

/*!
  Return the name of the solution.
*/
QString Solution::name() const
{
    return m_name;
}

/*!
  \internal
*/
void Solution::dump()
{
    qWarning() << "Project Filesystem:";
    dump(1);
}

/*!
  \internal
  \class PathGlob
*/

PathGlob::PathGlob()
{
}

PathGlob::PathGlob(const QString &str)
{
    // Need to split str into a series of glob words
    bool isEscape = false;
    int prevWord = 0;
    QChar escape('\\');
    QChar sep('/');
    QChar star('*');
    QChar ques('?');
    QChar brac('[');

    bool seenGlob = false;
    for (int ii = 0; ii < str.length(); ++ii) {
        if (isEscape) {
            isEscape = false;
            continue;
        }

        if (str.at(ii) == escape) {
            isEscape = true;
        } else if (str.at(ii) == sep) {
            QString wrd = str.mid(prevWord, ii - prevWord);
            if (!wrd.isEmpty()) {
                words << wrd.toLatin1();
                consts << !seenGlob;
            }
            seenGlob = false;
            prevWord = ii + 1;
        } else if (!seenGlob && (str.at(ii) == star || str.at(ii) == ques || str.at(ii) == brac)) {
            seenGlob = true;
        }
    }
    QString wrd = str.mid(prevWord);
    if (!wrd.isEmpty()) {
        words << wrd.toLatin1();
        consts << !seenGlob;
    }
}

QByteArray PathGlob::glob(int depth) const
{
    Q_ASSERT(depth >= 0 && depth < words.count());
    return words.at(depth);
}

int PathGlob::depth() const
{
    return words.count();
}

bool PathGlob::isConst(int depth) const
{
    Q_ASSERT(depth >= 0 && depth < consts.count());
    return consts.at(depth);
}

bool PathGlob::match(const QString &str, int depth) const
{
    Q_ASSERT(depth >= 0 && depth < words.count());
    if (depth >= words.count())
        return false;

    return 0 ==
        ::fnmatch(words.at(depth).constData(), str.toLatin1().constData(), 0);
}

class SimpleWildCard
{
public:
    SimpleWildCard(const QString &);

    bool match(const QString &) const;

private:
    QStringList m_components;
};

SimpleWildCard::SimpleWildCard(const QString &str)
{
    m_components = str.split('*');
}

bool SimpleWildCard::match(const QString &str) const
{
    int idx = 0;

    for (int ii = 0; ii < m_components.count(); ++ii) {
        const QString &component = m_components.at(ii);

        if (component.isEmpty() && (ii + 1) == m_components.count()) {
            idx = str.length();
        } else if (m_components.isEmpty()) {
            continue;
        } else {
            idx = str.indexOf(component, idx);
            if (-1 == idx)
                return false;
            idx += component.length();
        }
    }

    return idx == str.length();
}

/*!
  Return a list of solution files that match \a id.  \a id must be an absolute
  path, and may contain the "*" wild card in the file section.  For example:

    /services/Date.service
    /services/D*.service
    /services/D*.*
    /services/

  are valid and

    services/Date.service
    /s*s/Date.service

  are not.
 */
QStringList Solution::files(const QString &id) const
{
    QString _id = id;
    bool abs = false;
    if (_id.startsWith('!')) {
        _id = _id.mid(1);
        abs = true;
    }

    if (!_id.startsWith('/'))
        _id.prepend('/');

    // Find path component
    int idx = _id.lastIndexOf('/');
    Q_ASSERT(idx != -1);
    QString file = _id.mid(idx + 1);
    QString path = _id.left(idx + 1);

    QSet<QString> set;

    SimpleWildCard swc(file);
    if (abs) {
        QFastDir dir(path);
        QStringList files = dir.files();
        for (int jj = 0; jj < files.count(); ++jj) {
            if (file.isEmpty() || swc.match(files.at(jj)))
                set.insert(files.at(jj));
        }
    } else {
        for (int ii = 0; ii < m_paths.count(); ++ii) {
            QStringList files = m_paths.at(ii)->files(path);

            for (int jj = 0; jj < files.count(); ++jj) {
                if (file.isEmpty() || swc.match(files.at(jj)))
                    set.insert(files.at(jj));
            }
        }
    }

    QStringList rv;
    foreach(QString str, set)
        rv << (abs?"!":"") + path + str;
    return rv;
}

/*!
   Returns a list of file system paths that map to the solution path \a id.
*/
QStringList Solution::pathMappings(const QString &id)
{
    QStringList rv;
    for (int ii = 0; ii < m_paths.count(); ++ii) {
        QString path = m_paths.at(ii)->mappedPath(id);
        if (!path.isEmpty())
            rv << path;
    }
    return rv;
}

/*!
  Returns a list of the file system paths that map to the solution path \a id.

  FIXME \a unused
*/
QStringList Solution::paths(const QString &id, const SolutionFile &unused) const
{
    Q_UNUSED(unused);

    QString path = id;
    if (!path.endsWith('/'))
        path.append('/');

    QSet<QString> set;

    // Add mappings themselves
    for (int ii = 0; ii < m_paths.count(); ++ii) {

        QString location;
        location = m_paths.at(ii)->location();

        if (location.startsWith(id)) {
            int index = location.indexOf('/', id.length());
            QString subPath = location.mid(id.length(), index);
            if (!subPath.isEmpty())
                set.insert(subPath);
        }
    }

    // Add path contents
    for (int ii = 0; ii < m_paths.count(); ++ii) {
        QStringList subPaths = m_paths.at(ii)->paths(path);
        for (int jj = 0; jj < subPaths.count(); ++jj)
            set.insert(subPaths.at(jj));
    }

    return set.toList();
}


/*!
  Returns the filesystem path of the build directory given by \a _name.  This path
  may not exist.
  If name is relative, it will be taken as relative to the \a relative file.  If
  no relative file is provided and name is relative, an empty string will
  be returned.
 */
QString Solution::filesystemBuildPath(const QString &_name,
                                      const SolutionFile &relative) const
{
    QString sln;
    QString name;
    if (checkNameForSolution(_name, sln, name)) {
        if (sln != "current") {
            Solution *s = Solution::solution(sln);
            if (!s)
                return QString();
            else
                return s->filesystemBuildPath(name, SolutionFile());
        }
    }

    bool isRelative = QFastDir::isRelativePath(name);
    if (isRelative && !relative.isValid())
        return QString();

    QString absPath;
    if (isRelative)
        absPath = relative.solutionDir() + "/" + name;
    else
        absPath = name;
    absPath = QDir::cleanPath(absPath);

    return QDir::cleanPath(m_paths.first()->path() + absPath);
}

/*!
  Returns a SolutionFile for the build file given by \a _name.  This file may
  not exist.
  If name is relative, it will be taken as relative to the \a relative file.  If
  no relative file is provided and name is relative, a null SolutionFile will
  be returned.
 */
SolutionFile Solution::buildFile(const QString &_name,
                                 const SolutionFile &relative) const
{
    QString sln;
    QString name;
    if (checkNameForSolution(_name, sln, name)) {
        if (sln != "current") {
            Solution *s = Solution::solution(sln);
            if (!s)
                return SolutionFile();
            else
                return s->buildFile(name, SolutionFile());
        }
    }

    bool isRelative = QFastDir::isRelativePath(name);
    if (isRelative && !relative.isValid())
        return SolutionFile();

    QString absPath;
    if (isRelative)
        absPath = relative.solutionDir() + "/" + name;
    else
        absPath = name;
    absPath = QDir::cleanPath(absPath);

    SolutionFile rv;
    rv.m_type = SolutionFile::Build;
    rv.m_fs = this;
    rv.m_request = absPath;
    rv.m_path = QDir::cleanPath(m_paths.first()->path() + absPath);
    return rv;
}

/*!
  Returns the first solution file instance that maps to \a name with the given type.
  If name is relative, it will be taken as relative to the \a relative file.  If
  no relative file is provided and name is relative, a null SolutionFile will
  be returned.

  FIXME \a type
 */
SolutionFile Solution::file(const QString &name,
                            Type type,
                            const SolutionFile &relative) const
{
    SolutionFiles f = files(name, type, relative);
    if (f.isEmpty())
        return SolutionFile();
    else
        return f.first();
}

/*!
   The same as file(), except that it returns a list of all files, in case more
   than one exists.

   FIXME \a _name \a type \a relative.
 */
SolutionFiles Solution::files(const QString &_name,
                              Type type,
                              const SolutionFile &relative) const
{
    QString sln;
    QString name;
    if (checkNameForSolution(_name, sln, name)) {
        if (sln != "current") {
            Solution *s = Solution::solution(sln);
            if (!s)
                return SolutionFiles();
            else
                return s->files(name, type, SolutionFile());
        }
    }

    if (name.startsWith("!")) {
        name = name.mid(1);
        type = (Type)(type & ~_Project | Absolute);
    }

    bool isRelative = QFastDir::isRelativePath(name);
    if (isRelative && !relative.isValid())
        return SolutionFiles();

    QSet<QString> unique;
    SolutionFiles files;
    if (type & Wildcard) {

        if (type & _Project) {
            QString absPath;
            if (isRelative)
                absPath = relative.solutionDir() + "/" + name;
            else
                absPath = name;
            absPath = QDir::cleanPath(absPath);
            PathGlob glob(absPath);
            PathIterator iter(this);
            SolutionFiles tfiles = filesRecur(iter, glob, 0);
            for (int ii = 0; ii < tfiles.count(); ++ii) {
                const SolutionFile &tf = tfiles.at(ii);
                if (!unique.contains(tf.fsPath())) {
                    unique.insert(tf.fsPath());
                    files << tf;
                }
            }
        }

        if (type & Absolute) {
            QString absPath;
            if (isRelative)
                absPath = relative.fsDir() + "/" + name;
            else
                absPath = name;
            absPath = QDir::cleanPath(absPath);
            PathGlob glob(absPath);
            PathIterator iter("");
            SolutionFiles tfiles = filesRecur(iter, glob, 0);
            for (int ii = 0; ii < tfiles.count(); ++ii) {
                const SolutionFile &tf = tfiles.at(ii);
                if (!unique.contains(tf.fsPath())) {
                    unique.insert(tf.fsPath());
                    files << tf;
                }
            }
        }

    } else {

        if (type & _Project) {

            QString absPath;
            if (isRelative)
                absPath = relative.solutionDir() + "/" + name;
            else
                absPath = name;
            absPath = QDir::cleanPath(absPath);

            QString absDir = QFastDir::dir(absPath);
            QString absName = QFastDir::fileName(absPath);

            PathIterator pi(this);
            pi = pi.advance(absDir);

            SolutionFiles tfiles = pi.files(absName.toLatin1());

            for (int ii = 0; ii < tfiles.count(); ++ii) {
                const SolutionFile &tf = tfiles.at(ii);
                if (!unique.contains(tf.fsPath())) {
                    unique.insert(tf.fsPath());
                    files << tf;
                }
            }
        }

        if (type & Absolute) {

            QString absPath;
            if (isRelative)
                absPath = relative.fsDir() + "/" + name;
            else
                absPath = name;
            absPath = QDir::cleanPath(absPath);

            if (QFastDir::isFile(absPath)) {
                SolutionFile sf;
                sf.m_type = SolutionFile::Absolute;
                sf.m_fs = 0;
                sf.m_request = absPath;
                sf.m_path = absPath;
                if (!unique.contains(absPath)) {
                    unique.insert(absPath);
                    files << sf;
                }
            }

        }

    }

    return files;
}

/*!
  \internal
*/
SolutionFiles Solution::_pathsRecur(const PathIterator &iter,
                                    const PathGlob &glob,
                                    int depth) const
{
    SolutionFiles rv;

    if (depth == glob.depth()) {
        SolutionFile file;
        if (iter.solution())
            file.m_type = SolutionFile::Project;
        else
            file.m_type = SolutionFile::Absolute;
        file.m_fs = iter.solution();
        file.m_request = iter.path();
        file.m_isDir = true;
        foreach(QString path, iter.fsPath()) {
            file.m_path = path;
            rv << file;
        }
    } else if (glob.isConst(depth)) {
        PathIterator niter = iter.advance(glob.glob(depth));
        if (niter.isValid())
            rv = _pathsRecur(niter, glob, depth + 1);
    } else {
        QStringList paths = iter.paths(glob.glob(depth));
        foreach(QString path, paths) {
            PathIterator niter = iter.advance(path);
            if (niter.isValid())
                rv << _pathsRecur(niter, glob, depth + 1);
        }
    }
    return rv;
}


QStringList Solution::pathsRecur(const PathIterator &iter,
                                 const PathGlob &glob, int depth) const
{
    QStringList rv;

    if (depth == glob.depth()) {
        rv << iter.path();
    } else if (glob.isConst(depth)) {
        PathIterator niter = iter.advance(glob.glob(depth));
        if (niter.isValid())
            rv = pathsRecur(niter, glob, depth + 1);
    } else {
        QStringList paths = iter.paths(glob.glob(depth));
        foreach(QString path, paths) {
            PathIterator niter = iter.advance(path);
            if (niter.isValid())
                rv << pathsRecur(niter, glob, depth + 1);
        }
    }
    return rv;
}

SolutionFiles Solution::filesRecur(const PathIterator &iter,
                                   const PathGlob &glob, int depth) const
{
    SolutionFiles rv;

    if (depth == glob.depth() - 1) {
        // Files!
        rv = iter.files(glob.glob(depth));
    } else if (depth < glob.depth() && glob.isConst(depth)) {
        PathIterator niter = iter.advance(glob.glob(depth));
        if (niter.isValid())
            rv = filesRecur(niter, glob, depth + 1);
    } else if ( depth < glob.depth() ) {
        QStringList paths = iter.paths(glob.glob(depth));
        foreach(QString path, paths) {
            PathIterator niter = iter.advance(path);
            if (niter.isValid())
                rv << filesRecur(niter, glob, depth + 1);
        }
    }

    return rv;
}

/*!
  \internal
*/
SolutionFiles Solution::_paths(const QString &_name,
                               Type type,
                               const SolutionFile &relative) const
{
    QString sln;
    QString name;
    if (checkNameForSolution(_name, sln, name)) {
        if (sln != "current") {
            Solution *s = Solution::solution(sln);
            if (!s)
                return SolutionFiles();
            else
                return s->files(name, type, SolutionFile());
        }
    }

    if (name.startsWith("!")) {
        name = name.mid(1);
        type = (Type)(type & ~_Project | Absolute);
    }

    bool isRelative = QFastDir::isRelativePath(name);
    if (isRelative && !relative.isValid())
        return SolutionFiles();

    QSet<QString> unique;
    SolutionFiles files;
    if (type & Wildcard) {

        if (type & _Project) {
            QString absPath;
            if (isRelative)
                absPath = relative.solutionDir() + "/" + name;
            else
                absPath = name;
            absPath = QDir::cleanPath(absPath);
            PathGlob glob(absPath);
            PathIterator iter(this);
            SolutionFiles tfiles = _pathsRecur(iter, glob, 0);
            for (int ii = 0; ii < tfiles.count(); ++ii) {
                const SolutionFile &tf = tfiles.at(ii);
                if (!unique.contains(tf.fsPath())) {
                    unique.insert(tf.fsPath());
                    files << tf;
                }
            }
        }

        if (type & Absolute) {
            QString absPath;
            if (isRelative)
                absPath = relative.fsDir() + "/" + name;
            else
                absPath = name;
            absPath = QDir::cleanPath(absPath);
            PathGlob glob(absPath);
            PathIterator iter("");
            SolutionFiles tfiles = _pathsRecur(iter, glob, 0);
            for (int ii = 0; ii < tfiles.count(); ++ii) {
                const SolutionFile &tf = tfiles.at(ii);
                if (!unique.contains(tf.fsPath())) {
                    unique.insert(tf.fsPath());
                    files << tf;
                }
            }
        }

    } else {

        if (type & _Project) {

            QString absPath;
            if (isRelative)
                absPath = relative.solutionDir() + "/" + name;
            else
                absPath = name;
            absPath = QDir::cleanPath(absPath);

            QString absDir = QFastDir::dir(absPath);
            QString absName = QFastDir::fileName(absPath);

            PathIterator pi(this);
            pi = pi.advance(absDir);

            foreach(QString path, pi.fsPath()) {
                if (!unique.contains(path) && QFastDir::isDir(path)) {
                    unique.insert(path);
                    SolutionFile file;
                    file.m_type = SolutionFile::Project;
                    file.m_fs = this;
                    file.m_request = pi.path();
                    file.m_path = path;
                    file.m_isDir = true;
                    files << file;
                }

            }
        }

        if (type & Absolute) {

            QString absPath;
            if (isRelative)
                absPath = relative.fsDir() + "/" + name;
            else
                absPath = name;
            absPath = QDir::cleanPath(absPath);

            if (!unique.contains(absPath) && QFastDir::isDir(absPath)) {
                unique.insert(absPath);
                SolutionFile sf;
                sf.m_type = SolutionFile::Absolute;
                sf.m_fs = 0;
                sf.m_request = absPath;
                sf.m_path = absPath;
                sf.m_isDir = true;
                files << sf;
            }

        }

    }

    return files;

}

/*!
  Returns a list of fully qualified paths that match \a _name.  This is generally
  only useful if Wildcard is specified.

  FIXME \a type \a relative
*/
QStringList Solution::paths(const QString &_name,
                            Type type,
                            const SolutionFile &relative) const
{
    QString sln;
    QString name;
    if (checkNameForSolution(_name, sln, name)) {
        if (sln != "current") {
            Solution *s = Solution::solution(sln);
            if (!s)
                return QStringList();
            else
                return s->paths(name, type, SolutionFile());
        }
    }

    if (name.startsWith("!")) {
        name = name.mid(1);
        type = (Type)(type & ~_Project | Absolute);
    }

    bool isRelative = QFastDir::isRelativePath(name);
    if (isRelative && !relative.isValid())
        return QStringList();

    QStringList paths;

    if (type & Wildcard) {

        if (type & Absolute) {
            QString absPath;
            if (isRelative)
                absPath = relative.fsDir() + "/" + name;
            else
                absPath = name;
            absPath = QDir::cleanPath(absPath);

            PathGlob glob(absPath);
            PathIterator iter("");
            paths << pathsRecur(iter, glob, 0);
        }

        if (type & _Project) {
            QString absPath;
            if (isRelative)
                absPath = relative.solutionDir() + "/" + name;
            else
                absPath = name;
            absPath = QDir::cleanPath(absPath);

            PathGlob glob(absPath);
            PathIterator iter(this);
            paths << pathsRecur(iter, glob, 0);
        }

    } else {

        if (type & Absolute) {

            QString absPath;
            if (isRelative)
                absPath = relative.fsDir() + "/" + name;
            else
                absPath = name;
            absPath = QDir::cleanPath(absPath);

            if (QFastDir::isDir(absPath))
                paths << absPath;
        }

        if (type & _Project) {

            QString absPath;
            if (isRelative)
                absPath = relative.solutionDir() + "/" + name;
            else
                absPath = name;
            absPath = QDir::cleanPath(absPath);

            QString absDir = QFastDir::dir(absPath);
            QString absName = QFastDir::fileName(absPath);

            PathIterator pi(this);
            pi = pi.advance(absDir);
            if (pi.isValid())
                paths << pi.path();

        }

    }

    return paths;
}

/*!
  Returns a list of fully qualified sub-paths of \a name.  If \a name is
  relative is is evaluated as relative to the \a relative file.  If it is
  relative and the file is not provided, an empty list is returned.

  FIXME \a type
*/
QStringList Solution::subPaths(const QString &name,
                               Type type,
                               const SolutionFile &relative) const
{
    QStringList absPaths;
    QStringList projPaths;
    if (type & Absolute)
        absPaths = paths(name, (Solution::Type)(type & ~Absolute), relative);
    if (type & _Project)
        projPaths = paths(name, (Solution::Type)(type & ~Project), relative);

    QStringList paths;
    foreach(QString path, absPaths) {
        PathIterator pi(path);
        foreach(QString sub, pi.paths())
            paths << pi.advance(sub).path();
    }
    foreach(QString path, projPaths) {
        PathIterator pi(this);
        pi.advance(path);
        foreach(QString sub, pi.paths())
            paths << pi.advance(sub).path();
    }

    return paths;
}

/*!
  Returns a list of fully qualified filesystem paths that map to \a _name.
  If name is relative, it will be taken as relative to the \a relative file.  If
  no relative file is provided and name is relative, an empty string will
  be returned.

  FIXME \a type
*/
QStringList Solution::filesystemPaths(const QString &_name,
                                      Type type,
                                      const SolutionFile &relative) const
{
    QString name = _name;
    if (name.startsWith("!")) {
        name = name.mid(1);
        type = (Type)(type & ~_Project | Absolute);
    }

    QStringList absPaths;
    QStringList projPaths;
    if (type & Absolute)
        absPaths = paths(name, (Solution::Type)(type & ~_Project), relative);
    if (type & _Project)
        projPaths = paths(name, (Solution::Type)(type & ~Absolute), relative);

    QStringList paths;
    paths << absPaths;
    foreach(QString path, projPaths) {
        PathIterator pi(this);
        paths << pi.advance(path).fsPath();
    }

    return paths;
}

bool Solution::checkNameForSolution(const QString &name,
                                    QString &solution,
                                    QString &path) const
{
    int colonIdx = name.indexOf(':');
    int slashIdx = -1;
    if (-1 != colonIdx) {
       slashIdx = name.indexOf('/');

       if ((slashIdx == -1) || colonIdx < slashIdx) {
           solution = name.left(colonIdx);
           path = name.mid(colonIdx + 1);

           return true;
       }
    }
    path = name;
    return false;
}


/*!
  Takes a solution path \a id and returns a SolutionFile instance.

  If \a type is Solution::Project, all project directories (including the build
  directory) is searched.  The first directory containing the file is returned,
  or the build directory if the file doesn't exist.

  if \a type is Solution::Generated, the file is returned relative to the
  build directory.

  If \a type is Solution::Existing, the same directories as Solution::Project
  are searched, with the additional constraint that if the file does not exist
  (even in the build directory) a null SolutionFile is returned.

  The solution path \a id has the following form:
  \code
      [<Solution Name>:]<Request Path>
  \endcode
  The request path must not contain ':' characters before the first separator
  character '/' to prevent an ambiguous reference.  If necessary, the special
  solution name "current" can be used to work around this.

  If \a file is provided (an a non-"current" solution name is not provided),
  \a id may be specified relative to it.
 */
SolutionFile Solution::findFile(const QString &id,
                                FileType type,
                                const SolutionFile &file) const
{
    if (file.isValid() && file.m_fs != this)
        return file.m_fs->findFile(id, type, file);

    QString request = id.trimmed();
    if (request.startsWith("!"))
        return findAbsFile(request, file, type & Existing);
    QString request_orig = request;

    int colonIdx = request.indexOf(':');
    int slashIdx = -1;
    if (-1 != colonIdx) {
       slashIdx = request.indexOf('/');

       if ((slashIdx == -1) || colonIdx < slashIdx) {
           QString solution = request.left(colonIdx);
           QString requestPath = request.mid(colonIdx + 1);

           if (solution != "current") {
               Solution *sln = Solution::solution(solution);
               if (!sln)
                   return SolutionFile();
               else
                   return sln->findFile(requestPath, type);
           } else {
               request = requestPath;
           }
       }
    }

    if (QFastDir::isRelativePath(request)) {
        if (file.isValid()) {
            // XXX can lead to a ../something
            int index = file.m_request.lastIndexOf('/');
            Q_ASSERT(index != -1);
            QString path = file.m_request.left(index + 1);
            request = QDir::cleanPath(path + QDir::separator() + request);
        } else {
            request.prepend('/');
        }
    } else {
        request = QDir::cleanPath(request);
    }

    if (request.startsWith("!"))
        return findAbsFile(request, file, type & Existing);

    // request is now absolute

    if (type == Project) {
        // This is tricky... Look at the alternatives and then pick the
        // one with the longest path to qbuild.pro.
        QList<SolutionFile> alternatives;
        QString mappedPath;
        for (int ii = 0; ii < m_paths.count(); ++ii) {
            SolutionDir *path = m_paths.at((ii + 1) % m_paths.count());
            if (path->findFile(request, mappedPath)) {
                SolutionFile rv;
                rv.m_fs = this;
                rv.m_request = request;
                rv.m_path = mappedPath;
                alternatives << rv;
            }
        }
        // If we didn't match anything, return nothing
        if (!alternatives.count()) {
            SolutionFile rv;
            return rv;
        }

        struct {
            SolutionFile rv;
            QString dir;
        } longest;

        foreach (SolutionFile rv, alternatives) {
            QString dir;
            if (QFastDir::isDir(rv.m_path))
                dir = rv.m_path;
            else
                dir = QFastDir::dir(rv.m_path);
            while (!dir.isEmpty()) {
                if (QFastDir::isFile(dir+"/qbuild.pro")) {
                    if (dir.length() > longest.dir.length()) {
                        longest.dir = dir;
                        longest.rv = rv;
                    }
                    break;
                }
                dir = QFastDir::dir(dir);
            }
        }
        return longest.rv;
    }

    if (type == Existing) {
        // Check m_paths...
        QString mappedPath;
        for (int ii = 0; ii < m_paths.count(); ++ii) {
            SolutionDir *path = m_paths.at((ii + 1) % m_paths.count());
            if (path->findFile(request, mappedPath)) {
                SolutionFile rv;
                rv.m_fs = this;
                rv.m_request = request;
                rv.m_path = mappedPath;
                return rv;
            }
        }
    }

    // Must use build path
    Q_ASSERT(m_paths.first()->location() == QString());
    QString mappedPath =
        m_paths.first()->path() + QChar('/') + request;
    mappedPath = QDir::cleanPath(mappedPath);

    if (type != Existing || QFastDir::exists(mappedPath)) {
        SolutionFile rv;
        rv.m_fs = this;
        rv.m_request = request;
        rv.m_path = mappedPath;
        return rv;
    }

    return findAbsFile(request_orig, file, true);
}

/*!
  Returns the common.pri file for each solution.
*/
QList<SolutionFile> Solution::commonIncludes() const
{
    primeIncludes();
    return m_commonIncludes;
}

/*!
  Returns the blank.pri file for each solution.
*/
QList<SolutionFile> Solution::blankIncludes() const
{
    primeIncludes();
    return m_blankIncludes;
}

/*!
  Returns the disabled.pri file for each solution.
*/
QList<SolutionFile> Solution::disabledIncludes() const
{
    primeIncludes();
    return m_disabledIncludes;
}

/*!
  Returns the default.pri file for each solution.
*/
QList<SolutionFile> Solution::defaultIncludes() const
{
    primeIncludes();
    return m_defaultIncludes;
}

/*!
  \internal
*/
void Solution::primeIncludes() const
{
    if (m_includesPrimed)
        return;

    LOCK(Solution);
    m_lock.lock();
    QStringList extensionsPaths = FunctionProvider::extensionsPaths();

    foreach(QString extension, extensionsPaths) {
        QStringList files = this->files(extension + "/*");
        foreach(QString file, files) {
            if (!file.endsWith(".pri"))
                continue;

            if (file.startsWith(extension + "/common_") ||
               file == extension + "/common.pri") {
                m_commonIncludes << findFile(file, Existing);
            } else if (file.startsWith(extension + "/blank_") ||
                      file == extension + "/blank.pri") {
                m_blankIncludes << findFile(file, Existing);
            } else if (file.startsWith(extension + "/disabled_") ||
                      file == extension + "/disabled.pri") {
                m_disabledIncludes << findFile(file, Existing);
            } else if (file.startsWith(extension + "/default_") ||
                      file == extension + "/default.pri") {
                m_defaultIncludes << findFile(file, Existing);
            }
        }
    }
    m_includesPrimed = true;
    m_lock.unlock();
}

/*!
  Takes the true (absolute) \a realPath and returns it as a SolutionFile.  Thanks
  to the "!" mechanism (absolute fs paths), this will *always* return a valid
  SolutionFile unless a non-absolute path is passed in which case a null
  SolutionFile is returned.
 */
SolutionFile Solution::realToSolution(const QString &realPath)
{
    if (QFastDir::isRelativePath(realPath))
        return SolutionFile();

    SolutionFile rv;
    rv.m_fs = this;
    rv.m_path = realPath;
    int longest = 0;
    for (int ii = 0; ii < m_paths.count(); ++ii) {
        SolutionDir *option = m_paths.at(ii);

        if (realPath.startsWith(option->path()) && option->path().length() > longest) {
            longest = option->path().length();
            QString path = option->location() +
                           realPath.mid(option->path().length());
            rv.m_request = path;
        }
    }

    if (!longest)
        rv.m_request = "!" + realPath;

    return rv;
}

/*!
  Takes the true (absolute) \a realPath and returns it as a SolutionFile.
  This version uses fuzzy logic so that paths like <build>/devices/greenphone/src
  are mapped to /src.
*/
SolutionFile Solution::fuzzyRealToSolution(const QString &realPath)
{
    if (QFastDir::isRelativePath(realPath))
        return SolutionFile();

    SolutionFile rv;
    rv.m_fs = this;
    rv.m_path = realPath;
    int longest = 0;
    QString shortPath;
    for (int ii = 0; ii < m_paths.count(); ++ii) {
        SolutionDir *option = m_paths.at(ii);

        if (realPath.startsWith(option->path()) && option->path().length() > longest) {
            longest = option->path().length();
            QString path = option->location() +
                           realPath.mid(option->path().length());
            rv.m_request = path;
            shortPath = realPath.mid(option->path().length());
        }
    }

    if (longest) {
        QString tail = shortPath;
        do {
            for (int ii = 0; ii < m_paths.count(); ++ii) {
                SolutionDir *option = m_paths.at(ii);

                if (option->path().endsWith(shortPath)) {
                    tail.replace(shortPath, "");
                    rv.m_request = option->location() + tail;
                    if (rv.m_request.isEmpty())
                        rv.m_request = "/";
                    goto after;
                }
            }
            shortPath.replace(QRegExp("/[^/]*$"), "");
        } while (!shortPath.isEmpty());
after:
        ; // NOP
    } else {
        rv.m_request = "!" + realPath;
    }

    return rv;
}

SolutionFile Solution::findAbsFile(const QString &req,
                                   const SolutionFile &file,
                                   bool exist) const
{
    QString request;

    if (req.startsWith("!"))
        request = req.mid(1); // remove "!"
    else
        request = req;

    if (QFastDir::isRelativePath(request)) {
        if (file.isValid()) {
            // XXX can lead to a ../something
            int index = file.m_path.lastIndexOf('/');
            Q_ASSERT(index != -1);
            QString path = file.m_path.left(index + 1);
            request = path + QDir::separator() + request;
        } else {
            request.prepend('/');
        }
    }

    request = QDir::cleanPath(request);
    SolutionFile rv;

    if (!exist || QFastDir::exists(request)) {
        rv.m_type = SolutionFile::Absolute;
        rv.m_request = req;
        if (!rv.m_request.startsWith('!'))
            rv.m_request.prepend('!');
        rv.m_path = request;
        rv.m_fs = this;
    }

    return rv;
}

void Solution::dump(int ind)
{
    QByteArray indent(ind * 4, ' ');
    for (int ii = 0; ii < m_paths.count(); ++ii)
        qWarning() << indent.constData() << m_paths.at(ii)->location() << "->" << m_paths.at(ii)->path();
}

/*!
  \internal
  \class SolutionProject
  \brief The SolutionProject class allows access to projects.

  The SolutionProject is reenterant, although not threadsafe.  Multiple instances
  can refer to the same project in a threadsafe manner.
 */
SolutionProject::SolutionProject(Solution *solution,
                                 const QString &node)
: m_solution(solution),
  m_nodePath(node),
  m_filemode(false),
  m_primed(false),
  m_filePrimed(false)
{
    Q_ASSERT(m_solution);

    m_nodePath = simplifyNodeName(m_nodePath);
}

SolutionProject::SolutionProject()
: m_solution(0),
  m_filemode(false)
{
}

bool SolutionProject::isValid() const
{
    return m_solution != 0;
}

bool SolutionProject::isSubproject(const SolutionProject &other) const
{
    return other.solution() == solution() &&
           other.nodePath().startsWith(nodePath());
}

SolutionProject::SolutionProject(const SolutionProject &other)
: m_solution(other.m_solution),
  m_nodePath(other.m_nodePath),
  m_fileName(other.m_fileName),
  m_node(other.m_node),
  m_filemode(other.m_filemode),
  m_primed(other.m_primed),
  m_filePrimed(other.m_filePrimed),
  m_file(other.m_file)
{
}

SolutionProject &SolutionProject::operator=(const SolutionProject &other)
{
    m_solution = other.m_solution;
    m_nodePath = other.m_nodePath;
    m_fileName = other.m_fileName;
    m_node = other.m_node;
    m_filemode = other.m_filemode;
    m_primed = other.m_primed;
    m_filePrimed = other.m_filePrimed;
    m_file = other.m_file;
    return *this;
}

QString SolutionProject::node() const
{
    primeFile();
    return m_node;
}

QString SolutionProject::nodePath() const
{
    primeFile();
    return m_nodePath;
}

QString SolutionProject::fileName() const
{
    primeFile();
    return m_fileName;
}

bool SolutionProject::fileMode() const
{
    primeFile();
    return m_filemode;
}

void SolutionProject::setFileMode(bool m)
{
    primeFile();
    if (m_filemode == m)
        return;

    m_filemode = m;
    if (m_filemode) {
        if (fileName() == "qbuild.pro")
            m_node.append("{file}/");
        else
            m_node = m_node.left(m_node.length() - 1) + "{file}/";
    } else {
        m_node = m_node.left(m_node.length() - 7);
        if (!m_node.endsWith('/'))
            m_node.append("/");
    }
}

SolutionFile SolutionProject::projectFile() const
{
    primeFile();
    return m_file;
}


/*
    /hello/world.pri{file}
    /hello/world.pri
    /hello/world
*/
void SolutionProject::primeFile() const
{
    if (m_filePrimed)
        return;
    m_filePrimed = true;

    bool directFile = true;

    QString prime = m_nodePath;
    Q_ASSERT(prime.endsWith('/'));
    if (prime.length() != 1) // prime == "/" case
        prime = prime.left(prime.length() - 1);
    else
        directFile = false;

    // Check if this is a file mode access
    m_filemode = false;
    if (prime.endsWith("{file}")) {
        m_filemode = true;
        prime = prime.left(prime.length() - 6);
    }

    if (prime.endsWith('/')) {
        directFile = false;
        prime = prime.left(prime.length() - 1);
    }

    // First try the specified name
    if (directFile)
        m_file = solution()->findFile(prime, Solution::Existing);

    bool looped = false;
loop:
    if (m_file.isValid() && QFastDir::isFile(m_file.fsPath())) {

        int idx = prime.lastIndexOf('/');
        Q_ASSERT(idx != -1);
        QString name = prime.mid(idx + 1);

        if (name == "qbuild.pro") {
            m_nodePath = prime.left(idx + 1);
            m_node = m_nodePath;
            if (m_filemode)
                m_node.append("{file}/");
            m_fileName = name;
        } else {
            m_nodePath = prime.left(idx + 1);
            m_node = m_nodePath + name;
            if (m_filemode)
                m_node.append("{file}/");
            else
                m_node.append("/");
            m_fileName = name;
        }

    } else {
        // Use a qbuild.pro file
        m_file = solution()->findFile(prime + "/qbuild.pro",
                                      Solution::Existing);
        m_node = prime + "/";
        if (m_filemode)
            m_node.append("{file}/");
        m_fileName = "qbuild.pro";
    }

    // We have a path that goes through a symlink.
    // Use some magic to determine the correct path
    // and re-run this function so that everything is good.
    SolutionFile canon = m_file.canonicalPath();
    if ( canon.fsPath() != m_file.fsPath() ) {
        Q_ASSERT(!looped);
        //qWarning() << "Found a path with a symlink!";
        //qWarning() << "m_file" << m_file;
        //qWarning() << "canon" << canon;
        m_file = canon;
        m_nodePath = m_file.solutionPath();
        if ( m_nodePath.startsWith("!") )
            m_nodePath = m_nodePath.mid(1);
        m_nodePath = simplifyNodeName(m_nodePath);
        looped = true;
        goto loop;
    }
}

struct ProjectCache
{
    /* Call locked */
    void outputDeadlockMessage(const SolutionProject &proj)
    {
        QThread *me = QThread::currentThread();

        qWarning() << "QBuild: Fatal project deadlock detected:";

        for (QHash<QThread *, QStringList>::ConstIterator
                iter = servicingLog.begin(); iter != servicingLog.end(); ++iter){
            if (iter.key() == me)
                qWarning() << "Thread" << iter.key() << "(deadlock thread)";
            else
                qWarning() << "Thread" << iter.key();
            const QStringList &str = *iter;
            for (int ii = 0; ii < str.count(); ++ii)
                qOutput() << "    " << str.at(ii);
            if (iter.key() == me)
                qOutput() << "     XXX" << proj.node() << "XXX";
        }
        qOutput() << "QBuild: Fatal project DEADLOCK";
        QBuild::shutdown();
    }

    /* Call unlocked */
    Project *project(const SolutionProject &proj)
    {
        LOCK(ProjectCache);
        lock.lock();

        QThread *me = QThread::currentThread();

        Cache::Iterator iter = cache.find(proj.node());
        while (iter == cache.end()) {
            if (servicing.contains(proj.node())) {

                // Check for deadlocks - the system has NOT deadlocked if we
                // find a servicing thread.  It has deadlocked if we find our
                // way back to ourselves
                QString current = proj.node();
                QStringList stack;
                stack << current;
                while (true) {
                    QThread *s = servicing[current];
                    if (s == me) {
                        // Deadlock
                        outputDeadlockMessage(proj);
                    } else {
                        QHash<QThread *, QString>::Iterator witer =
                            threadWaiting.find(s);
                        if (witer == threadWaiting.end())
                            break; // No deadlock, there is a progressing thread
                        else
                            current = *witer;
                        stack << current;
                    }
                }

                threadWaiting.insert(me, proj.node());

                WaitingThreads::Iterator witer =
                    waitingThreads.find(proj.node());
                if (witer == waitingThreads.end()) {
                    QWaitCondition *wc = 0;
                    if (waits.isEmpty()) {
                        wc = new QWaitCondition;
                    } else {
                        wc = waits.takeFirst();
                    }
                    witer = waitingThreads.insert(proj.node(), qMakePair(wc, int(0)));
                }
                witer->second++;
                witer->first->wait(&lock);

                iter = cache.find(proj.node());
                Q_ASSERT(iter != cache.end());
                witer = waitingThreads.find(proj.node());
                Q_ASSERT(witer != waitingThreads.end());
                witer->second--;
                if (!witer->second) {
                    waits << witer->first;
                    waitingThreads.erase(witer);
                }

                threadWaiting.remove(me);
            } else {
                servicing.insert(proj.node(), me);
                servicingLog[me].append(proj.node());
                lock.unlock();
                if (!proj.fileMode()) {
                    QString name = proj.node();
                    if (name.length() > 1)
                        name.chop(1);
                    if ( !options.silent ) qOutput() << "QBuild: Opening project" << name;
                }
                QBuild::beginPerfTiming("Project::Load");
                Project *p = new Project(proj, proj.fileMode()?Project::FileMode:Project::ProjectMode);
                p->finalize();
                QBuild::endPerfTiming();
                LOCK(ProjectCache);
                lock.lock();
                servicing.remove(proj.node());
                servicingLog[me].removeLast();
                iter = cache.insert(proj.node(), p);
                WaitingThreads::ConstIterator iter = waitingThreads.find(proj.node());
                if (iter != waitingThreads.end())
                    iter->first->wakeAll();
            }
        }

        Project *rv = *iter;
        lock.unlock();
        return rv;
    }

    // Protects read/write from all structures in the project cache
    QMutex lock;

    // Mapping between project node() and the finalized Project ptr
    typedef QHash<QString, Project *> Cache;
    Cache cache;

    QHash<QString, QThread *> servicing;
    QHash<QThread *, QString> threadWaiting;
    typedef QHash<QString, QPair<QWaitCondition *, int> > WaitingThreads;
    WaitingThreads waitingThreads;
    QHash<QThread *, QStringList> servicingLog;

    QList<QWaitCondition *> waits;
};

Q_GLOBAL_STATIC(ProjectCache, projectCache)
Q_GLOBAL_STATIC(ProjectCache, fileModeProjectCache)

Project *SolutionProject::project() const
{
    ProjectCache *cache = fileMode()?fileModeProjectCache():projectCache();
    if (!cache)
        return 0;

    return cache->project(*this);
}

Solution *SolutionProject::solution() const
{
    return m_solution;
}

QStringList SolutionProject::subProjects() const
{
    prime();
    return m_subProjects;
}

SolutionProject SolutionProject::subProject(const QString &project) const
{
    return SolutionProject(solution(), node() + "/" + project);
}

/*
   Sets up sub projects.

   Descends down sub paths until it either finds a .pro file, or it reaches
   the end.  Searches for qbuild.pro.
 */
void SolutionProject::prime() const
{
    if (m_primed)
        return;

    primeFile();
    QStringList subDirs = solution()->paths(m_node);
    for (int ii = 0; ii < subDirs.count(); ++ii)
        if (search(subDirs.at(ii)))
            m_subProjects << subDirs.at(ii);

    m_primed = true;
}


bool SolutionProject::search(const QString &project) const
{
    QString path = m_node + project;

    int lastSeparator = project.lastIndexOf('/');
    QString projName;
    if (lastSeparator == -1)
        projName = project;
    else
        projName = project.mid(lastSeparator);

    QString qbuild_pro = path + QChar('/') + QLatin1String("qbuild.pro");
    SolutionFile file = solution()->findFile(qbuild_pro, Solution::Existing);

    if (!file.isValid()) {
        // Nothing here...
        QStringList subDirs = solution()->paths(path);
        for (int ii = 0; ii < subDirs.count(); ++ii)
            if (search(project + '/' + subDirs.at(ii)))
                return true;
    } else {
        // Found!
        return true;
    }

    return false;
}

QString SolutionProject::simplifyNodeName(const QString &node)
{
    QString rv = node;
    if (!rv.startsWith('/'))
        rv.prepend('/');
    if (!rv.endsWith('/'))
        rv.append('/');

    while (rv.contains("//"))
        rv.replace("//", "/");

    // replace /foo/../ with /
    while (rv.contains("/../"))
        rv.replace(QRegExp("/[^/]+/\\.\\./"), "/");

    // replace /./ with /
    while (rv.contains("/./"))
        rv.replace(QRegExp("/\\./"), "/");

    return rv;
}

void SolutionProject::splitNodeName(const QString &node,
                                    QString &solutionName,
                                    QString &path)
{
    int solutionIdx = node.indexOf(':');
    if (solutionIdx != -1) {
        solutionName = node.left(solutionIdx);
        solutionIdx++;
    } else {
        solutionIdx = 0;
    }

    path = node.mid(solutionIdx);
}

/*!
  Construct and return a SolutionProject from the specified \a node name and
  associcated \a solution.

  The \a node name has the following form:
    [<Solution Name>:]<Project path>[{<File name>}]

  If the <Solution Name> component is missing, \a solution is used instead.
  If \a solution is null, the Solution::defaultSolution() will be used.

  The special <Solution Name> "current" refers to the passed in solution.
 */
SolutionProject SolutionProject::fromNode(const QString &node,
                                          Solution *solution)
{
    QString solutionName;
    QString projPath;

    splitNodeName(node, solutionName, projPath);

    if (!solutionName.isEmpty() && solutionName != "current") {
        solution = Solution::solution(solutionName);
        if (!solution)
            return SolutionProject();
    }
    if (!solution)
        solution = Solution::defaultSolution();

    return SolutionProject(solution, projPath);
}

/*!
  \overload

  Using this overload, the <Project path> in the \a node name can be relative
  to the \a project.  Care must be taken to assume that the relative path does
  not contain ":" characters, as this makes disambiguation impossible.  For
  example, if "my:project/project" was meant to refer to a relative project
  directory, you would have to pass "current:my:project/project" instead.
*/
SolutionProject SolutionProject::fromNode(const QString &node,
                                          Project *project)
{
    Q_ASSERT(project);
    Q_ASSERT(project->solution());

    QString solutionName;
    QString projPath;
    QString fileName;

    splitNodeName(node, solutionName, projPath);

    Solution *solution = 0;
    if (!solutionName.isEmpty() && solutionName != "current") {
        solution = Solution::solution(solutionName);
        if (!solution)
            return SolutionProject();
    } else {
        solution = project->solution();
    }

    if (QFastDir::isRelativePath(projPath)) {
        projPath = project->nodePath() + "/" + projPath;
    }

    return SolutionProject(solution, projPath);
}

SolutionProject SolutionProject::fromSolutionFile(const QString &_file,
                                                  Solution *solution)
{
    QString file = _file;

    QString solutionName;

    int idx = file.indexOf(':');
    if (idx != -1) {
        solutionName = file.left(idx);
        file = file.mid(idx + 1);
    }


    if (!solutionName.isEmpty())
        solution = Solution::solution(solutionName);
    else if (!solution)
        solution = Solution::defaultSolution();

    if (!solution)
        return SolutionProject();

    return SolutionProject(solution, file);
}

/*!

\file qbuild.solution
\brief Describes the qbuild.solution file and its contents.

\c{qbuild.solution} is a QSettings-readable ini file. There are two useful
pieces of information in this file.

\list
\o The \c {BuildPath} key contains the location of the build hierarchy.
If the first character is a $ the rest of the string is taken as an
environment variable.
\o The \c {FilePaths} array contains an ordered list of project paths to map.
Each entry in the array must further contain a \c {FilePath} key and an
optional \c {ProjectLocation} key.  The \c {FilePath} points to a directory
to map into the project filesystem, and the \c {ProjectLocation} the point
in the filesystem to map it.  If \c {ProjectLocation} is omitted, it defaults
to the root of the project.
\endlist

Any relative paths in a qbuild.solution file are resolved relative to the
location of the containing qbuild.solution file.

The \c extensions directory under the path containing the qbuild executable is
always mapped into the \c extensions path of the solution to provide the
default extensions.

Here is an example solution file.

\code
[Solution]
[Solution\FilePaths]
size=1
1\FilePath=/home/user/src/myproject
\endcode

Here is a solution file that relies on an environment variable to locate the build tree.

\code
[Solution]
BuildPath=$QPEDIR
[Solution\FilePaths]
size=1
1\FilePath=/home/user/src/myproject
\endcode

\sa {Solution Filesystem}, SolutionDescriptor

*/

/*!
  \class SolutionDescriptor
  \brief The SolutionDescriptor class describes what would be done if a Solution was created on a particular path.

  Basically a wrapper around the \l qbuild.solution file.
*/

/*!
  \internal
*/
SolutionDescriptor::SolutionDescriptor()
: m_default(true)
{
}

/*!
  \internal
*/
SolutionDescriptor::SolutionDescriptor(const SolutionDescriptor &other)
: m_default(other.m_default), m_paths(other.m_paths),
  m_rootPath(other.m_rootPath), m_defaultSolution(other.m_defaultSolution)
{
}

/*!
  \internal
*/
SolutionDescriptor &
SolutionDescriptor::operator=(const SolutionDescriptor &other)
{
    m_default = other.m_default;
    m_paths = other.m_paths;
    m_rootPath = other.m_rootPath;
    m_defaultSolution = other.m_defaultSolution;
    return *this;
}

/*!
  \internal
*/
SolutionDescriptor::SolutionDescriptor(const QString &rootPath)
: m_default(false), m_rootPath(rootPath)
{
    init();
}

/*!
  \internal
*/
bool SolutionDescriptor::findSolutionDescriptor(const QString &rootPath, SolutionDescriptor &out)
{
    QDir dir(rootPath);

    do {
        if (dir.exists("qbuild.solution")) {
            out = SolutionDescriptor(dir.path());
            return true;
        }
    } while (dir.cdUp());

    return false;
}

/*!
  Returns true if there was no qbuild.solution file found and this is a
  default descriptor.
*/
bool SolutionDescriptor::isDefault() const
{
    return m_default;
}

/*!
  \internal
  Returns the path to qbuild.solution?!?
*/
QString SolutionDescriptor::rootPath() const
{
    return m_rootPath;
}

/*!
  \internal
  Returns the number of real paths mapped into the solution filesystem?!?
*/
QStringList SolutionDescriptor::solutions() const
{
    return m_paths.keys();
}

/*!
  \internal
  Returns a number of real paths for a given \a solution path?!?
*/
QList<SolutionDescriptor::Path>
SolutionDescriptor::paths(const QString &solution) const
{
    QHash<QString, QList<Path> >::ConstIterator iter = m_paths.find(solution);
    if (iter == m_paths.end())
        return QList<Path>();
    else
        return *iter;
}

/*!
  \internal
  Returns the default rootPath()?!?
*/
QString SolutionDescriptor::defaultSolution() const
{
    return m_defaultSolution;
}

/*!
  \internal
*/
void SolutionDescriptor::init()
{
    QDir dir(rootPath());

    if (dir.exists("qbuild.solution")) {

        m_default = false;

        QSettings cfg(dir.filePath("qbuild.solution"), QSettings::IniFormat);

        foreach(QString solution, cfg.childGroups()) {
            cfg.beginGroup(solution);

            QList<Path> paths;

            // Build dir
            QString build = cfg.value("BuildPath").toString().trimmed();
            if ( build.indexOf('$') == 0 )
                build = qgetenv(build.mid(1).toLocal8Bit().constData());
            if (build.isEmpty()) {
                paths << Path(QString(), rootPath());
            } else {
                paths << Path(QString(), path(rootPath(), build));
            }

            // File paths
            int count = cfg.beginReadArray("FilePaths");
            for (int ii = 0; ii < count; ++ii) {
                cfg.setArrayIndex(ii);
                QString filePath = cfg.value("FilePath").toString().trimmed();
                if ( filePath.indexOf('$') == 0 )
                    filePath = qgetenv(filePath.mid(1).toLocal8Bit().constData());
                if (filePath.isEmpty())
                    continue;

                QString location = cfg.value("ProjectLocation").toString().trimmed();
                if (location == "/")
                    location = QString();

                paths << Path(location, path(rootPath(), filePath));
            }
            cfg.endArray();

            m_paths[solution] = paths;

            if (cfg.value("Default", false).toBool())
                m_defaultSolution = solution;

            cfg.endGroup();
        }

        if (m_defaultSolution.isEmpty() && m_paths.contains("default"))
            m_defaultSolution = "default";

    } else {
        m_default = true;
        m_paths["default"] << Path(QString(), rootPath());
        m_defaultSolution = "default";
    }
}

/*!
  \internal
  Will delete any solutions already created through solution descriptors.
*/
void SolutionDescriptor::createSolutions() const
{
    static QHash<QString, Solution *> createdSolutions;

    foreach(QString solution, solutions()) {
        if (createdSolutions.contains(solution))
            delete createdSolutions[solution];
        createdSolutions[solution] = new Solution(solution, *this);
    }
}

/*!
  \internal
*/
QString SolutionDescriptor::path(const QString &base, const QString &sub) const
{
    if (QFastDir::isRelativePath(sub)) {
        return QDir::cleanPath(base + "/" + sub);
    } else {
        return QDir::cleanPath(sub);
    }
}

/*!
  \internal
  \class SolutionDescriptor::Path
*/
