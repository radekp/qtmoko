TEMPLATE=plugin
TARGET=bluetoothprinting

PLUGIN_FOR=qtopia
PLUGIN_TYPE=qtopiaprinting

CONFIG+=qtopia singleexec
QTOPIA+=printing comm

STRING_LANGUAGE=en_US
AVAILABLE_LANGUAGES=$$QTOPIA_AVAILABLE_LANGUAGES
LANGUAGES=$$QTOPIA_LANGUAGES
TRANSLATION_SUBDIR=translations

pkg [
    name=bluetooth-printing
    desc="Bluetooth printing plugin for Qt Extended."
    version=$$QTOPIA_VERSION
    license=$$QTOPIA_LICENSE
    maintainer=$$QTOPIA_MAINTAINER
]

HEADERS=\
    bluetoothplugin.h\
    qbluetoothobexagent.h

SOURCES=\
    bluetoothplugin.cpp\
    qbluetoothobexagent.cpp

