TEMPLATE=app
TARGET=mindbreaker

CONFIG+=qtopia singleexec quicklaunch

STRING_LANGUAGE=en_US
AVAILABLE_LANGUAGES=$$QTOPIA_AVAILABLE_LANGUAGES
LANGUAGES=$$QTOPIA_LANGUAGES
UNIFIED_NCT_LUPDATE=1
TRANSLATION_SUBDIR=translations

pkg [
    name=mindbreaker
    desc="Mindbreaker game for Qt Extended."
    version=$$QTOPIA_VERSION
    license=$$QTOPIA_LICENSE
    maintainer=$$QTOPIA_MAINTAINER
]

HEADERS=\
    mindbreaker.h

SOURCES=\
    mindbreaker.cpp\
    main.cpp

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
    files=mindbreaker.desktop
    path=/apps/Games
]

pics [
    hint=pics
    files=pics/*
    path=/pics/mindbreaker
]

