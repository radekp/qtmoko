TEMPLATE=app
CONFIG+=qtopia
TARGET=bluetooth

QTOPIA*=comm
CONFIG+=quicklaunch singleexec

STRING_LANGUAGE=en_US
AVAILABLE_LANGUAGES=$$QTOPIA_AVAILABLE_LANGUAGES
LANGUAGES=$$QTOPIA_LANGUAGES
UNIFIED_NCT_LUPDATE=1
TRANSLATION_SUBDIR=translations

pkg [
    name=bluetooth
    desc="Bluetooth FTP application for Qt Extended."
    version=$$QTOPIA_VERSION
    license=$$QTOPIA_LICENSE
    maintainer=$$QTOPIA_MAINTAINER
]

HEADERS=\
    dirdeleterdialog.h\
    mainwindow.h

SOURCES=\
    mainwindow.cpp\
    dirdeleterdialog.cpp\
    main.cpp

# Install rules

target [
    hint=sxe
    domain=trusted
]

desktop [
    hint=desktop
    files=bluetooth.desktop
    path=/apps/Applications
]

help [
    hint=help
    source=help
    files=*.html
]

