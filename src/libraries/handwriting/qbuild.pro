TEMPLATE=lib
TARGET=qmstroke
MODULE_NAME=handwriting
VERSION=4.0.0

CONFIG+=qtopia hide_symbols singleexec

STRING_LANGUAGE=en_US
AVAILABLE_LANGUAGES=$$QTOPIA_AVAILABLE_LANGUAGES
LANGUAGES=$$QTOPIA_LANGUAGES
TRANSLATION_SUBDIR=translations

pkg [
    name=libhandwriting
    desc="Multi-stroke gesture recognition library for Qt Extended."
    version=$$QTOPIA_VERSION
    license=$$QTOPIA_LICENSE
    maintainer=$$QTOPIA_MAINTAINER
]

HEADERS=\
    char.h\
    profile.h\
    signature.h\
    stroke.h

PRIVATE_HEADERS=\
    combining_p.h\

SOURCES=\
    char.cpp\
    combining.cpp\
    profile.cpp\
    signature.cpp\
    stroke.cpp

# Install rules

help [
    hint=help
    source=help
    files=*.html
]

