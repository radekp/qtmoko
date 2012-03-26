TEMPLATE=app
TARGET=qasteroids

CONFIG+=qtopia singleexec quicklaunch

STRING_LANGUAGE=en_US
AVAILABLE_LANGUAGES=$$QTOPIA_AVAILABLE_LANGUAGES
LANGUAGES=$$QTOPIA_LANGUAGES
UNIFIED_NCT_LUPDATE=1
TRANSLATION_SUBDIR=translations

pkg [
    name=qasteroids
    desc="Asteroids game for Qt Extended."
    version=$$QTOPIA_VERSION
    license=$$QTOPIA_LICENSE
    maintainer=$$QTOPIA_MAINTAINER
]

HEADERS=\
    ledmeter.h\
    sprites.h\
    toplevel.h\
    view.h

SOURCES=\
    ledmeter.cpp\
    sprites.cpp\
    toplevel.cpp\
    view.cpp\
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
    files=qasteroids.desktop
    path=/apps/Games
]

pics [
    hint=pics
    files=pics/*
    path=/pics/qasteroids
]

sounds [
    hint=image
    files=sounds/*
    path=/sounds/qasteroids
]

