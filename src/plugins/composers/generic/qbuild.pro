TEMPLATE=plugin
TARGET=genericcomposer

PLUGIN_FOR=qtopia
PLUGIN_TYPE=composers

CONFIG+=qtopia singleexec
QTOPIA*=mail
equals(QTOPIA_UI,home):MODULES*=homeui

STRING_LANGUAGE=en_US
AVAILABLE_LANGUAGES=$$QTOPIA_AVAILABLE_LANGUAGES
LANGUAGES=$$QTOPIA_LANGUAGES
TRANSLATION_SUBDIR=translations

pkg [
    name=generic-composer
    desc="Generic composer plugin for Qt Extended."
    version=$$QTOPIA_VERSION
    license=$$QTOPIA_LICENSE
    maintainer=$$QTOPIA_MAINTAINER
]

!enable_cell:DEFINES+=QTOPIA_NO_SMS

HEADERS=\
    genericcomposer.h\
    templatetext.h

SOURCES=\
    genericcomposer.cpp\
    templatetext.cpp

