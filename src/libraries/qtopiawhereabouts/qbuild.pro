TEMPLATE=lib
TARGET=qtopiawhereabouts
MODULE_NAME=qtopiawhereabouts
VERSION=4.0.0

CONFIG+=qtopia hide_symbols singleexec
LIBS += -lgps

STRING_LANGUAGE=en_US
AVAILABLE_LANGUAGES=$$QTOPIA_AVAILABLE_LANGUAGES
LANGUAGES=$$QTOPIA_LANGUAGES
UNIFIED_NCT_LUPDATE=1
TRANSLATION_SUBDIR=translations

pkg [
    name=whereaboutslib
    desc="Whereabouts library for Qt Extended."
    version=$$QTOPIA_VERSION
    license=$$QTOPIA_LICENSE
    maintainer=$$QTOPIA_MAINTAINER
]

HEADERS=\
    qwhereabouts.h\
    qwhereaboutscoordinate.h\
    qwhereaboutsupdate.h\
    qwhereaboutsfactory.h\
    qwhereaboutsplugin.h\
    qnmeawhereabouts.h\
    qgpsdwhereabouts.h\

PRIVATE_HEADERS=\
    qnmeawhereabouts_p.h

SOURCES=\
    qwhereabouts.cpp\
    qwhereaboutscoordinate.cpp\
    qwhereaboutsupdate.cpp\
    qwhereaboutsfactory.cpp\
    qwhereaboutsplugin.cpp\
    gpsdwhereabouts.cpp\
    qnmeawhereabouts.cpp

settings [
    hint=image
    files=etc/default/Trolltech/Whereabouts.conf
    path=/etc/default/Trolltech
]

