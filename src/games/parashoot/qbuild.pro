TEMPLATE=app
TARGET=parashoot

CONFIG+=qtopia singleexec quicklaunch

STRING_LANGUAGE=en_US
AVAILABLE_LANGUAGES=$$QTOPIA_AVAILABLE_LANGUAGES
LANGUAGES=$$QTOPIA_LANGUAGES
UNIFIED_NCT_LUPDATE=1
TRANSLATION_SUBDIR=translations

pkg [
    name=parashoot
    desc="Shoot the parachutes game for Qt Extended."
    version=$$QTOPIA_VERSION
    license=$$QTOPIA_LICENSE
    maintainer=$$QTOPIA_MAINTAINER
]

HEADERS=\
    animateditem.h\
    interface.h\
    man.h\
    cannon.h\
    base.h\
    bullet.h\
    helicopter.h

SOURCES=\
    animateditem.cpp\
    interface.cpp\
    man.cpp\
    cannon.cpp\
    base.cpp\
    bullet.cpp\
    helicopter.cpp\
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
    files=parashoot.desktop
    path=/apps/Games
]

pics [
    hint=pics
    files=pics/*
    path=/pics/parashoot
]

sounds [
    hint=image
    files=sounds/*
    path=/sounds/parashoot
]

