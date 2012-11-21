requires(enable_cell)
TEMPLATE=plugin
TARGET=mmscomposer

PLUGIN_FOR=qtopia
PLUGIN_TYPE=composers

CONFIG+=qtopia singleexec
QTOPIA*=mail

STRING_LANGUAGE=en_US
AVAILABLE_LANGUAGES=$$QTOPIA_AVAILABLE_LANGUAGES
LANGUAGES=$$QTOPIA_LANGUAGES
TRANSLATION_SUBDIR=translations

pkg [
    name=mms-composer
    desc="MMS composer plugin for Qt Extended."
    version=$$QTOPIA_VERSION
    license=$$QTOPIA_LICENSE
    maintainer=$$QTOPIA_MAINTAINER
]

HEADERS=\
    mmscomposer.h\
    videoselector.h

SOURCES=\
    mmscomposer.cpp\
    videoselector.cpp

