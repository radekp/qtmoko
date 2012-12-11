TEMPLATE=app
CONFIG+=qtopia
TARGET=mediaplayer

QTOPIA*=media
MODULES*=accelerometer
CONFIG+=quicklaunch singleexec
enable_pictureflow:MODULES*=pictureflow

STRING_LANGUAGE=en_US
AVAILABLE_LANGUAGES=$$QTOPIA_AVAILABLE_LANGUAGES
LANGUAGES=$$QTOPIA_LANGUAGES
UNIFIED_NCT_LUPDATE=1
TRANSLATION_SUBDIR=translations

pkg [
    name=mediaplayer
    desc="Media player application for Qt Extended."
    version=$$QTOPIA_VERSION
    license=$$QTOPIA_LICENSE
    maintainer=$$QTOPIA_MAINTAINER
]

DEFINES+=NO_VISUALIZATION
DEFINES+=QTOPIA
#DEFINES+=NO_THUMBNAIL

HEADERS=\
    mediaplayer.h\
    playercontrol.h\
    statewidget.h\
    elidedlabel.h\
    playerwidget.h\
    playerfullscreenwidget.h\
    mediabrowser.h\
    visualization.h\
    browser.h\
    keyhold.h\
    menumodel.h\
    menuview.h\
    requesthandler.h\
    servicerequest.h\
    keyfilter.h\
    rotate.h\
    playmediaservice.h

SOURCES=\
    main.cpp\
    mediaplayer.cpp\
    playercontrol.cpp\
    statewidget.cpp\
    elidedlabel.cpp\
    playerwidget.cpp\
    playerfullscreenwidget.cpp\
    mediabrowser.cpp\
    visualization.cpp\
    browser.cpp\
    keyhold.cpp\
    menumodel.cpp\
    menuview.cpp\
    requesthandler.cpp\
    keyfilter.cpp\
    rotate.cpp\
    playmediaservice.cpp

!contains(QTOPIAMEDIA_ENGINES,helix) {
    DEFINES+=NO_HELIX
}

# Install rules

target [
    hint=sxe
    domain=trusted
]

desktop [
    hint=desktop
    files=mediaplayer.desktop
    path=/apps/Applications
]

pics [
    hint=pics
    files=pics/*
    path=/pics/mediaplayer
]

help [
    hint=help
    source=help
    files=*.html
]

playmediaservice [
    hint=image
    files=services/PlayMedia/mediaplayer
    path=/services/PlayMedia
]

