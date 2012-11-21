TEMPLATE=lib
TARGET=qtopiaprinting
MODULE_NAME=qtopiaprinting
VERSION=4.0.0

CONFIG+=qtopia hide_symbols singleexec

STRING_LANGUAGE=en_US
AVAILABLE_LANGUAGES=$$QTOPIA_AVAILABLE_LANGUAGES
LANGUAGES=$$QTOPIA_LANGUAGES
UNIFIED_NCT_LUPDATE=1
TRANSLATION_SUBDIR=translations

pkg [
    name=printinglib
    desc="Printing library for Qt Extended."
    version=$$QTOPIA_VERSION
    license=$$QTOPIA_LICENSE
    maintainer=$$QTOPIA_MAINTAINER
]

FORMS=\
    printdialogbase.ui

HEADERS=\
    qprinterinterface.h

SOURCES=\
    qprinterinterface.cpp
           
UNIX [
    TYPE=CONDITIONAL_SOURCES
    CONDITION=unix:!x11
    PRIVATE_HEADERS=qprintdialogcreator_p.h
    SOURCES=qprintdialogcreator.cpp
]

