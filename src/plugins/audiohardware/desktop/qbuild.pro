TEMPLATE=plugin
TARGET=desktopaudiohardware

PLUGIN_FOR=qtopia
PLUGIN_TYPE=audiohardware

CONFIG+=qtopia singleexec
QTOPIA*=audio comm

STRING_LANGUAGE=en_US
AVAILABLE_LANGUAGES=$$QTOPIA_AVAILABLE_LANGUAGES
LANGUAGES=$$QTOPIA_LANGUAGES
TRANSLATION_SUBDIR=translations

pkg [
    name=desktop-audiohardware
    desc="Desktop audioharware plugin for Qt Extended."
    version=$$QTOPIA_VERSION
    license=$$QTOPIA_LICENSE
    maintainer=$$QTOPIA_MAINTAINER
]

HEADERS=\
    desktopaudioplugin.h

SOURCES=\
    desktopaudioplugin.cpp

