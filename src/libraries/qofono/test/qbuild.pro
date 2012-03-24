TEMPLATE=app
TARGET=qofonotest

CONFIG+=qtopia
DEFINES+=QTOPIA
QT+=dbus
INCLUDEPATH+=$$shellQuote($$path(,project))/..

# I18n info
STRING_LANGUAGE=en_US
LANGUAGES=en_US

# Package info
pkg [
    name=qofonotest
    desc="oFono test program for QtMoko"
    license=GPLv2
    version=1.0
    maintainer="Radek Polak <psonek2@seznam.cz>"
]

# Input files
SOURCES += main.cpp \
    mainwindow.cpp \
    ofonoutil.cpp \
    ../qofonodbusabstractinterface.cpp \
    ../qofonosimmanager.cpp \
    ../qofonoconnectionmanager.cpp \
    ../qofonosupplementaryservices.cpp \
    ../qofonodbuspendingcall.cpp \
    ../qofonoutil.cpp \
    ../qofonopushnotification.cpp \
    ../qofonocellbroadcast.cpp \
    ../qofononetworkregistration.cpp \
    ../qofonomessagemanager.cpp \
    ../qofonoradiosettings.cpp \
    ../qofonovoicecallmanager.cpp \
    ../qofonophonebook.cpp \
    ../qofonomanager.cpp \
    ../qofonomodem.cpp \
    ../qofonovoicecall.cpp \
    ../qofononetworkoperator.cpp \
    ../qofonoobject.cpp

HEADERS += mainwindow.h \
    ofonoutil.h \
    qtelephonynamespace.h \
    ../qofonovoicecallmanager.h \
    ../qofonophonebook.h \
    ../qofonosupplementaryservices.h \
    ../qofonoradiosettings.h \
    ../qofonodbuspendingcall.h \
    ../qofonodbusabstractinterface.h \
    ../qofonoconnectionmanager.h \
    ../qofonoutil.h \
    ../qofonodbuspendingreply.h \
    ../qofonopushnotification.h \
    ../qofonocellbroadcast.h \
    ../qofonostringmap.h \
    ../qofononetworkregistration.h \
    ../qofonomessagemanager.h \
    ../qofonosimmanager.h \
    ../qofonomanager.h \
    ../qofonomodem.h \
    ../qofonovoicecall.h \
    ../qofononetworkoperator.h \
    ../qofonoobject.h \
    ../qofonoobjectlist.h

FORMS += mainwindow.ui

# Install rules
target [
    hint=sxe
    domain=untrusted
]

desktop [
    hint=desktop
    files=qofonotest.desktop
    path=/apps/Applications
]

pics [
    hint=pics
    files=pics/*
    path=/pics/qofonotest
]

