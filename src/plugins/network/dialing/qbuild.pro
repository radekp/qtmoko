TEMPLATE=plugin
TARGET=dialing

PLUGIN_FOR=qtopia
PLUGIN_TYPE=network

CONFIG+=qtopia singleexec
QTOPIA*=comm
enable_cell:QTOPIA*=phone

STRING_LANGUAGE=en_US
AVAILABLE_LANGUAGES=$$QTOPIA_AVAILABLE_LANGUAGES
LANGUAGES=$$QTOPIA_LANGUAGES
TRANSLATION_SUBDIR=translations

enable_fso {
    MODULES*=qfso
} else {
    DEFINES+=QTOPIA_NO_FSO
}

pkg [
    name=dialing-network
    desc="Dialing network plugin for Qt Extended."
    version=$$QTOPIA_VERSION
    license=$$QTOPIA_LICENSE
    maintainer=$$QTOPIA_MAINTAINER
]

FORMS=\
    dialingbase.ui\
    advancedbase.ui

HEADERS=\
    dialupplugin.h\
    dialup.h\
    config.h\
    dialing.h\
    advanced.h\
    dialstring.h

SOURCES=\
    dialupplugin.cpp\
    dialup.cpp\
    config.cpp\
    dialing.cpp\
    advanced.cpp\
    dialstring.cpp

conf.hint=image
isEmpty(DIALING_NETWORK_CONFIGS) {
    DIALING_NETWORK_CONFIGS=dialup dialupIR
    enable_cell:DIALING_NETWORK_CONFIGS+=dialupGPRS
}
for(l,DIALING_NETWORK_CONFIGS) {
    conf.files+=etc/network/$${l}.conf
}
conf.path=/etc/network


bin [
    hint=script
    files=\
        ppp-network\
        bin/qtopia-pppd-internal
    path=/bin
]

pics [
    hint=pics
    files=pics/*
    path=/pics/Network/dialup
]

icons [
    hint=pics
    files=icons/*
    path=/pics/Network/icons/dialup
]

