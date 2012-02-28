CONFIG+=embedded
MODULE_NAME=qtopiacore
SUBDIRS=
include(common.pri)
build_qtopia_sqlite:MODULES*=sqlite

target_post [
    TYPE=RULE
    prerequisiteActions=sdk
]

configure [
    TYPE=RULE
    tests=\
        "$$not($$testFile(qbuild.configure_args,$$CONFIGURE_ARGS))"\
        # If a Makefile was removed, we need to re-run configure
        # Makefiles get removed when you abort a build while qmake is running
        "$$run_if_not_found($$[OUTPUT.1.ABS])"
    outputFiles=$$QTDIR/config.status makefiles
    commands=\
        "#(eh)echo $$shellQuote(Qtopia Core has changed. You need to run configure.)"\
        "$$error(Fatal error)"

]
add_input_files_to_rule(configure, $$path(/qtopiacore/qt/configure,existing))
add_input_files_to_rule(configure, $$path(/qtopiacore/qt/mkspecs,existing), "*.conf")
add_input_files_to_rule(configure, $$path(/qtopiacore/qt/mkspecs,existing), "*.pr[iof]")
add_input_files_to_rule(configure, $$path(/qtopiacore/qt/src,existing), "*.pr[iof]")
add_input_files_to_rule(configure, $$path(/qtopiacore/qt/qmake,existing), "*")
add_input_files_to_rule(configure, $$path(/qtopiacore/qt/config.tests,existing), "*")
add_input_files_to_rule(configure, $$path(/src/build/qt_patch,project), "*")
#print_rule(configure)

qt [
    TYPE=RULE
    help="Build Qtopia Core"
    prerequisiteActions="#(h)target_pre" configure
    inputFiles=makefiles
    outputFiles=$$qt_get_libs($$QTDIR)
    commands=\
        "#(eh)echo Checking if we need to build Qtopia Core..."\
        # Force hide all output (make -q doesn't actually suppress output?)
        "#(se)$$QT_PATCH_ENV $$MAKE -C $$QTDIR -q >/dev/null 2>&1; test $? -ne 0"\
        # Fix up the Qt Makefiles so they don't spew out "cd foo && make" lines
        "#(E)$$QTOPIACORE/fixmakefiles $$shellQuote($$QTDIR) $$[INPUT.0]"\
        "#(eh)echo $$MAKE -C $$QTDIR"\
        "#(e)$$QT_PATCH_ENV $$MAKE -C $$QTDIR"
]

QTLIBS=$$qt_get_libs($$QTDIR,$$SDKROOT/qtopiacore/target)

sdk [
    TYPE=RULE
    help="Install Qtopia Core into the SDK"
    prerequisiteActions=qt
    outputFiles=sdkdone
    commands=\
        "#(eh)echo $$MAKE -C $$QTDIR install"\
        "#(e)rm -rf $$SDKROOT/qtopiacore/target"
]
enable_singleexec {
    sdk.outputFiles+=singleexec.pri
    sdk.commands+=\
        # List the plugins we've built for singleexec builds
        "#(e):> $$[OUTPUT.1]"\
        "#(e)$$QT_PATCH_ENV $$MAKE -C $$QTDIR singleexec_pri"
}
sdk.outputFiles+=$$QTLIBS
sdk.commands+=\
        # Go ahead and install now
        "#(e)$$QT_PATCH_ENV $$MAKE -C $$QTDIR install"\
        # Put the doc symlink here so that assistant works
        "#(e)$$MKSPEC.DEL_FILE $$SDKROOT/qtopiacore/target/doc"\
        "#(e)$$MKSPEC.SYMBOLIC_LINK $$path(/qtopiacore/qt/doc,existing) $$SDKROOT/qtopiacore/target/doc"\
        # Sometimes Qt doesn't set the timestamps so set them now
        "#(e)$$QTOPIACORE/fixsdktimestamps $$shellQuote($$QTDIR) $$shellQuote($$SDKROOT/qtopiacore/target)"\
        "#(e):> $$[OUTPUT.0]"

clean [
    TYPE=RULE
    help="Clean Qtopia Core"
    commands=\
        "#(en)rm qbuild.configure_args makefiles"\
        "#(eh)echo $$MAKE clean"\
        "#(e)$$QT_PATCH_ENV $$MAKE -C $$QTDIR/src clean"
]

STRING_LANGUAGE=en_US
AVAILABLE_LANGUAGES=$$QTOPIA_AVAILABLE_LANGUAGES
LANGUAGES=$$QTOPIA_LANGUAGES
TRTARGET=qt
TS_DIR=$$path(/src/qt,project)

TRANSLATABLES.TYPE=CONDITIONAL_SOURCES
TRANSLATABLES.CONDITION=false
<script>
var translatables = project.property("TRANSLATABLES");
function do_files(prop, files)
{
    for ( var ii in files )
        translatables.property(prop).unite(files[ii].filesystemPath());
}
function getAllTranslatables(iter)
{
    do_files("SOURCES", iter.files("*.cpp"));
    do_files("HEADERS", iter.files("*.h"));
    do_files("FORMS", iter.files("*.ui"));
    var list = iter.paths("*");
    for ( var ii in list ) {
        if ( list[ii] != "qt3support" )
            getAllTranslatables(iter.cd(list[ii]));
    }
}
getAllTranslatables(new PathIterator(project.property("QT_DEPOT").strValue()+"/src"));
</script>

!enable_singleexec {
    libs [
        files=$$QTLIBS
        path=/lib
        depends=qt
        hint=image generated
    ]

    plugins [
        path=/qt_plugins
        commands=\
            "cp -Rf $$QTDIR/plugins/* $$QTOPIA_IMAGE/qt_plugins"
        depends=qt
        hint=image
    ]

    # No automatic stripping for these
    release:!isEmpty(MKSPEC.STRIP) {
        for(l,QTLIBS) {
            libs.commands+="$$MKSPEC.STRIP $$MKSPEC.STRIPFLAGS_SHLIB $$QTOPIA_IMAGE"$$libs.path"/"$$basename($$l)
        }
        plugins.commands+=\
            "#(ve)plugins=$(find "$$QTOPIA_IMAGE"/qt_plugins -type f);
             for p in $plugins; do
                 echo "$$MKSPEC.STRIP" "$$MKSPEC.STRIPFLAGS_SHLIB" $p
                 "$$MKSPEC.STRIP" "$$MKSPEC.STRIPFLAGS_SHLIB" $p
             done"
    }

    # The server does this in singleexec
    patchqt [
        commands=\
            "$$path(/src/build/bin/patchqt,generated) $$QTOPIA_IMAGE/lib/libQtCore.so.4 $$QTOPIA_PREFIX"
        depends=install_libs
        hint=image
    ]
}

fonts.files=$$QTOPIA_FONTS
fonts.path=/lib/fonts
fonts.hint=image

fontdir.path=/lib/fonts
fontdir.commands=\
    ":>$$QTOPIA_IMAGE/lib/fonts/fontdir"
fontdir.hint=image

# Non-persisted dependency used when building
depends [
    TYPE=DEPENDS
    EVAL=\
        "build_moc_rule.TYPE=RULE"\
        "build_moc_rule.prerequisiteActions+=""#(h)"$$project()"/target_post"""\
        "build_uic_rule.TYPE=RULE"\
        "build_uic_rule.prerequisiteActions+=""#(h)"$$project()"/target_post"""\
        "build_rcc_rule.TYPE=RULE"\
        "build_rcc_rule.prerequisiteActions+=""#(h)"$$project()"/target_post"""\
        "compiler_source_depends.TYPE=RULE"\
        "compiler_source_depends.prerequisiteActions+=""#(h)"$$project()"/target_post"""
]

# This matches the logic in qbuild/extensions/qt.js
defs [
    TYPE=DEPENDS PERSISTED SDK
    EVAL=\
        "QTDIR="$$SDKROOT/qtopiacore/target\
        "QTINC=$$QTDIR/include"\
        "INCLUDEPATH+=$$QTINC"\
        "QTLIBS=$$QTDIR/lib"\
        "QTDATA=$$QTDIR"
]

pdefs [
    TYPE=DEPENDS PERSISTED
    EVAL=\
        "QTINC_P="$$QTDIR"/include"\
        "INCLUDEPATH+=$$QTINC_P"\
        "QTLIBS_P="$$QTDIR"/lib"\
        "QTDATA_P="$$QTDIR
]

pkg.name=qtopiacore
pkg.desc="Qt for Embedded Linux"
pkg.version=$$QTE_VERSION

enable_singleexec {
    # When linking against Qt in singleexec builds, you get the plugins too (if you ask for them)
    singleexec_plugins [
        TYPE=DEPENDS
        EVAL=\
            "qt_static_plugins:LIBS+=`cat "$$path(singleexec.pri,generated)"`"
    ]
}

