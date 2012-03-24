TEMPLATE=lib
TARGET=qofono
MODULE_NAME=qofono
VERSION=1.0.0

CONFIG+=qtopia hide_symbols singleexec

QT+=dbus

DEFINES+=QOFONO_LIBRARY
DEFINES+=QTMOKO

pkg [
    name=qofono
    desc="ofono.org bindings for QtMoko."
    version=$$QTOPIA_VERSION
    license=$$QTOPIA_LICENSE
    maintainer=$$QTOPIA_MAINTAINER
]

SOURCES += qofonodbusabstractinterface.cpp \
    qofonosimmanager.cpp \
    qofonoconnectionmanager.cpp \
    qofonosupplementaryservices.cpp \
    qofonodbuspendingcall.cpp \
    qofonoutil.cpp \
    qofonopushnotification.cpp \
    qofonocellbroadcast.cpp \
    qofononetworkregistration.cpp \
    qofonomessagemanager.cpp \
    qofonoradiosettings.cpp \
    qofonovoicecallmanager.cpp \
    qofonophonebook.cpp \
    qofonomanager.cpp \
    qofonomodem.cpp \
    qofonovoicecall.cpp \
    qofononetworkoperator.cpp \
    qofonoobject.cpp

HEADERS += qofonovoicecallmanager.h \
    qofonophonebook.h \
    qofonosupplementaryservices.h \
    qofonoradiosettings.h \
    qofonodbuspendingcall.h \
    qofonodbusabstractinterface.h \
    qofonoconnectionmanager.h \
    qofonoutil.h \
    qofonodbuspendingreply.h \
    qofonopushnotification.h \
    qofonocellbroadcast.h \
    qofonostringmap.h \
    qofononetworkregistration.h \
    qofonomessagemanager.h \
    qofonosimmanager.h \
    qofonomanager.h \
    qofonomodem.h \
    qofonovoicecall.h \
    qofononetworkoperator.h \
    qofonoobject.h \
    qofonoobjectlist.h