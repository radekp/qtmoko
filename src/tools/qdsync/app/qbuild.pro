TEMPLATE=app
TARGET=qdsync

CONFIG+=qtopia singleexec
enable_singleexec:CONFIG+=quicklaunch
QTOPIA+=comm
MODULES*=qdsync_common crypt

STRING_LANGUAGE=en_US
AVAILABLE_LANGUAGES=$$QTOPIA_AVAILABLE_LANGUAGES
LANGUAGES=$$QTOPIA_LANGUAGES
TRANSLATION_SUBDIR=translations

pkg [
    name=synchronization
    desc="Synchronization application for Qt Extended."
    version=$$QTOPIA_VERSION
    license=$$QTOPIA_LICENSE
    maintainer=$$QTOPIA_MAINTAINER
    multi=tools/qdsync/common/qdsync_common
]

HEADERS=\
    qdsync.h\
    qcopbridge.h\
    syncauthentication.h\
    log.h

SOURCES=\
    main.cpp\
    qdsync.cpp\
    qcopbridge.cpp\
    syncauthentication.cpp

# Install rules

target [
    hint=sxe
    domain=trusted
]

desktop [
    hint=desktop nct
    trtarget=qdsync-nct
    files=qdsync.desktop
    path=/apps/Applications
]

help [
    hint=help
    source=help
    files=*.html
]

pics [
    hint=pics
    files=pics/*
    path=/pics/qdsync
]

usbservice [
    hint=image
    files=services/usbserial/qdsync
    path=/services/UsbGadget/Serial
]

usbservice2 [
    hint=image
    files=services/usbethernet/qdsync
    path=/services/UsbGadget/Ethernet
]

