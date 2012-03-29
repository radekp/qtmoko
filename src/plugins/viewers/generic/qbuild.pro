TEMPLATE=plugin
TARGET=genericviewer

PLUGIN_FOR=qtopia
PLUGIN_TYPE=viewers

CONFIG+=qtopia singleexec
QTOPIA*=mail
equals(QTOPIA_UI,home):MODULES*=homeui

STRING_LANGUAGE=en_US
AVAILABLE_LANGUAGES=$$QTOPIA_AVAILABLE_LANGUAGES
LANGUAGES=$$QTOPIA_LANGUAGES
TRANSLATION_SUBDIR=translations

pkg [
    name=generic-viewer
    desc="Generic viewer plugin for Qt Extended."
    version=$$QTOPIA_VERSION
    license=$$QTOPIA_LICENSE
    maintainer=$$QTOPIA_MAINTAINER
]

!enable_cell {
    DEFINES+=QTOPIA_NO_SMS
    !enable_voip:DEFINES+=QTOPIA_NO_DIAL_FUNCTION
}

HEADERS=\
    attachmentoptions.h\
    browser.h\
    genericviewer.h

SOURCES=\
    attachmentoptions.cpp\
    browser.cpp\
    genericviewer.cpp

