TEMPLATE=app
TARGET=fifteen

CONFIG+=qtopia singleexec quicklaunch

STRING_LANGUAGE=en_US
AVAILABLE_LANGUAGES=$$QTOPIA_AVAILABLE_LANGUAGES
LANGUAGES=$$QTOPIA_LANGUAGES
UNIFIED_NCT_LUPDATE=1
TRANSLATION_SUBDIR=translations

pkg [
    name=fifteen
    desc="Sliding puzzle game for Qt Extended."
    version=$$QTOPIA_VERSION
    license=$$QTOPIA_LICENSE
    maintainer=$$QTOPIA_MAINTAINER
]

HEADERS=\
    fifteen.h

SOURCES=\
    fifteen.cpp\
    main.cpp

# Install rules

target [
    hint=sxe
    domain=trusted
]

desktop [
    hint=desktop
    files=fifteen.desktop
    path=/apps/Games
]

help [
    hint=help
    source=help
    files=*.html
]

pics [
    hint=pics
    files=pics/*
    path=/pics/fifteen
]

