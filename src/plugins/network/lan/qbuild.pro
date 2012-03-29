TEMPLATE=plugin
TARGET=lan

PLUGIN_FOR=qtopia
PLUGIN_TYPE=network

CONFIG+=qtopia singleexec
QTOPIA*=comm

STRING_LANGUAGE=en_US
AVAILABLE_LANGUAGES=$$QTOPIA_AVAILABLE_LANGUAGES
LANGUAGES=$$QTOPIA_LANGUAGES
TRANSLATION_SUBDIR=translations

pkg [
    name=lan-network
    desc="LAN network plugin for Qt Extended."
    version=$$QTOPIA_VERSION
    license=$$QTOPIA_LICENSE
    maintainer=$$QTOPIA_MAINTAINER
]

FORMS=\
    wirelessbase.ui\
    wirelessencryptbase.ui\
    roamingbase.ui

HEADERS=\
    lanplugin.h\
    lan.h\
    config.h\
    wirelessconfig.h\
    encryptionconfig.h\
    wirelessscan.h\
    roamingconfig.h\
    roamingmonitor.h\
    wnet.h\
    wlanregistrationprovider.h\
    wirelessipconfig.h

SOURCES=\
    lanplugin.cpp\
    lan.cpp\
    config.cpp\
    wirelessconfig.cpp\
    encryptionconfig.cpp\
    wirelessscan.cpp\
    roamingconfig.cpp\
    roamingmonitor.cpp\
    wnet.cpp\
    wlanregistrationprovider.cpp\
    wirelessipconfig.cpp

conf.hint=image
isEmpty(LAN_NETWORK_CONFIGS):LAN_NETWORK_CONFIGS=lan lan-pcmcia wlan-pcmcia wlan
for(l,LAN_NETWORK_CONFIGS) {
    conf.files+=etc/network/$${l}.conf
}
conf.path=/etc/network

bin [
    hint=script
    files=lan-network
    path=/bin
]

pics [
    hint=pics
    files=pics/*
    path=/pics/Network/lan
]

icons [
    hint=pics
    files=icons/*
    path=/pics/Network/icons/lan
]

