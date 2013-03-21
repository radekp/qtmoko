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

#include "qwhereaboutsfactory.h"
#include "qwhereaboutsplugin.h"
#include "qgpsdwhereabouts.h"

#include <QPluginManager>
#include <QSettings>

Q_GLOBAL_STATIC_WITH_ARGS(QPluginManager, pluginManager, (QLatin1String("whereabouts")))

/*!
    \class QWhereaboutsFactory
    \inpublicgroup QtLocationModule
    \ingroup whereabouts
    \brief The QWhereaboutsFactory class creates QWhereabouts instances.

    It detects the available Whereabouts plugins, allowing the preferred
    plugin to be loaded into the application at runtime. The create()
    function returns a QWhereabouts instance representing the plugin
    identified by a given key. The valid keys (i.e. the supported plugins)
    can be retrieved using the keys() function.

    Qtopia includes a built-in whereabouts plugin, "gpsd". This reads data
    from a \l{http://gpsd.berlios.de}{GPSd} daemon if one is present. When
    using this plugin, the \c source argument to create() can be an empty
    string; in this case, the plugin reads from \c localhost on
    the default TCP port (2947). Otherwise, the \c source specifies the
    address to read from, in "host:port" form (e.g. "127.0.0.1:2947").

    A default plugin can be specified by the "Plugins/Default" value in the
    \c $QPEDIR/etc/Settings/Trolltech/Whereabouts.conf settings file. To
    specify a different default plugin, set this value to the name of the
    plugin class.

    \sa QWhereaboutsPlugin, {Location Services}
*/

/*!
    Creates and returns a QWhereabouts object specified by \a key that will
    read position data from the given \a source. The \a key is either a
    built-in plugin or the class name of a whereabouts plugin
    (see QWhereaboutsPlugin). Note that the keys are case-insensitive.

    If \a key is an empty string, this uses the default plugin, or the first
    known plugin if no default is specified.

    If \a source is an empty string, a default source is used, if possible.
    The default value is determined by the individual plugin.

    \sa QWhereaboutsPlugin
*/
QWhereabouts *QWhereaboutsFactory::create(const QString &key, const QString &source)
{
    if (key.isEmpty()) {
        QSettings settings("Trolltech", "Whereabouts");
        settings.beginGroup("Plugins");
        QString plugin = settings.value("Default").toString();
        if (plugin.isEmpty()) {
            QStringList allKeys = keys();
            if (allKeys.count() == 0 || allKeys[0].isEmpty())
                return 0;
            plugin = allKeys[0];
        }
        return QWhereaboutsFactory::create(plugin);
    }

    if (key.toLower() == "gpsd") {
        if (source.trimmed().isEmpty())
            return new QGpsdWhereabouts;
        // QGpsdWhereabouts will take care of any bad arguments
        QStringList args = source.split(":");
        quint16 port = args.value(1).toUInt();
        return new QGpsdWhereabouts(0, QHostAddress(args.value(0)), port);
    }

    QWhereaboutsPlugin *interface =
            qobject_cast<QWhereaboutsPlugin *>(pluginManager()->instance(key.toLower()));
    if (interface)
        return interface->create(source);
    return 0;
}

/*!
    Returns the list of valid keys that can be passed to create(), i.e. the
    names of the available whereabouts plugins.
*/
QStringList QWhereaboutsFactory::keys()
{
    QStringList keys = pluginManager()->list();
    keys << "gpsd";
    return keys;
}
