TEMPLATE=plugin
TARGET=option3g

PLUGIN_FOR=qtopia
PLUGIN_TYPE=network

CONFIG+=qtopia singleexec
QTOPIA*=comm
enable_cell:QTOPIA*=phone

STRING_LANGUAGE=en_US
AVAILABLE_LANGUAGES=$$QTOPIA_AVAILABLE_LANGUAGES
LANGUAGES=$$QTOPIA_LANGUAGES

enable_fso {
    MODULES*=qfso
} else {
    DEFINES+=QTOPIA_NO_FSO
}

pkg [
    name=option3g-network
    desc="Option 3g Dialing network plugin for QtMoko."
    version=$$QTOPIA_VERSION
    license=$$QTOPIA_LICENSE
    maintainer=$$QTOPIA_MAINTAINER
]

FORMS=\
    dialingbase.ui\
    advancedbase.ui

HEADERS=\
    option3gplugin.h\
    hsointerface.h\
    config.h\
    dialing.h\
    dialstring.h

SOURCES=\
    option3gplugin.cpp\
    hsointerface.cpp\
    config.cpp\
    dialing.cpp\
    dialstring.cpp

conf [
    hint=image
    files+=etc/network/hso.conf
    path=/etc/network
]
    
pics [
    hint=pics
    files=pics/*
    path=/pics/Network/option3g
]

icons [
    hint=pics
    files=icons/*
    path=/pics/Network/icons/option3g
]

