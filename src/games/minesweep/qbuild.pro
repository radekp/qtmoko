TEMPLATE=app
TARGET=minesweep

CONFIG+=qtopia singleexec quicklaunch

STRING_LANGUAGE=en_US
AVAILABLE_LANGUAGES=$$QTOPIA_AVAILABLE_LANGUAGES
LANGUAGES=$$QTOPIA_LANGUAGES
UNIFIED_NCT_LUPDATE=1
TRANSLATION_SUBDIR=translations

pkg [
    name=minesweeper
    desc="Minesweeper game for Qt Extended."
    version=$$QTOPIA_VERSION
    license=$$QTOPIA_LICENSE
    maintainer=$$QTOPIA_MAINTAINER
]

HEADERS=\
    minefield.h\
    minesweep.h

SOURCES=\
    minefield.cpp\
    minesweep.cpp\
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
    files=minesweep.desktop
    path=/apps/Games
]

pics [
    hint=pics
    files=pics/*
    path=/pics/minesweep
]

