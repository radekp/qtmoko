TEMPLATE=plugin
TARGET=ogg-content

PLUGIN_FOR=qtopia
PLUGIN_TYPE=content

CONFIG+=qtopia singleexec
LIBS += -lvorbisfile

pkg [
    name=ogg-content
    desc="OGG content plugin for Qt Extended."
    version=$$QTOPIA_VERSION
    license=$$QTOPIA_LICENSE
    maintainer=$$QTOPIA_MAINTAINER
]

HEADERS=\
    oggcontentplugin.h

SOURCES=\
    oggcontentplugin.cpp

