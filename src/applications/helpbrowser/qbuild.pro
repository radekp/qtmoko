TEMPLATE=app
CONFIG+=qtopia
TARGET=helpbrowser

CONFIG+=quicklaunch singleexec

STRING_LANGUAGE=en_US
AVAILABLE_LANGUAGES=$$QTOPIA_AVAILABLE_LANGUAGES
LANGUAGES=$$QTOPIA_LANGUAGES
UNIFIED_NCT_LUPDATE=1
TRANSLATION_SUBDIR=translations

pkg [
    name=helpbrowser
    desc="Help viewer application for Qt Extended."
    version=$$QTOPIA_VERSION
    license=$$QTOPIA_LICENSE
    maintainer=$$QTOPIA_MAINTAINER
]

HEADERS=\
    helpbrowser.h\
    helppreprocessor.h\
    navigationbar_p.h

SOURCES=\
    helpbrowser.cpp main.cpp\
    helppreprocessor.cpp\
    navigationbar_p.cpp

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

desktop [
    hint=desktop
    files=helpbrowser.desktop
    path=/apps/Applications
]

pics [
    hint=pics
    files=pics/helpbrowser/*
    path=/pics/helpbrowser
]

pics2 [
    hint=pics
    files=pics/help/*
    path=/pics/help
]

helpservice [
    hint=image
    files=services/Help/helpbrowser
    path=/services/Help
]

