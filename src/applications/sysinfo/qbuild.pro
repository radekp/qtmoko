TEMPLATE=app
CONFIG+=qtopia
TARGET=sysinfo

QTOPIA*=comm
enable_cell:QTOPIA*=phone
CONFIG+=quicklaunch singleexec

STRING_LANGUAGE=en_US
AVAILABLE_LANGUAGES=$$QTOPIA_AVAILABLE_LANGUAGES
LANGUAGES=$$QTOPIA_LANGUAGES
UNIFIED_NCT_LUPDATE=1
TRANSLATION_SUBDIR=translations

pkg [
    name=sysinfo
    desc="System information application for Qt Extended."
    version=$$QTOPIA_VERSION
    license=$$QTOPIA_LICENSE
    maintainer=$$QTOPIA_MAINTAINER
]

FORMS=\
    documenttypeselector.ui

HEADERS=\
    memory.h\
    graph.h\
    load.h\
    storage.h\
    versioninfo.h\
    sysinfo.h\
    dataview.h\
    securityinfo.h\
    networkinfo.h\
    cleanupwizard.h\
    deviceselector.h

SOURCES=\
    memory.cpp\
    graph.cpp\
    load.cpp\
    storage.cpp\
    versioninfo.cpp\
    sysinfo.cpp\
    main.cpp\
    dataview.cpp\
    securityinfo.cpp\
    networkinfo.cpp\
    cleanupwizard.cpp

CELL [
    TYPE=CONDITIONAL_SOURCES
    CONDITION=enable_cell
    HEADERS=siminfo.h modeminfo.h
    SOURCES=siminfo.cpp modeminfo.cpp
]

volatile [
    TYPE=CPP_SOURCES COMPILER_CONFIG
    SOURCES=versioninfo.cpp
]
!isEmpty(QT_CHANGE):volatile.DEFINES+=QT_CHANGE=$$define_string($$QT_CHANGE)
!isEmpty(QTOPIA_CHANGE):volatile.DEFINES+=QTOPIA_CHANGE=$$define_string($$QTOPIA_CHANGE)

# Always rebuild versioninfo.o so that the reported build date is correct
force_rebuild [
    TYPE=RULE
    outputFiles=force_rebuild $$path(versioninfo.cpp,existing)
    commands="#(e)true"
]

# Install rules

target [
    hint=sxe
    domain=trusted
]

help [
    hint=help
    source=help
    files=*.html
]

pics [
    hint=pics
    files=pics/*
    path=/pics/sysinfo
]

desktop [
    hint=desktop
    files=sysinfo.desktop
    path=/apps/Applications
]

cleanupservice [
    hint=image
    files=services/CleanupWizard/sysinfo
    path=/services/CleanupWizard
]

