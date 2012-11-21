TEMPLATE=app
TARGET=mediaserver

CONFIG+=qtopia singleexec
QTOPIA*=media audio 
enable_telephony:QTOPIA*=phone

STRING_LANGUAGE=en_US
AVAILABLE_LANGUAGES=$$QTOPIA_AVAILABLE_LANGUAGES
LANGUAGES=$$QTOPIA_LANGUAGES
UNIFIED_NCT_LUPDATE=1
TRANSLATION_SUBDIR=translations

pkg [
    name=mediaserver
    desc="Media server for Qt Extended."
    version=$$QTOPIA_VERSION
    license=$$QTOPIA_LICENSE
    maintainer=$$QTOPIA_MAINTAINER
]

DEFINES+=CONFIGURED_ENGINES=$$define_string($$QTOPIAMEDIA_ENGINES)

# Give us a direct connection to the document system
DEFINES+=QTOPIA_DIRECT_DOCUMENT_SYSTEM_CONNECTION

equals(QTOPIA_SOUND_SYSTEM,alsa)|equals(QTOPIA_SOUND_SYSTEM,pulse):DEFINES+=QTOPIA_HAVE_ALSA

HEADERS=\
    sessionmanager.h\
    engineloader.h\
    buildermanager.h\
    buildernegotiator.h\
    urinegotiator.h\
    mediaagent.h\
    qsoundprovider.h\
    qtopiamediaprovider.h\
    mediacontrolserver.h\
    mediacontentserver.h\
    domainmanager.h\
    sessionmanagersession.h\
    mediaagentsession.h\
    drmsession.h\
    mediapowercontrol.h\
    mediavolumecontrol.h\
    qaudiointerfaceserver.h

SOURCES=\
    main.cpp\
    sessionmanager.cpp\
    engineloader.cpp\
    buildermanager.cpp\
    buildernegotiator.cpp\
    urinegotiator.cpp\
    mediaagent.cpp\
    qsoundprovider.cpp\
    qtopiamediaprovider.cpp\
    mediacontrolserver.cpp\
    mediacontentserver.cpp\
    domainmanager.cpp\
    sessionmanagersession.cpp\
    mediaagentsession.cpp\
    drmsession.cpp\
    mediapowercontrol.cpp\
    mediavolumecontrol.cpp\
    qaudiointerfaceserver.cpp

telephony [
    TYPE=CONDITIONAL_SOURCES
    CONDITION=enable_telephony
    HEADERS=callmonitor.h
    SOURCES=callmonitor.cpp
]

# Install rules

target [
    hint=sxe
    domain=trusted
]

mediaserverservice [
    hint=image
    files = services/MediaServer/mediaserver
    path = /services/MediaServer
]

domainmanager [
    hint=image
    files = etc/default/Trolltech/AudioDomains.conf
    path = /etc/default/Trolltech
]

