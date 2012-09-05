TEMPLATE=app
TARGET=neocontrol

CONFIG+=qtopia
DEFINES+=QTOPIA
LIBS += -lasound

# I18n info
STRING_LANGUAGE=en_US
LANGUAGES=en_US

# Package info
pkg [
    name=neocontrol
    desc="Openmoko control"
    license=GPLv2
    version=1.0
    maintainer="Radek Polak <psonek2@seznam.cz>"
]

# Input files
HEADERS=\
    neocontrol.h\
    mixerslider.h

SOURCES=\
    main.cpp\
    neocontrol.cpp\
    mixerslider.cpp

# Install rules
target [
    hint=sxe
    domain=untrusted
]

desktop [
    hint=desktop
    files=neocontrol.desktop
    path=/apps/Applications
]

pics [
    hint=pics
    files=pics/*
    path=/pics/neocontrol
]

