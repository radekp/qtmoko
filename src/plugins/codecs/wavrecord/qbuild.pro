TEMPLATE=plugin
TARGET=wavrecord

PLUGIN_FOR=qtopia
PLUGIN_TYPE=codecs

CONFIG+=qtopia singleexec

STRING_LANGUAGE=en_US
AVAILABLE_LANGUAGES=$$QTOPIA_AVAILABLE_LANGUAGES
LANGUAGES=$$QTOPIA_LANGUAGES
TRANSLATION_SUBDIR=translations

contains(PROJECTS,3rdparty/libraries/gsm) {
    MODULES*=gsm
    DEFINES+=WAVGSM_SUPPORTED
}

pkg [
    name=wavrecord-codec
    desc="Wavrecord codec plugin for Qt Extended."
    version=$$QTOPIA_VERSION
    license=$$QTOPIA_LICENSE
    maintainer=$$QTOPIA_MAINTAINER
]

HEADERS=\
    wavrecord.h\
    wavrecordimpl.h

SOURCES=\
    wavrecord.cpp\
    wavrecordimpl.cpp

