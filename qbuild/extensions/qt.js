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

\extension qt

*/

function qt_init()
{
###
    CONFIG*=moc uic rcc cpp_compiler depends
    QT*=core gui
    DEPENDS.AUTO_PERSIST*=QT

    QMAKE.FINALIZE.qt.CALL = qt_finalize
    QMAKE.FINALIZE.qt.RUN_AFTER_ME = depends cpp_compiler
###
}

function qt_finalize()
{
    if ( !project.config("qt") || project.absName.match(/\{file\}\/$/) )
        return;

    qt_get_qtdir();
    echo("qt", "Got QTDIR "+project.property("QTDIR").strValue());
###
    contains(QT_CONFIG,reduce_exports):COMPILER.REDUCE_EXPORTS=1
    release:DEFINES+=QT_NO_DEBUG
    no_keywords:DEFINES+=QT_NO_KEYWORDS
    plugin {
        staticlib:DEFINES+=QT_STATICPLUGIN
        DEFINES+=QT_PLUGIN
    }
###
    // Pull in dependencies (which may add to QT)
    //project.info("calling depends_load_dependencies from qt_finalize");
    depends_load_dependencies();
###
    embedded:contains(QT,gui):QT*=network
###
    var qtdir = project.property("QTDIR");
    var qtinc = project.property("QTINC");
    var qtinc_p = project.property("QTINC_P");
    var qtlibs = project.property("QTLIBS");
    var qtlibs_p = project.property("QTLIBS_P");
    var includepath = project.property("INCLUDEPATH");
    var libs = project.property("LIBS");
    var defines = project.property("DEFINES");
    var qt = qt_sorted();
    for ( var ii in qt ) {
        var q = qt_resolveQt(qt[ii]);
        // Includes
        includepath.append(qtinc.strValue()+"/"+q.qlib);
        if ( !qtinc_p.isEmpty() )
            includepath.append(qtinc_p.strValue()+"/"+q.qlib);
        // DEFINES
        defines.append(q.defines);
        // CONFIG values
### q.config
        CONFIG*=<0>
###
        // LIBS
        if ( !libs.contains("-l"+q.qlib) ) {
            libs.append("-L"+qtlibs.strValue());
            libs.append("-l"+q.qlib);
        }
    }

    for ( var ii in qt ) {
        var q = qt_resolveQt(qt[ii]);

        // LIBS that the Qt lib depends on (needed for static builds)
        var file;
        // The .prl files exist in the SDK but not early enough for regular builds
        if ( !qtlibs_p.isEmpty() ) {
            file = qtlibs_p.strValue()+"/lib"+q.qlib+".prl";
        } else {
            file = qtlibs.strValue()+"/lib"+q.qlib+".prl";
        }
        if ( qbuild.invoke("exists", file) != "true" ) {
            requires("exists("+file+")");
            continue;
        }
        var prl = project.sproject(file);
        if ( !prl.isValid() )
            project.warning(file+" is not valid!");

        var proj = prl.fileModeProject();
        if ( proj.isProperty("QMAKE_PRL_LIBS") )
            libs.unite(proj.property("QMAKE_PRL_LIBS").value());
    }
}

function qt_get_qtdir()
{
    if (!project.config("qt") || project.absName.match(/\{file\}\/$/) )
        return;

    if ( !project.config("embedded") && project.config("system_qt") ) {
###
        # This matches the logic in src/libraries/qt/qbuild.pro
        QTDIR=$$[QT_PREFIX]
        QTINC=$$[QT_HEADERS]
        INCLUDEPATH+=$$QTINC
        QTLIBS=$$[QT_LIBRARIES]
        QT_NAME=HOST_QT
        QTDATA=$$[QT_DATA]
###
    } else {
        echo("qt", "already set? "+project.property("QTDIR").strValue());
        if ( !project.property("QTDIR").isEmpty() ) {
            echo("qt", "already got QTDIR");
            return;
        }

        var qt;
        if ( project.config("embedded") ) {
            qt = "qtopiacore";
            project.property("QT_NAME").setValue("QTE")
        } else {
            qt = "qt";
            project.property("QT_NAME").setValue("DQT")
        }
        echo("qt", "pull in "+qt+".dep now");
        // Pull in <qt>.dep now (it sets QTDIR)
        var prop = project.property("QT.MODULES");
        prop.setValue(qt);
        depends_load_modules(prop);
        echo("qt", "did it get set? "+project.property("QTDIR").strValue());
    }
    if ( project.property("QTDIR").isEmpty() ) {
        project.error("Did not get QTDIR!");
    }

    // Set QT_CONFIG
    var file;
    // The qconfig.pri file exists in the SDK but not early enough for regular builds
    if ( project.isProperty("QTDATA_P") ) {
        file = project.property("QTDATA_P").strValue()+"/mkspecs/qconfig.pri";
    } else {
        file = project.property("QTDATA").strValue()+"/mkspecs/qconfig.pri";
    }
    if ( qbuild.invoke("exists", file) != "true" ) {
        project.error(file+" does not exist!");
    }
    var qconfig = project.sproject(file).fileModeProject();
    project.property("QT_CONFIG").setValue(qconfig.property("QT_CONFIG").value());

    // Set the VERSION values
### project.property("QT_NAME").strValue()
    QT_VERSION=<0>_VERSION
    QT_MAJOR_VERSION=<0>_MAJOR_VERSION
    QT_MINOR_VERSION=<0>_MINOR_VERSION
    QT_REVISION_VERSION=<0>_REVISION_VERSION
###
}

function qt_sorted()
{
    // Taken from $QT_DEPOT/mkspecs/features/qt.prf
    var list = new Array( "webkit", "phonon", "dbus", "testlib", "script", "svg", "qt3support", "sql", "xmlpatterns", "xml", "opengl", "gui", "network", "core" );
    var qt = project.property("QT");
    var ret = project.property("QT.SORTED");
    for ( var ii in list ) {
        var value = list[ii];
        if ( qt.contains(value) )
            ret.unite(value);
    }
    qt.remove(ret.value());
    if ( !qt.isEmpty() ) {
        project.error("Unable to resolve QT values "+qt.value().join(", "));
    }
    qt.setValue(ret.value());
    return ret.value();
}

function qt_resolveQt(qt)
{
    var obj = new Object;
    obj.defines = new Array;
    // Adapted from $QT_DEPOT/mkspecs/features/qt.prf
    if ( qt == "core" ) obj.qlib = "QtCore";
    else if ( qt == "gui" ) obj.qlib = "QtGui";
    else if ( qt == "sql" ) obj.qlib = "QtSql";
    else if ( qt == "network" ) obj.qlib = "QtNetwork";
    else if ( qt == "xml" ) obj.qlib = "QtXml";
    else if ( qt == "testlib" ) obj.qlib = "QtTest";
    // Using optionally-available modules may make your project get disabled
    else if ( qt == "xmlpatterns" ) {
        requires("contains(QT_CONFIG,xmlpatterns)");
        obj.qlib = "QtXmlPatterns";
    }
    else if ( qt == "opengl" ) {
        requires("contains(QT_CONFIG,opengl)");
        obj.qlib = "QtOpenGL";
        obj.config = "opengl";
    }
    else if ( qt == "svg" ) {
        requires("contains(QT_CONFIG,svg)");
        obj.qlib = "QtSvg";
    }
    else if ( qt == "dbus" ) {
        requires("contains(QT_CONFIG,dbus)");
        obj.qlib = "QtDBus";
        obj.config= "dbusadaptors dbusinterfaces";
    }
    else if ( qt == "phonon" ) {
        requires("contains(QT_CONFIG,phonon)");
        obj.qlib = "phonon";
    }
    else if ( qt == "webkit" ) {
        requires("contains(QT_CONFIG,webkit)");
        obj.qlib = "QtWebKit";
    }
    else if ( qt == "qt3support" ) {
        requires("contains(QT_CONFIG,qt3support)");
        obj.qlib = "Qt3Support";
        obj.defines.push("QT3_SUPPORT");
    }
    // No trigger for qtscript so use the script check instead
    // QtScript is always available to host projects
    else if ( qt == "script" ) {
        requires("!embedded|script");
        obj.qlib = "QtScript";
    }
    else project.error("Unable to resolve qt "+qt);
    obj.defines.push("QT_"+qt.toUpperCase()+"_LIB");
    return obj;
}

function qt_get_libs(qtdir,sdkdir)
{
    if (!sdkdir)
        sdkdir = qtdir;
    var ret = new Array();
    var suffix = ".so.4";
    if (project.config("enable_singleexec"))
        suffix = ".a";
    ret.push(sdkdir+"/lib/libQtCore"+suffix);
    ret.push(sdkdir+"/lib/libQtGui"+suffix);
    ret.push(sdkdir+"/lib/libQtNetwork"+suffix);
    ret.push(sdkdir+"/lib/libQtSql"+suffix);
    ret.push(sdkdir+"/lib/libQtXml"+suffix);

    var qconfig = project.sproject(qtdir+"/mkspecs/qconfig.pri").fileModeProject();
    if (qconfig.property("QT_CONFIG").contains("svg"))
        ret.push(sdkdir+"/lib/libQtSvg"+suffix);
    if (qconfig.property("QT_CONFIG").contains("opengl"))
        ret.push(sdkdir+"/lib/libQtOpenGL"+suffix);
    if (qconfig.property("QT_CONFIG").contains("xmlpatterns"))
        ret.push(sdkdir+"/lib/libQtXmlPatterns"+suffix);
    if (qconfig.property("QT_CONFIG").contains("phonon"))
        ret.push(sdkdir+"/lib/libphonon"+suffix);
    if (qconfig.property("QT_CONFIG").contains("webkit"))
        ret.push(sdkdir+"/lib/libQtWebKit"+suffix);
    if (qconfig.property("QT_CONFIG").contains("dbus"))
        ret.push(sdkdir+"/lib/libQtDBus"+suffix);
    if (qconfig.property("QT_CONFIG").contains("script"))
        ret.push(sdkdir+"/lib/libQtScript"+suffix);
    if (qconfig.property("QT_CONFIG").contains("declarative"))
        ret.push(sdkdir+"/lib/libQtDeclarative"+suffix);

    // Qt doesn't tell us if this is on or not (use the Qt Extended switch instead)
    if (project.config("script"))
        ret.push(sdkdir+"/lib/libQtScript"+suffix);

    return ret;
}

