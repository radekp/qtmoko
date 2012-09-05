# -------------------------------------------------
# Project created by QtCreator 2009-04-14T22:32:40
# -------------------------------------------------
TARGET = neocontrol
TEMPLATE = app
QT += core \
    gui \
    network
LIBS += -lasound
SOURCES += main.cpp \
    neocontrol.cpp \
    mixerslider.cpp
HEADERS += neocontrol.h \
    mixerslider.h
