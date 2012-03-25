# This is an application
TEMPLATE=app

# The binary name
TARGET=qtmaze

# This app uses Qtopia
CONFIG+=qtopia
DEFINES+=QTOPIA

# Build this app as a quicklauncher plugin
# You need to be using the QTOPIA_ADD_APPLICATION/QTOPIA_MAIN macros or this will not work!
#CONFIG+=quicklaunch

# Build this project into the singleexec binary
#CONFIG+=singleexec

# Specify the languages that make lupdate should produce .ts files for
STRING_LANGUAGE=en_US
AVAILABLE_LANGUAGES=$$QTOPIA_AVAILABLE_LANGUAGES
LANGUAGES=$$QTOPIA_LANGUAGES

# Package information (used for qbuild packages)
pkg [
    name=qtmaze
    desc="Ball-in-the-labyrinth game"
    license=GPLv3
    version=0.1.5
    maintainer="Anton Olkhovik <ant007h@gmail.com>"
]

# These are the source files that get built to create the application
FORMS=\
    formbase.ui

HEADERS=\
    form.h\
    renderarea.h\
    accelerometers.h\
    types.h\
    json.h\
    paramsloader.h\
    vibro.h

SOURCES=\
    main.cpp\
    form.cpp\
    renderarea.cpp\
    accelerometers.cpp\
    json.c\
    paramsloader.cpp\
    vibro.cpp

#RESOURCES = resources.qrc

# SXE information
target [
    hint=sxe
    domain=untrusted
]

# Install the launcher item. The metadata comes from the .desktop file
# and the path here.
desktop [
    hint=desktop
    files=qtmaze.desktop
    path=/apps/Games
]

# Install some pictures.
pics [
    hint=pics
    files=pics/qtmaze/*
    path=/pics/qtmaze
]

config [
    hint=image
    files=etc/qtmaze/*
    path=/etc/qtmaze
]

# Install help files
help [
    hint=help
    source=help
    files=qtmaze.html
]
