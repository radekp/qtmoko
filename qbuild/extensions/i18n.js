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

\extension i18n

The i18n extension handles translatable files.

The usual way to enable i18n is to set the string language and the translation languages.

\code
STRING_LANGUAGE=en_US
LANGUAGES=en_US de
\endcode

\sa {Conditional Sources Extension}

*/

/*!

\qbuild_variable STRING_LANGUAGE
\ingroup i18n_extension

This is the language used in strings.

*/

/*!

\qbuild_variable AVAILABLE_LANGUAGES
\ingroup i18n_extension

The list of languages to produce translations for. If the \l STRING_LANGUAGE is in this list
a set of plural-only translations will be created.

Note that this variable implicitly contains any values in the \l LANGUAGES variable so third
party projects may just wish to use that variable.

*/

/*!

\qbuild_variable LANGUAGES
\ingroup i18n_extension

The list of languages to install translations for.

*/

/*!

\qbuild_variable TS_DIR
\ingroup i18n_extension

The directory to store .ts files in (leave empty for the project directory).

*/

/*!

\qbuild_variable EXTRA_TS_FILES
\ingroup i18n_extension

*/

var lupdate_silent = "-silent "; // must end with a space if non-empty
//lupdate_silent = "";

function i18n_init()
{
    if ( project.absName.match(/\{file\}\/$/) )
        return;
###
    CONFIG*=installs
    QMAKE.FINALIZE.i18n.CALL = i18n_finalize
    QMAKE.FINALIZE.i18n.RUN_BEFORE_ME = defaults qtopia
    QMAKE.FINALIZE.i18n.RUN_AFTER_ME = rules cpp_compiler
###
}

function i18n_finalize()
{
    if (!project.config("i18n"))
        return;

    if ( project.property("STRING_LANGUAGE").isEmpty() &&
         project.property("LANGUAGES").isEmpty() )
    {
        return;
    }

###
    isEmpty(TRTARGET) {
        TRTARGET=$$TARGET
        equals(TEMPLATE,lib):if(!plugin|if(plugin:!quicklaunch)):TRTARGET=lib$$TRTARGET
    }

    # A useful define (since it is the name given to the .ts files)
    DEFINES+=QTOPIA_TRTARGET=$$define_string($$TRTARGET)

    isEmpty(TS_DIR):TS_DIR=$$path(.,project)
    AVAILABLE_LANGUAGES*=$$LANGUAGES
    TRANSLATIONS=$$LANGUAGES
    TRANSLATIONS*=$$STRING_LANGUAGE
###

    if ( project.property("I18N.LINSTALL").strValue() == "" ) {
        var script = project.property("I18N.LINSTALL");
        var file = project.buildFile("QtopiaSdk:/src/build/bin/linstall").filesystemPath();
        script.setValue(file);
        if ( script.strValue() == "" ) {
            project.warning("Unable to locate linstall script needed to handle i18n.");
            return;
        }
    }

    if ( project.property("I18N.NCT_LUPDATE").strValue() == "" ) {
        var script = project.property("I18N.NCT_LUPDATE");
        var file = project.buildFile("QtopiaSdk:/src/build/bin/nct_lupdate").filesystemPath();
        script.setValue(file);
        if ( script.strValue() == "" ) {
            project.warning("Unable to locate nct_lupdate script needed to handle i18n.");
            return;
        }
    }

    if ( project.property("I18N.THEME_LUPDATE").strValue() == "" ) {
        var script = project.property("I18N.THEME_LUPDATE");
        var file = project.buildFile("QtopiaSdk:/src/build/bin/theme_lupdate").filesystemPath();
        script.setValue(file);
        if ( script.strValue() == "" ) {
            project.warning("Unable to locate theme_lupdate script needed to handle i18n.");
            return;
        }
    }

    if (i18n_get_translatables())
        i18n_handle_translatables();
    i18n_extra_ts_files();
    i18n_hints();
}

function i18n_get_translatables()
{
    var i18n = project.property("I18N");
    var translatables = i18n.property("TRANSLATABLES");
    var source_objects = i18n.property("SOURCE_OBJECTS");

    // Regular sources
    var objects = project.find("TYPE", "CPP_SOURCES");
    objects.push(project.find("TYPE", "SOURCES"));
    // Conditional sources that were not enabled
    objects.push(project.find("TYPE", "CONDITIONAL_SOURCES"));
    for ( var ii in objects )
        source_objects.unite(objects[ii]);

    // Non-translatable sources
    objects = project.find("TYPE", "NON_TRANSLATABLE");
    for (var ii in objects)
        source_objects.remove(objects[ii]);

    var list = source_objects.value();
    for (var ii in list) {
        var obj = qbuild.object(list[ii]);
        translatables.property("HEADERS").unite(obj.property("HEADERS").value());
        translatables.property("HEADERS").unite(obj.property("PRIVATE_HEADERS").value());
        translatables.property("HEADERS").unite(obj.property("SEMI_PRIVATE_HEADERS").value());
        translatables.property("SOURCES").unite(obj.property("SOURCES").value());
        translatables.property("FORMS").unite(obj.property("FORMS").value());
    }
    if (translatables.property("HEADERS").isEmpty() &&
        translatables.property("SOURCES").isEmpty() &&
        translatables.property("FORMS").isEmpty() )
    {
        return false;
    }
    return true;
}

function i18n_handle_translatables()
{
    var translatables = project.property("I18N.TRANSLATABLES");
    var string_language = project.property("STRING_LANGUAGE");
    var all_languages = project.property("AVAILABLE_LANGUAGES");
    var trtarget = project.property("TRTARGET").strValue();
    var srcdir = project.property("TS_DIR").strValue();

    // The rule to create .ts files
    var rule = project.rule()
    var cmd = new Array();
    cmd.push("rm -f translatables.pro");

    var list = new Array("HEADERS", "SOURCES", "FORMS");
    for (var ii in list) {
        var varname = list[ii];
        var input = translatables.property(varname).value();
        for (var ii in input)
            cmd.push("echo \""+varname+"+="+source_file(input[ii])+"\" >>translatables.pro");
    }

    var list = all_languages.value();
    for (var ii in list) {
        if (list[ii] == string_language.strValue()) continue; // not the string language
        var ts = srcdir+"/"+trtarget+"-"+list[ii]+".ts";
        cmd.push("echo \"TRANSLATIONS+="+ts+"\" >>translatables.pro");
    }
    for (var ii in cmd)
        rule.commands.append("#(ve)"+cmd[ii]);
    var command = "$$HOST_QT_BINS/lupdate "+lupdate_silent+"$$path(translatables.pro,generated)";
    rule.commands.append("#(eh)echo $$shellQuote("+command+")");
    rule.commands.append("#(e)cd "+srcdir+";"+command);
    i18n_depend_on_qt(rule.name);

    if ( !string_language.isEmpty() ) {
        // Now do a -pluralonly run for the string language
        rule.commands.append("#(ve)grep -v '^TRANSLATIONS' translatables.pro >translatables2.pro");
        var ts = srcdir+"/"+trtarget+"-"+string_language.strValue()+".ts";
        rule.commands.append("#(ve)echo \"TRANSLATIONS+="+ts+"\" >>translatables2.pro");
        command = "$$HOST_QT_BINS/lupdate "+lupdate_silent+"-pluralonly $$path(translatables2.pro,generated)";
        rule.commands.append("#(eh)echo $$shellQuote("+command+")");
        rule.commands.append("#(e)cd "+srcdir+";"+command);
    }

    var lupdate = i18n_get_lupdate();
    lupdate.prerequisiteActions.append(rule.name);

    // linstall
    var linstall = project.property("I18N.LINSTALL").strValue();
    var languages = project.property("LANGUAGES");
    if (languages.isEmpty()) {
        echo("i18n", "No LANGUAGES");
        return;
    }

    // The rule to install .qm files
    rule = project.rule();
    rule.commands.append(linstall+" $$TRTARGET $$shellQuote($$TRANSLATIONS) $$QTOPIA_IMAGE/i18n $$TS_DIR");
    i18n_depend_on_qt(rule.name);

    var image = installs_getImage();
    image.prerequisiteActions.append(rule.name);
    project.property("pkg.default_targets").unite(rule.name);
}

function i18n_get_lupdate()
{
    var lupdate = project.rule("lupdate");
    if (!lupdate.help)
        lupdate.help = "Create/update .ts files";
    return lupdate;
}

function i18n_hints()
{
    var objs = project.find("hint");

    for (var ii in objs) {
        var obj = qbuild.object(objs[ii]);
        var hint = obj.property("hint");
        var handled = false;
        if ( hint.contains("nct") ) {
            i18n_hint_nct(obj);
            handled = true;
        }
        if ( hint.contains("themecfg") ) {
            i18n_hint_themecfg(obj);
            handled = true;
        }
        if ( hint.contains("extra_ts") ) {
            i18n_hint_extra_ts(obj);
            handled = true;
        }
    }
}

/*!

\hint nct

*/
function i18n_hint_nct(obj)
{
    var data = {
        trtarget: {
            value: null,
            type: "single"
        },
        outdir: {
            optional: true,
            value: project.property("TS_DIR").strValue(),
            type: "existingFile"
        },
        files: {
            value: null,
            type: "existingFiles"
        }
    };
    if ( !installs.fetchdata(obj, data) ) return;

    var linstall = project.property("I18N.LINSTALL").strValue();
    if (!obj.property("hint").contains("content") || !data.trtarget.value.match(/^Qtopia/)) {
        // The rule to install .qm files
        var rule = project.rule("nct_linstall_"+obj.name);
        rule.commands.append("#(e)"+linstall+" "+data.trtarget.value+" $$shellQuote($$TRANSLATIONS) $$QTOPIA_IMAGE/i18n $$path(.,project)");
        i18n_depend_on_qt(rule.name);

        var image = installs_getImage();
        image.prerequisiteActions.append(rule.name);
        project.property("pkg.default_targets").unite(rule.name);
    }


    // The rule to create .ts files
    var rule = project.rule("nct_lupdate_"+obj.name);
    rule.inputFiles = data.files.value;
    var cmd = new Array();
    cmd.push("cd "+data.outdir.value);
    var nct_lupdate = project.property("I18N.NCT_LUPDATE").strValue();
    var command = nct_lupdate+" "+lupdate_silent;
    if ( obj.property("hint").contains("content") ) {
        if ( data.trtarget.value.match(/^Qtopia/) ) {
            command += " -depot $$path(/,project)";
        }
        command += " -content "+data.trtarget.value;
    }
    command += " $$shellQuote($$AVAILABLE_LANGUAGES) $$shellQuote($$STRING_LANGUAGE) $$[INPUT.ABS]";
    cmd.push(command);
    rule.commands.append("#(eh)echo $$shellQuote("+command+")");
    rule.commands.append("#(e)"+cmd.join(";"));
    i18n_depend_on_qt(rule.name);

    var lupdate = i18n_get_lupdate();
    lupdate.prerequisiteActions.append(rule.name);
    installs_process_depends(rule, obj);
}

/*!

\hint themecfg

*/
function i18n_hint_themecfg(obj)
{
    var data = {
        trtarget: {
            value: null,
            type: "single"
        },
        outdir: {
            optional: true,
            value: project.property("TS_DIR").strValue(),
            type: "existingFile"
        },
        files: {
            value: null,
            type: "existingFiles"
        }
    };
    if ( !installs.fetchdata(obj, data) ) return;

    var linstall = project.property("I18N.LINSTALL").strValue();
    // The rule to install .qm files
    var rule = project.rule("theme_linstall_"+obj.name);
    rule.commands.append("#(e)"+linstall+" "+data.trtarget.value+" $$shellQuote($$TRANSLATIONS) $$QTOPIA_IMAGE/i18n $$path(.,project)");
    i18n_depend_on_qt(rule.name);

    var image = installs_getImage();
    image.prerequisiteActions.append(rule.name);
    project.property("pkg.default_targets").unite(rule.name);

    // The rule to create .ts files
    var rule = project.rule("theme_lupdate_"+obj.name);
    rule.inputFiles = data.files.value;
    var cmd = new Array();
    cmd.push("cd "+data.outdir.value);
    var nct_lupdate = project.property("I18N.THEME_LUPDATE").strValue();
    var command = nct_lupdate+" "+lupdate_silent+
	"$$shellQuote($$AVAILABLE_LANGUAGES) $$shellQuote($$STRING_LANGUAGE) $$[INPUT.ABS]";
    cmd.push(command);
    rule.commands.append("#(eh)echo $$shellQuote("+command+")");
    rule.commands.append("#(e)"+cmd.join(";"));
    i18n_depend_on_qt(rule.name);

    var lupdate = i18n_get_lupdate();
    lupdate.prerequisiteActions.append(rule.name);
    installs_process_depends(rule, obj);
}

/*!

\hint extra_ts

*/
function i18n_hint_extra_ts(obj)
{
    var data = {
        file: {
            value: null,
            type: "single"
        },
        source: {
            value: null,
            type: "single"
        },
        outfile: {
            optional: true,
            value: null,
            type: "single"
        }
    };
    if ( !installs.fetchdata(obj, data) ) return;

    if ( !data.outfile.value )
        data.outfile.value = data.file.value;
    if ( data.outfile.value.match(/__LANG__/) )
        project.warning(obj.name+".outfile cannot contain __LANG__");

    // The rule to install .qm files
    var rule = project.rule("install_"+obj.name);
    var languages = project.property("LANGUAGES").value();
    for ( var ii in languages ) {
        var lang = languages[ii];
        rule.commands.append("#(e)$$MKSPEC.MKDIR $$QTOPIA_IMAGE/i18n/"+lang);
        var ts = qbuild.invoke("path",
            data.source.value.replace(/__LANG__/, lang)+"/"+data.file.value.replace(/__LANG__/, lang)+".ts",
            "existing");
        if ( project.filesystemFile(ts) ) {
            var qm = "$$QTOPIA_IMAGE/i18n/"+lang+"/"+data.outfile.value+".qm";
            rule.commands.append("$$HOST_QT_BINS/lrelease "+lupdate_silent+"-compress -nounfinished -removeidentical "+ts+" -qm "+qm);
        } else {
            rule.commands.append("#(eh)echo $$shellQuote(WARNING: "+ts+" is missing and cannot be installed.)");
        }
    }
    i18n_depend_on_qt(rule.name);

    var image = installs_getImage();
    image.prerequisiteActions.append(rule.name);
    project.property("pkg.default_targets").unite(rule.name);
}

// Legacy support, leverages the above extra_ts hint to do work
function i18n_extra_ts_files()
{
    var ts_files = project.property("EXTRA_TS_FILES");
    if (ts_files.isEmpty()) return;

    var list = ts_files.value();
    for ( var ii in list ) {
### ii list[ii]
        i18n.extra_ts_files_<0> [
            file=<1>
            source=/i18n/__LANG__
            hint=extra_ts
        ]
###
    }
}

function i18n_depend_on_qt(rulename)
{
    var rule = project.rule(rulename);
    if ( !project.config("system_qt") && project.config("in_build_tree") &&
         qbuild.root.isSubproject(project.sproject("/src/libraries/qt/qt")) )
    {
        rule.prerequisiteActions.append("/src/libraries/qt/qt");
    }
}

