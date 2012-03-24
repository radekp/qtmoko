TEMPLATE=lib
TARGET=qtopiaphonemodem
MODULE_NAME=qtopiaphonemodem
VERSION=4.0.0

CONFIG+=qtopia hide_symbols singleexec
QTOPIA*=phone

pkg [
    name=phonemodemlib
    desc="Phonemodem library for Qt Extended."
    version=$$QTOPIA_VERSION
    license=$$QTOPIA_LICENSE
    maintainer=$$QTOPIA_MAINTAINER
]

HEADERS=\
    qmodemservice.h\
    qmodemserviceplugin.h\
    qmodemnetworkregistration.h\
    qmodempreferrednetworkoperators.h\
    qmodemcallbarring.h\
    qmodemcallforwarding.h\
    qmodemcallsettings.h\
    qmodemsmsreader.h\
    qmodemsmssender.h\
    qmodempinmanager.h\
    qmodemcall.h\
    qmodemcallprovider.h\
    qmodemsimtoolkit.h\
    qmodemphonebook.h\
    qmodemcellbroadcast.h\
    qmodemservicenumbers.h\
    qmodemsupplementaryservices.h\
    qmodemsiminfo.h\
    qmodemindicators.h\
    qmodemsimfiles.h\
    qmodemsimgenericaccess.h\
    qmodemadviceofcharge.h\
    qmodemconfiguration.h\
    qmodemvibrateaccessory.h\
    qmodemrffunctionality.h\
    qmodemgprsnetworkregistration.h\
    qmodemcallvolume.h\
    qmodemllindicators.h

PRIVATE_HEADERS=\
    qmodempinmanager_p.h\
    qmodemsimfiles_p.h\
    qmodempppdmanager_p.h

SOURCES=\
    qmodemservice.cpp\
    qmodemserviceplugin.cpp\
    qmodemnetworkregistration.cpp\
    qmodempreferrednetworkoperators.cpp\
    qmodemcallbarring.cpp\
    qmodemcallforwarding.cpp\
    qmodemcallsettings.cpp\
    qmodemsmsreader.cpp\
    qmodemsmssender.cpp\
    qmodempinmanager.cpp\
    qmodemcall.cpp\
    qmodemcallprovider.cpp\
    qmodemsimtoolkit.cpp\
    qmodemphonebook.cpp\
    qmodemcellbroadcast.cpp\
    qmodemservicenumbers.cpp\
    qmodemsupplementaryservices.cpp\
    qmodemsiminfo.cpp\
    qmodemindicators.cpp\
    qmodemsimfiles.cpp\
    qmodemsimgenericaccess.cpp\
    qmodemadviceofcharge.cpp\
    qmodemconfiguration.cpp\
    qmodemvibrateaccessory.cpp\
    qmodemrffunctionality.cpp\
    qmodemgprsnetworkregistration.cpp\
    qmodempppdmanager.cpp\
    qmodemcallvolume.cpp\
    qmodemllindicators.cpp

