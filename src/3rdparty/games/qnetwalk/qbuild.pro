# This is an application
TEMPLATE=app

TARGET=qnetwalk

CONFIG+=qtopia

# Input
HEADERS=cell.h mainwindow.h
SOURCES=cell.cpp main.cpp mainwindow.cpp
RESOURCES=pics/pics.qrc
DEFINES+=QTOPIA_PHONE

STRING_LANGUAGE=en_US
AVAILABLE_LANGUAGES=$$QTOPIA_AVAILABLE_LANGUAGES
LANGUAGES=$$QTOPIA_LANGUAGES
TRANSLATION_SUBDIR=translations

pkg [
    name=qnetwalk
    desc="QNetwalk"
    version=$$QTOPIA_VERSION
    license=$$QTOPIA_LICENSE
    maintainer=$$QTOPIA_MAINTAINER
]

target [
    hint=sxe
    domain=trusted
]

desktop [
    hint=desktop
    files=qnetwalk.desktop
    path=/apps/Games
]

pics [
    hint=pics
    files=pics/*
    path=/pics/qnetwalk
]

sounds [
    hint=image
    files=sounds/*
    path=/sounds/qnetwalk
]


