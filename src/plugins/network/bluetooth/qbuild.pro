TEMPLATE=plugin
TARGET=bluetooth

PLUGIN_FOR=qtopia
PLUGIN_TYPE=network

CONFIG+=qtopia singleexec
QTOPIA*=comm

STRING_LANGUAGE=en_US
AVAILABLE_LANGUAGES=$$QTOPIA_AVAILABLE_LANGUAGES
LANGUAGES=$$QTOPIA_LANGUAGES
TRANSLATION_SUBDIR=translations

pkg [
    name=bluetoothdun-network
    desc="Bluetooth DUN network plugin for Qt Extended."
    version=$$QTOPIA_VERSION
    license=$$QTOPIA_LICENSE
    maintainer=$$QTOPIA_MAINTAINER
]

FORMS=\
    advancedbtbase.ui\
    dialingbtbase.ui

HEADERS=\
    bluetoothplugin.h\
    bluetoothimpl.h\
    config.h\
    btdialupdevice.h\
    configui.h

SOURCES=\
    bluetoothplugin.cpp\
    bluetoothimpl.cpp\
    config.cpp\
    btdialupdevice.cpp\
    configui.cpp

conf.hint=image
isEmpty(BLUETOOTH_NETWORK_CONFIGS):BLUETOOTH_NETWORK_CONFIGS=bluetoothDUN
for(l,BLUETOOTH_NETWORK_CONFIGS) {
    conf.files+=etc/network/$${l}.conf
}
conf.path=/etc/network

bin [
    hint=script
    files=btdun-network
    path=/bin
]

