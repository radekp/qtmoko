TEMPLATE=plugin
TARGET=neovendor

PLUGIN_FOR=qtopia
PLUGIN_TYPE=phonevendors

CONFIG+=qtopia
QTOPIA+=phonemodem

HEADERS		=  vendor_neo_p.h neoplugin.h
SOURCES	        =  vendor_neo.cpp neoplugin.cpp

settings [
    hint=image optional
    files=\
        etc/default/Trolltech/Modem.conf
    path=/etc/default/Trolltech
]

