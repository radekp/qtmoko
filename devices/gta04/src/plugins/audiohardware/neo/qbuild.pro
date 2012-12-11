TEMPLATE=plugin
TARGET=neoaudiohardware

PLUGIN_FOR=qtopia
PLUGIN_TYPE=audiohardware

CONFIG+=qtopia
QTOPIA+=audio
enable_bluetooth:QTOPIA+=comm

HEADERS		=  neoaudioplugin.h
SOURCES	        =  neoaudioplugin.cpp

conf [
    hint=image
    files=a4/*.state
    path=/etc/alsa/a4
]

confa3 [
    hint=image
    files=a3/*.state
    path=/etc/alsa/a3
]
INSTALLS+=confa3
