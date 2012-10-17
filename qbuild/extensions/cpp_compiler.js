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

\extension cpp_compiler

The cpp_compiler extension handles interaction with the C/C++ compiler.

\tableofcontents

\section1 Rules

The cpp_compiler creates the following rules.

\table
\header \o Rule name \o User rule \o Description
\row \o objects \o Yes \o
\row \o clean_objects \o Yes \o
\row \o force_clean_objects \o Yes \o
\row \o target \o Yes \o
\row \o compiler_depends_depends \o No \o
\row \o compiler_source_depends \o No \o
\endtable

\section1 Variables

\table
\header \o Type \o Name \o Description
\row \o Input
     \o TARGETDIR
     \o Output directory for linked binaries.  If omitted the project
        directory is used.
\row \o Input
     \o TARGET
     \o Target name.
\row \o Input
     \o TYPE == COMPILER_CONFIG
     \o Used to propagate compiler configuration
\row \o Input
     \o COMPILER.OUTPUT
     \o exe, lib or staticlib

\row \o Output
     \o COMPILER.CFLAGS
     \o
\row \o Output
     \o COMPILER.CXXFLAGS
     \o
\row \o Output
     \o COMPILER.DEFINES
     \o
\row \o Output
     \o COMPILER.LFLAGS
     \o

\row \o Output
     \o COMPILER.INCLUDEPATH.DEPENDS_RULES
     \o
\row \o Output
     \o COMPILER.INCLUDEPATH
     \o

\row \o Output
     \o COMPILER.SOURCE_DEPENDS
     \o
\row \o Output
     \o COMPILER.SOURCE_DEPENDS_RULES
     \o

\row \o Output
     \o COMPILER.LIBS
     \o
\row \o Output
     \o COMPILER.LIBS.DEPENDS_RULES
     \o

\row \o Output
     \o COMPILER.TARGETDIR
     \o
\row \o Output
     \o COMPILER.OBJDIR
     \o
\row \o Output
     \o COMPILER.DEPDIR
     \o

\row \o Output
     \o COMPILER.TARGET
     \o
\row \o Output
     \o COMPILER.VERSION
     \o (lib/staticlib)
\row \o Output
     \o COMPILER.STATICLIB
     \o (staticlib)
\row \o Output
     \o COMPILER.SHAREDLIB
     \o (lib)
\row \o Output
     \o COMPILER.SHAREDLIB_SONAME
     \o (lib)
\row \o Output
     \o COMPILER.EXECUTABLE
     \o (exe)

\row \o Output
     \o COMPILER.ARGS_TEST_FILE
     \o
     \endtable

*/

/*!

\qbuild_variable TARGET
\brief The TARGET variable names the output binary.

This variable must be set. It names the output binary. For plugins, lib$${TARGET}.so is created. For libraries, lib$${TARGET}.so.$$VERSION is created.

*/

/*!

\qbuild_variable SOURCES
\brief The SOURCES variable selects source files used to build the target binary.

*/

/*!

\qbuild_variable VERSION
\brief The VERSION variable is used to set the library version.

This variable sets the version for libraries. Library are created as lib$${TARGET}.so.$$VERSION.

*/

/*!

\qbuild_variable SOURCEPATH
\brief The SOURCEPATH variable is used to locate source code.

This variable is like qmake's VPATH but it also applies to includes (-I).

*/

/*!

\qbuild_variable VPATH
\brief The VPATH variable is used to locate source code.

You should use \l SOURCEPATH in most instances since it updates the includes (-I) at the same time.

*/

/*!

\qbuild_variable LIBS
\brief The LIBS variable is used to add libs to the link line.

Note that you should not use this variable to express a dependency. Use \l MODULES instead and
let the other project declare the LIBS values.

*/

function cpp_compiler_init()
{
###
    CONFIG*=conditional_sources
    QMAKE.FINALIZE.cpp_compiler.CALL = cpp_compiler_finalize
    QMAKE.FINALIZE.cpp_compiler.RUN_AFTER_ME = rules
    QMAKE.FINALIZE.cpp_compiler.RUN_BEFORE_ME = depends defaults mkspec conditional_sources
    TYPE*=CPP_SOURCES COMPILER_CONFIG
    CONFIG+=link_test
###
    var paths = project.paths();
    for (var ii in paths)
        project.property("INCLUDEPATH.PROJECT").unite(paths[ii].filesystemDir());
}

function cpp_compiler_config()
{
    // Basic mkspec defined (usually) flags
###
    COMPILER.LINK = $$MKSPEC.CC

    COMPILER.CFLAGS += $$MKSPEC.CFLAGS
    COMPILER.CXXFLAGS += $$MKSPEC.CXXFLAGS
    COMPILER.LFLAGS += $$MKSPEC.LFLAGS
    LINK_TEST.LFLAGS += $$MKSPEC.LFLAGS
    LINK_TEST.LFLAGS += -rdynamic

    embedded:!enable_rtti {
        # dynamic_cast<>() is not allowed (no RTTI)
        DEFINES+=QT_NO_DYNAMIC_CAST
        COMPILER.CXXFLAGS+=$$MKSPEC.CXXFLAGS_DISABLE_RTTI
        !enable_exceptions:COMPILER.CXXFLAGS+=$$MKSPEC.CXXFLAGS_DISABLE_EXCEPTIONS
    }

    warn_on {
        COMPILER.CFLAGS += $$MKSPEC.CFLAGS_WARN_ON
        COMPILER.CXXFLAGS += $$MKSPEC.CXXFLAGS_WARN_ON
    } else {
        COMPILER.CFLAGS += $$MKSPEC.CFLAGS_WARN_OFF
        COMPILER.CXXFLAGS += $$MKSPEC.CXXFLAGS_WARN_OFF
    }

    release|optimize {
        COMPILER.CFLAGS += $$MKSPEC.CFLAGS_OPTBASE $$MKSPEC.CFLAGS_OPTMORE
        COMPILER.CXXFLAGS += $$MKSPEC.CXXFLAGS_OPTBASE $$MKSPEC.CXXFLAGS_OPTMORE
    }

    release {
        COMPILER.CFLAGS += $$MKSPEC.CFLAGS_RELEASE
        COMPILER.CXXFLAGS += $$MKSPEC.CXXFLAGS_RELEASE
        COMPILER.LFLAGS += $$MKSPEC.LFLAGS_RELEASE
        LINK_TEST.LFLAGS += $$MKSPEC.LFLAGS_RELEASE
    }

    debug {
        COMPILER.CFLAGS += $$MKSPEC.CFLAGS_DEBUG
        COMPILER.CXXFLAGS += $$MKSPEC.CXXFLAGS_DEBUG
        COMPILER.LFLAGS += $$MKSPEC.LFLAGS_DEBUG
        LINK_TEST.LFLAGS += $$MKSPEC.LFLAGS_DEBUG
    }

    equals(COMPILER.OUTPUT,lib)|if(equals(COMPILER.OUTPUT,staticlib):use_pic) {
        COMPILER.CFLAGS += $$MKSPEC.CFLAGS_PIC
        COMPILER.CXXFLAGS += $$MKSPEC.CFLAGS_PIC
    }

    equals(COMPILER.OUTPUT,lib) {
        COMPILER.LFLAGS += $$MKSPEC.LFLAGS_SHLIB
        equals(COMPILER.REDUCE_EXPORTS,1):hide_symbols {
            COMPILER.CFLAGS += $$MKSPEC.CFLAGS_HIDESYMS
            COMPILER.CXXFLAGS += $$MKSPEC.CXXFLAGS_HIDESYMS
        }
    }

    !isEmpty(MKSPEC.RPATH) {
        enable_rpath:embedded {
            COMPILER.LFLAGS+=$$MKSPEC.RPATH$$QTOPIA_PREFIX/lib
            LINK_TEST.LFLAGS+=$$MKSPEC.RPATH$$QTOPIA_PREFIX/lib
        } else {
            COMPILER.LFLAGS+=$$MKSPEC.RPATH$$QTDIR/lib
            COMPILER.LFLAGS+=$$MKSPEC.RPATH$$path(QtopiaSdk:/lib/host,generated)
            LINK_TEST.LFLAGS+=$$MKSPEC.RPATH$$QTDIR/lib
            LINK_TEST.LFLAGS+=$$MKSPEC.RPATH$$path(QtopiaSdk:/lib/host,generated)
        }
    }
###

    // Project defined flags
    var compiler = project.property("COMPILER");
    var defines = compiler.property("DEFINES");
    var include = compiler.property("INCLUDEPATH");
    var cflags = compiler.property("CFLAGS");
    var cxxflags = compiler.property("CXXFLAGS");
    var lflags = compiler.property("LFLAGS");
    var libs = compiler.property("LIBS");
    var libs_depends = compiler.property("LIBS.DEPENDS_RULES");
    var source_deps = compiler.property("SOURCE_DEPENDS");
    var source_deps_rules = compiler.property("SOURCE_DEPENDS_RULES");

    var objects = project.find("TYPE", "COMPILER_CONFIG");

    {
        var list = project.property("MKSPEC.INCLUDEPATH").value();
        for ( var ii in list ) {
            include.unite("-I"+list[ii]);
        }
    }

    for (var ii in objects) {
        var obj = qbuild.object(objects[ii]);

        if (obj.isProperty("DEFINES")) {
            var defines_list = obj.property("DEFINES").value();
            for (var jj in defines_list)
                defines.append("-D" + defines_list[jj]);
        }

        if (obj.isProperty("INCLUDEPATH.GENERATED")) {
            var prop = obj.property("INCLUDEPATH.GENERATED");
            var include_list = prop.value();
            for (var jj in include_list)
                include.append("-I" + include_list[jj]);
            if (prop.isProperty("DEPENDS_RULES"))
                source_deps_rules.unite(prop.property("DEPENDS_RULES").value());
        }

        if (obj.isProperty("INCLUDEPATH.PROJECT")) {
            var prop = obj.property("INCLUDEPATH.PROJECT");
            var include_list = prop.value();
            for (var jj in include_list)
                include.append("-I" + include_list[jj]);
            if (prop.isProperty("DEPENDS_RULES"))
                source_deps_rules.unite(prop.property("DEPENDS_RULES").value());
        }

        if (obj.isProperty("SOURCEPATH")) {
            // The include path should include all the source paths
            var prop = obj.property("SOURCEPATH");
            var include_list = prop.value();
            for ( var jj in include_list ) {
                var source_paths;
                if ( new File(include_list[jj]).isRelative() ) {
                    source_paths = new Array();
                    var paths = project.paths(include_list[jj]);
                    for ( var kk in paths ) {
                        source_paths.push(paths[kk].filesystemPath());
                    }
                } else {
                    source_paths = solution.pathMappings(include_list[jj]);
                }
                for ( var kk in source_paths ) {
                    include.append("-I" + source_paths[kk]);
                }
            }
        }

        if (obj.isProperty("INCLUDEPATH")) {
            var prop = obj.property("INCLUDEPATH");
            var include_list = prop.value();
            for (var jj in include_list)
                include.append("-I" + include_list[jj]);
            if (prop.isProperty("DEPENDS_RULES"))
                source_deps_rules.unite(prop.property("DEPENDS_RULES").value());
        }

        if (obj.isProperty("LIBS"))  {
            var prop = obj.property("LIBS");
            libs.append(prop.value());
            if (prop.isProperty("DEPENDS_RULES"))
                libs_depends.unite(prop.property("DEPENDS_RULES").value());
        }

        if (obj.isProperty("CFLAGS"))
            cflags.append(obj.property("CFLAGS").value());

        if (obj.isProperty("CXXFLAGS"))
            cxxflags.append(obj.property("CXXFLAGS").value());

        if (obj.isProperty("LFLAGS"))
            lflags.append(obj.property("LFLAGS").value());

        if (obj.isProperty("SOURCE_DEPENDS"))
            source_deps.unite(obj.property("SOURCE_DEPENDS").value());

        if (obj.isProperty("SOURCE_DEPENDS_RULES"))
            source_deps_rules.unite(obj.property("SOURCE_DEPENDS_RULES").value());
    }

    libs_depends.append("#(oh)headers");
}

function cpp_compiler_target_names(targetdir)
{
    var target = project.property("TARGET").strValue();
    if (!target)
        target = "default";

    project.property("COMPILER.TARGET").setValue(target);

    var output = project.property("COMPILER.OUTPUT").strValue();
    if (output == "lib") {
        var version = project.property("VERSION").strValue();
        var vNum = new Array(3);
        vNum[0] = "1";
        vNum[1] = "0";
        vNum[2] = "0";
        if (version) {
            var vsplit = version.split(".");
            for (var ii = 0; ii < vsplit.length && ii < 3; ++ii)
                vNum[ii] = vsplit[ii];
        }
### vNum[0] vNum[1] vNum[2]
        COMPILER.OUTPUT = lib

        COMPILER.VERSION = <0>.<1>.<2>
        COMPILER.VERSION.1 = <0>
        COMPILER.VERSION.2 = <0>.<1>
        COMPILER.VERSION.3 = <0>.<1>.<2>

        plugin {
            COMPILER.SHAREDLIB_SONAME = \
                lib$${COMPILER.TARGET}.so
        } !isEmpty(MKSPEC.LFLAGS_SONAME) {
            COMPILER.SHAREDLIB_SONAME = \
                lib$${COMPILER.TARGET}.so.$${COMPILER.VERSION.1}
        } else {
            COMPILER.SHAREDLIB_SONAME = \
                lib$${COMPILER.TARGET}.so.$${COMPILER.VERSION.3}
        }

        !isEmpty(MKSPEC.LFLAGS_SONAME) {
            COMPILER.LFLAGS += $$MKSPEC.LFLAGS_SONAME$$COMPILER.SHAREDLIB_SONAME
        }

        COMPILER.SHAREDLIB = \
            $${COMPILER.TARGETDIR}/lib$${COMPILER.TARGET}.so
        COMPILER.TARGETFILE=$$COMPILER.SHAREDLIB
        !plugin {
        COMPILER.SHAREDLIBS = \
            $${COMPILER.SHAREDLIB}.$${COMPILER.VERSION.3} \
            $${COMPILER.SHAREDLIB}.$${COMPILER.VERSION.2} \
            $${COMPILER.SHAREDLIB}.$${COMPILER.VERSION.1}
        }

        COMPILER.SHAREDLIBS += \
            $${COMPILER.SHAREDLIB}

###
    } else if (output == "staticlib") {

###
        COMPILER.OUTPUT = staticlib
        COMPILER.STATICLIB = $${COMPILER.TARGETDIR}/lib$${COMPILER.TARGET}.a
        COMPILER.TARGETFILE=$$COMPILER.STATICLIB
###

    } else {
        // exe
###
        COMPILER.OUTPUT = exe
        COMPILER.EXECUTABLE = \
            $${COMPILER.TARGETDIR}/$${COMPILER.TARGET}
        COMPILER.TARGETFILE=$$COMPILER.EXECUTABLE
###

    }
}

function cpp_compiler_paths()
{
    var targetdir = project.property("TARGETDIR").strValue();
    if (!targetdir)
        targetdir = ".";
    targetdir = project.buildPath(targetdir);
    project.property("COMPILER.TARGETDIR").setValue(targetdir);
    var objdir = qbuild.invoke("path", ".obj", "generated");
    project.property("COMPILER.OBJDIR").setValue(objdir);
    project.property("COMPILER.DEPDIR").setValue(objdir);


    var rule = project.rule("ensure_objdir");
    rule.outputFiles = objdir;
    rule.commands = "#(e)$$MKSPEC.MKDIR $$[OUTPUT.0]";
}

function cpp_compiler_c_type(filename)
{
    var file = File(filename);
    if (file.extension() == "c")
        return "CC";
    else
        return "CXX";
}

/*
  Create the compiler command line variables:
    QBUILD_CXX_IMP
    QBUILD_CC_IMP

  Also creates a rule, cpp_compiler_args_test, that updates the argument test file
  $${COMPILER.OBJDIR}/qbuild.o.args (persisted in COMPILER.ARGS_TEST_FILE)
*/
function cpp_compiler_object_args(objdir)
{
###
    QBUILD_CXX_IMP = "#(eh)echo $$MKSPEC.CXX $$[INPUT.0]" \
                     "#(Et)$$MKSPEC.CXX -MMD -MF $$[OUTPUT.1] -c $${COMPILER.CXXFLAGS} $${COMPILER.DEFINES} $${COMPILER.INCLUDEPATH} -o $$[OUTPUT.0] $$[INPUT.0]"
    QBUILD_CC_IMP = "#(eh)echo $$MKSPEC.CC $$[INPUT.0]" \
                    "#(Et)$$MKSPEC.CC -MMD -MF $$[OUTPUT.1] -c $${COMPILER.CFLAGS} $${COMPILER.DEFINES} $${COMPILER.INCLUDEPATH} -o $$[OUTPUT.0] $$[INPUT.0]"

    QBUILD_CC_PP_IMP = "#(eh)echo $$MKSPEC.CC -E $$[OUTPUT.0.ABS]" \
                    "#(Et)$$MKSPEC.CC -E $${COMPILER.CFLAGS} $${COMPILER.DEFINES} $${COMPILER.INCLUDEPATH} -o $$[OUTPUT.0] $$[INPUT.0]"
    QBUILD_CXX_PP_IMP = "#(eh)echo $$MKSPEC.CXX -E $$[OUTPUT.0.ABS]" \
                    "#(Et)$$MKSPEC.CXX -E $${COMPILER.CFLAGS} $${COMPILER.DEFINES} $${COMPILER.INCLUDEPATH} -o $$[OUTPUT.0] $$[INPUT.0]"

    QBUILD_CC_ASM_IMP = "#(eh)echo $$MKSPEC.CC -S $$[OUTPUT.0.ABS]" \
                    "#(Et)$$MKSPEC.CC -S $${COMPILER.CFLAGS} $${COMPILER.DEFINES} $${COMPILER.INCLUDEPATH} -o $$[OUTPUT.0] $$[INPUT.0]"
    QBUILD_CXX_ASM_IMP = "#(eh)echo $$MKSPEC.CXX -S $$[OUTPUT.0.ABS]" \
                    "#(Et)$$MKSPEC.CXX -S $${COMPILER.CFLAGS} $${COMPILER.DEFINES} $${COMPILER.INCLUDEPATH} -o $$[OUTPUT.0] $$[INPUT.0]"

    QBUILD_CPP_ARGS_TEST = "$$MKSPEC.CXX $${COMPILER.CXXFLAGS} $${COMPILER.DEFINES} $${COMPILER.INCLUDEPATH};" \
                           "$$MKSPEC.CC $${COMPILER.CCFLAGS} $${COMPILER.DEFINES} $${COMPILER.INCLUDEPATH}"
###

    create_args_rule({
        name: "cpp_compiler_args_test",
        file: objdir+"/qbuild.o.args",
        contents: project.property("QBUILD_CPP_ARGS_TEST").strValue(),
        prereq: "#(oh)ensure_objdir"
    });
    project.property("COMPILER.ARGS_TEST_FILE").setValue(objdir+"/qbuild.o.args");
}

/*
   For each source file to be built we create three rules:
      1. The rule to actually compile the file into an object.  This rule
         depends on the completion of the dependency rule.
      2. The dependency rule pulls in all the dependency information for the
         given file.  It does this by depending on the generation of the .dep
         file and then by processing and including this file.
      3. The dependency file generation rule.  Runs the compiler in -MM mode to
         create a file with dependency information.
 */

/*
   Creates a rule to build a source object based on the \a filename.
 */
function cpp_compiler_object_rule(filename, sourcedesc, objdir)
{
    var file = File(filename);
    var type = cpp_compiler_c_type(filename);
    var name = file.stripextension().name();

    var dependencyFileName = cpp_compiler_dep_file(filename);
    var outputFileName = objdir + "/" + name + ".o";
    var ppFileName = objdir + "/" + name + (type=="CC"?".i":".ii");
    var asmFileName = objdir + "/" + name + ".s";

    var compileRule = project.rule(basename(outputFileName));
    compileRule.outputFiles = outputFileName;
    compileRule.outputFiles.append(dependencyFileName);
    compileRule.inputFiles = filename;
    compileRule.inputFiles.append("#(f)$$include_depends_rule($$[OUTPUT.1.ABS])");
    if (sourcedesc != null && sourcedesc.isProperty("DEPENDS_DEPENDS"))
        compileRule.inputFiles.append(sourcedesc.property("DEPENDS_DEPENDS").value());
    compileRule.inputFiles.append("#(f)$${COMPILER.ARGS_TEST_FILE}");
    compileRule.commands = project.property("QBUILD_" + type + "_IMP").value();
    compileRule.prerequisiteActions.append("#(oh)ensure_objdir");
    compileRule.inputFiles.append(project.property("COMPILER.SOURCE_DEPENDS").value());
    compileRule.prerequisiteActions.append("compiler_depends_depends");
    compileRule.prerequisiteActions.append("compiler_source_depends");
    compileRule.category = "Compiler";

    var ppRule = project.rule(basename(ppFileName));
    ppRule.outputFiles = ppFileName;
    ppRule.inputFiles = filename;
    ppRule.inputFiles.append("#(f)$$include_depends_rule($$[OUTPUT.1.ABS])");
    ppRule.inputFiles.append("#(f)$${COMPILER.ARGS_TEST_FILE}");
    ppRule.commands = project.property("QBUILD_" + type + "_PP_IMP").value();
    ppRule.prerequisiteActions.append("#(oh)ensure_objdir");
    ppRule.category = "Compiler";

    var asmRule = project.rule(basename(asmFileName));
    asmRule.outputFiles = asmFileName;
    asmRule.inputFiles = filename;
    asmRule.inputFiles.append("#(f)$$include_depends_rule($$[OUTPUT.1.ABS])");
    asmRule.inputFiles.append("#(f)$${COMPILER.ARGS_TEST_FILE}");
    asmRule.commands = project.property("QBUILD_" + type + "_ASM_IMP").value();
    asmRule.prerequisiteActions.append("#(oh)ensure_objdir");
    asmRule.category = "Compiler";


    if (sourcedesc != null && sourcedesc.isProperty("DEPENDS")) {
        compileRule.inputFiles.unite(sourcedesc.property("DEPENDS").value());
        ppRule.inputFiles.unite(sourcedesc.property("DEPENDS").value());
    }

    project.property("COMPILER.OBJECTS").unite(outputFileName);
    project.property("COMPILER.EXTRA_OUTPUTS").unite(ppFileName);
    project.property("COMPILER.EXTRA_OUTPUTS").unite(asmFileName);
    project.property("COMPILER.DEPFILES").unite(dependencyFileName);
}

function cpp_compiler_objects(objdir)
{
    var sources = project.find("TYPE", "CPP_SOURCES");
    sources.push(project.find("TYPE", "SOURCES"));

    for (var ii in sources) {
        var baseobj = qbuild.object(sources[ii]);
        var obj = baseobj.property("SOURCES");
        var source_files = obj.value();

        var is_generated = baseobj.property("TYPE").contains("GENERATED_SOURCES");

        for (var jj in source_files){
            var filename = null;

            if (is_generated) {
                filename = source_files[jj];
            } else {
                filename = source_file(source_files[jj], baseobj);

                if (!filename)  {
                    project.warning("Unable to find file " + source_files[jj]);
                    continue;
                }
            }

            if ( cpp_compiler_c_type(filename) == "CXX" ) {
###
                CONFIG*=QBUILD_PROJECT_CONTAINS_CXX
                COMPILER.LINK=$$MKSPEC.CXX
###
            }

            var sourcedesc = null;
            if (obj.isProperty("~" + jj))
                sourcedesc = obj.property("~" + jj);

            cpp_compiler_object_rule(filename, sourcedesc, objdir);
        }
    }
}

function cpp_compiler_external_rules()
{
###
    QBUILD_COMPILER_CLEAN  = "$$MKSPEC.DEL_FILE $${COMPILER.OBJECTS} $${COMPILER.EXTRA_OUTPUTS}"
    QBUILD_COMPILER_CLEAN += "$$MKSPEC.DEL_FILE $${COMPILER.DEPFILES}"
    QBUILD_COMPILER_CLEAN += "$$MKSPEC.DEL_FILE $${COMPILER.ARGS_TEST_FILE}"
    QBUILD_COMPILER_CLEAN_FORCE  = "$$MKSPEC.DEL_FILE $${COMPILER.OBJDIR}/*.o"
    QBUILD_COMPILER_CLEAN_FORCE += "$$MKSPEC.DEL_FILE $${COMPILER.DEPDIR}/*.d"
    QBUILD_COMPILER_CLEAN_FORCE += "$$MKSPEC.DEL_FILE $${COMPILER.DEPDIR}/*.i"
    QBUILD_COMPILER_CLEAN_FORCE += "$$MKSPEC.DEL_FILE $${COMPILER.DEPDIR}/*.ii"
    QBUILD_COMPILER_CLEAN_FORCE += "$$MKSPEC.DEL_FILE $${COMPILER.DEPDIR}/*.s"
    QBUILD_COMPILER_CLEAN_FORCE += "$$MKSPEC.DEL_FILE $${COMPILER.ARGS_TEST_FILE}"

    !isEmpty(COMPILER.OBJECTS) {
        COMPILER.RULES.objects.TYPE = RULE
        COMPILER.RULES.objects.help = "Generate all C/C++ objects"
        COMPILER.RULES.objects.inputFiles = $${COMPILER.OBJECTS}
        COMPILER.RULES.objects.prerequisiteActions += "#(oh)headers";

        COMPILER.RULES.clean_objects.TYPE = RULE
        COMPILER.RULES.clean_objects.help = "Remove compiler generated objects"
        COMPILER.RULES.clean_objects.commands = $$QBUILD_COMPILER_CLEAN

        COMPILER.RULES.clean.TYPE = RULE
        COMPILER.RULES.clean.prerequisiteActions += clean_objects
    }

    COMPILER.RULES.force_clean_objects.TYPE = RULE
    COMPILER.RULES.force_clean_objects.help = "Remove all *.o and *.d files"
    COMPILER.RULES.force_clean_objects.commands = $$QBUILD_COMPILER_CLEAN_FORCE

    COMPILER.RULES.force_clean.TYPE = RULE
    COMPILER.RULES.force_clean.prerequisiteActions += force_clean_objects
###
}

function cpp_compiler_internal_rules()
{
###
    QBUILD_PROJECT_CONTAINS_CXX:COMPILER.LFLAGS*=$$MKSPEC.CXXFLAGS
    else:COMPILER.LFLAGS*=$$MKSPEC.CFLAGS

    QBUILD_LINK_MKDIR = "#(e)$$MKSPEC.MKDIR $$[OTHER.0]"
    QBUILD_LINK_EXE_IMPL = "#(eh)echo $$COMPILER.LINK $$[OUTPUT.0]"\
                           "#(E)$$COMPILER.LINK $${COMPILER.LFLAGS} -o $$[OUTPUT.0] $$[INPUT] $${COMPILER.LIBS}"
    QBUILD_LINK_EXE_TEST = "$$COMPILER.LINK $${COMPILER.LFLAGS} $${COMPILER.LIBS}"

    QBUILD_LINK_TEST_IMPL = "#(eh)echo $$COMPILER.LINK $$[OUTPUT.0]"\
                            "#(E)$$COMPILER.LINK $${LINK_TEST.LFLAGS} -o $$[OUTPUT.0] $$[INPUT] $${COMPILER.LIBS}"

    QBUILD_LINK_LIB_IMPL = "#(eh)echo $$COMPILER.LINK $$[OUTPUT.0]"\
                           "#(e)$$MKSPEC.DEL_FILE $${COMPILER.TARGETDIR}/lib$${COMPILER.TARGET}.*"\
                           "#(e)$$MKSPEC.DEL_FILE $$[OUTPUT.0] $$[OUTPUT.1] $$[OUTPUT.2] $$[OUTPUT.3]"\
                           "#(e)$$MKSPEC.DEL_FILE $${COMPILER.TARGETDIR}/lib$${COMPILER.TARGET}.*"\
                           "#(E)$$COMPILER.LINK $${COMPILER.LFLAGS} -o $$[OUTPUT.0] $$[INPUT] $${COMPILER.LIBS}"
    QBUILD_LINK_LIB_TEST = "$$COMPILER.LINK $${COMPILER.LFLAGS} $${COMPILER.LIBS}"

    !plugin {
        QBUILD_LINK_LIB_IMPL += "$$MKSPEC.SYMBOLIC_LINK $$basename($$[OUTPUT.0]) $$[OUTPUT.1]"\
                                "$$MKSPEC.SYMBOLIC_LINK $$basename($$[OUTPUT.0]) $$[OUTPUT.2]"\
                                "$$MKSPEC.SYMBOLIC_LINK $$basename($$[OUTPUT.0]) $$[OUTPUT.3]"
    }


    QBUILD_LINK_STATICLIB_IMPL = "#(eh)echo $$MKSPEC.AR $$[OUTPUT.0]"\
                                 "#(E)$$MKSPEC.AR $$[OUTPUT.0] $$[INPUT]"
    QBUILD_LINK_STATICLIB_TEST = "$$MKSPEC.AR"

    QBUILD_CLEAN_LIB_IMPL = "#(ne)$$MKSPEC.DEL_FILE $${COMPILER.TARGETDIR}/lib$${COMPILER.TARGET}.*"
    QBUILD_CLEAN_APP_IMPL = "#(ne)$$MKSPEC.DEL_FILE $${COMPILER.EXECUTABLE}"
    QBUILD_TARGETDEPS=cpp_compiler_target

    COMPILER.RULES.cpp_compiler_target [
        TYPE = RULE
        inputFiles = $${COMPILER.OBJECTS}
        other = $${COMPILER.TARGETDIR}
        prerequisiteActions += $${COMPILER.LIBS.DEPENDS_RULES}
        prerequisiteActions += cpp_compiler_link_args_test
        prerequisiteActions += target_pre # for the install_target implicit dependency
        commands = $$QBUILD_LINK_MKDIR
        category = Linker
    ]

    equals(COMPILER.OUTPUT,lib) {
        QBUILD_CLEAN_IMPL=$$QBUILD_CLEAN_LIB_IMPL
        QBUILD_LINK_ARGS_TEST=$$QBUILD_LINK_LIB_TEST
        link_test:QBUILD_TARGETDEPS+=link_test
        !plugin {
            COMPILER.RULES.cpp_compiler_target [
                outputFiles = \
                    $${COMPILER.SHAREDLIB}.$${COMPILER.VERSION.3} \
                    $${COMPILER.SHAREDLIB}.$${COMPILER.VERSION.2} \
                    $${COMPILER.SHAREDLIB}.$${COMPILER.VERSION.1}
            ]
        }
        COMPILER.RULES.cpp_compiler_target [
            outputFiles += $${COMPILER.SHAREDLIB}
            commands += $$QBUILD_LINK_LIB_IMPL
        ]
    }

    equals(COMPILER.OUTPUT,staticlib) {
        QBUILD_CLEAN_IMPL=$$QBUILD_CLEAN_LIB_IMPL
        QBUILD_LINK_ARGS_TEST=$$QBUILD_LINK_STATICLIB_TEST
        link_test:QBUILD_TARGETDEPS+=link_test
        # Not for singleexec
        enable_singleexec:singleexec:QBUILD_TARGETDEPS-=link_test
        COMPILER.RULES.cpp_compiler_target [
            outputFiles = $${COMPILER.STATICLIB}
            commands += $$QBUILD_LINK_STATICLIB_IMPL
        ]
    }

    equals(COMPILER.OUTPUT,exe) {
        QBUILD_CLEAN_IMPL=$$QBUILD_CLEAN_APP_IMPL
        QBUILD_LINK_ARGS_TEST=$$QBUILD_LINK_EXE_TEST
        COMPILER.RULES.cpp_compiler_target [
            outputFiles = $${COMPILER.EXECUTABLE}
            commands += $$QBUILD_LINK_EXE_IMPL
        ]
    }

    COMPILER.RULES.target [
        TYPE=RULE
        help="Compile and link target"
    ]

    COMPILER.RULES.target_post [
        TYPE=RULE
        prerequisiteActions=$$QBUILD_TARGETDEPS
    ]

    COMPILER.RULES.remove_target [
        TYPE=RULE
        other=$$COMPILER.RULES.cpp_compiler_target.outputFiles
        commands=\
            "#(eh)echo rm $$[OTHER.0]"\
            $$QBUILD_CLEAN_IMPL
    ]

    COMPILER.RULES.clean [
        TYPE=RULE
        prerequisiteActions+=remove_target
    ]

    COMPILER.RULES.force_clean [
        TYPE=RULE
        prerequisiteActions+=remove_target
    ]

    COMPILER.RULES.relink [
        TYPE=RULE
        help="Relink the target"
        prerequisiteActions=remove_target target
        serial=true
    ]

    COMPILER.RULES.link_test [
        TYPE=RULE
        prerequisiteActions=cpp_compiler_target
        inputFiles=$$COMPILER.OBJECTS
        outputFiles=$$path(.,generated)/link_test
        other=$$path(.,generated)
        commands=$$QBUILD_LINK_MKDIR\
                 $$QBUILD_LINK_TEST_IMPL
        category = Linker
    ]

    QBUILD_PROJECT_CONTAINS_CXX {
        COMPILER.RULES.link_test.inputFiles += $$path(/extensions/link_test.cpp,existing)
    } else {
        COMPILER.RULES.link_test.inputFiles += $$path(/extensions/link_test.c,existing)
    }
###

    // Re-link the target if its link line changes.
    var objdir = project.property("COMPILER.OBJDIR").strValue();
    create_args_rule({
        name: "cpp_compiler_link_args_test",
        file: objdir+"/qbuild.link.args",
        contents: project.property("QBUILD_LINK_ARGS_TEST").strValue(),
        prereq: "#(oh)ensure_objdir"
    });
}

function cpp_compiler_depends_depends()
{
    var rule = project.rule("compiler_depends_depends");
    rule.inputFiles = project.property("COMPILER.DEPENDS_DEPENDS").value();
    rule = project.rule("compiler_source_depends");
    rule.prerequisiteActions.append(project.property("COMPILER.SOURCE_DEPENDS_RULES").value());
}

function cpp_compiler_finalize()
{
    if (!project.config("cpp_compiler") || project.property("TEMPLATE").strValue() == "blank")
        return;

    setup_target_rules();

    // This is needed so that we can apply the appropriate RPATH
    if ( project.config("qt") && !project.config("embedded") )
        qt_get_qtdir();
    cpp_compiler_config();
    cpp_compiler_paths();

    var objdir = project.property("COMPILER.OBJDIR").strValue();
    var targetdir = project.property("COMPILER.TARGETDIR").strValue();

    cpp_compiler_target_names(targetdir);

    cpp_compiler_depends_depends();

    cpp_compiler_object_args(objdir);
    cpp_compiler_objects(objdir);

    cpp_compiler_external_rules();
    cpp_compiler_internal_rules();
}

function cpp_compiler_dep_file(file)
{
    var name = basename(file);
    var objdir = qbuild.invoke("path", ".obj", "generated");
    var dependencyFileName = objdir + "/" + name + ".d";
    return dependencyFileName;
}

