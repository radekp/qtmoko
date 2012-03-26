TEMPLATE=lib
TARGET=qtopiabase
MODULE_NAME=qtopiabase
VERSION=4.0.0

CONFIG+=qtopia hide_symbols singleexec
QTOPIA=
MODULES*=mathlib
# Included directly because it is not a module
DEPENDS*=/src/3rdparty/libraries/dlmalloc::persisted
enable_dbus_ipc:QT*=dbus

STRING_LANGUAGE=en_US
AVAILABLE_LANGUAGES=$$QTOPIA_AVAILABLE_LANGUAGES
LANGUAGES=$$QTOPIA_LANGUAGES
TRANSLATION_SUBDIR=translations

pkg [
    name=libqtopiabase
    desc="Base library for Qt Extended."
    version=$$QTOPIA_VERSION
    license=$$QTOPIA_LICENSE
    maintainer=$$QTOPIA_MAINTAINER
]

x11 {
    SOURCEPATH+=/qtopiacore/qt/src/gui/embedded
    # If we hide symbols, QUnixSocket and QTransportAuth are hidden from Qt Extended!
    CONFIG-=hide_symbols
}

HEADERS=\
    custom.h\
    qabstractipcinterfacegroup.h\
    qabstractipcinterfacegroupmanager.h\
    qabstractipcinterface.h\
    qdawg.h\
    qglobalpixmapcache.h\
    qlog.h\
    qsignalintercepter.h\
    qslotinvoker.h\
    qsoundcontrol.h\
    qstorage.h\
    qtopiaabstractservice.h\
    qtopiachannel.h\
    qtopiaglobal.h\
    qtopiaipcadaptor.h\
    qtopiaipcenvelope.h\
    qtopiaipcmarshal.h\
    qtopialog.h\
    qtopialog-config.h\
    qtopianamespace.h\
    qtopiaservices.h\
    qtopiasxe.h\
    qtopiatimer.h\
    qtranslatablesettings.h\
    quniqueid.h\
    version.h\
    # Valuespace code
    qexpressionevaluator.h\
    qfilemonitor.h\
    qmallocpool.h\
    qpacketprotocol.h\
    qsystemlock.h\
    qtopiailglobal.h\
    qvaluespace.h

PRIVATE_HEADERS=\
    qactionconfirm_p.h\
    qmemoryfile_p.h\
    # Valuespace code
    qfixedpointnumber_p.h

SEMI_PRIVATE_HEADERS=\
    testslaveinterface_p.h\
    qcopenvelope_p.h

SOURCES=\
    qactionconfirm.cpp\
    qabstractipcinterfacegroup.cpp\
    qabstractipcinterfacegroupmanager.cpp\
    qcopenvelope.cpp\
    qdawg.cpp\
    qlog.cpp\
    qmemoryfile.cpp\
    qmemoryfile_unix.cpp\
    qsignalintercepter.cpp\
    qslotinvoker.cpp\
    qsoundcontrol.cpp\
    qstorage.cpp\
    qtopiaabstractservice.cpp\
    qtopiachannel.cpp\
    qtopiaipcadaptor.cpp\
    qtopiaipcenvelope.cpp\
    qtopiaipcmarshal.cpp\
    qtopialog.cpp\
    qtopianamespace.cpp\
    qtopiaservices.cpp\
    qtopiasxe.cpp\
    qtopiatimer.cpp\
    qtranslatablesettings.cpp\
    quniqueid.cpp\
    # Valuespace code
    applayer.cpp\
    qexpressionevaluator.cpp\
    qfilemonitor.cpp\
    qfixedpointnumber.cpp\
    qmallocpool.cpp\
    qpacketprotocol.cpp\
    qsystemlock.cpp\
    qvaluespace.cpp

DBUS_IPC [
    TYPE=CONDITIONAL_SOURCES
    CONDITION=enable_dbus_ipc
    SOURCES=qabstractipcinterface_dbus.cpp
]

IPC [
    TYPE=CONDITIONAL_SOURCES
    CONDITION=!enable_dbus_ipc
    SOURCES=qabstractipcinterface.cpp
]

QWS [
    TYPE=CONDITIONAL_SOURCES
    CONDITION=!x11
    PRIVATE_HEADERS=\
        qsharedmemorycache_p.h
    SOURCES=\
        qsharedmemorycache.cpp
]

X11 [
    TYPE=CONDITIONAL_SOURCES
    CONDITION=x11
    HEADERS=\
        qcopchannel_x11.h
    PRIVATE_HEADERS=\
        qcopchannel_x11_p.h \
        qunixsocket_p.h \
        qunixsocketserver_p.h
    SOURCES=\
        qcopchannel_x11.cpp \
        qunixsocket.cpp \
        qunixsocketserver.cpp \
        qglobalpixmapcache_x11.cpp
]

X11_SXE [
    TYPE=CONDITIONAL_SOURCES
    CONDITION=x11:enable_sxe
    HEADERS=\
        qtransportauth_qws.h\
        qtransportauthdefs_qws.h
    PRIVATE_HEADERS=\
        qtransportauth_qws_p.h
    MOC_COMPILE_EXCEPTIONS=\
        qtransportauth_qws_p.h
    SOURCES=\
        qtransportauth_qws.cpp
]

HEADERS+=$$path(custom-qtopia.h,generated)
MOC_IGNORE+=$$path(custom-qtopia.h,generated)
SOURCES+=$$path(custom-qtopia.cpp,generated)

# Install rules

defbtn [
    hint=image
    files=etc/defaultbuttons.conf
    path=/etc
]

