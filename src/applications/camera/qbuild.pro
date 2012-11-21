TEMPLATE=app
CONFIG+=qtopia
TARGET=camera

QTOPIA*=video
CONFIG+=quicklaunch singleexec
enable_pictureflow:MODULES*=pictureflow

STRING_LANGUAGE=en_US
AVAILABLE_LANGUAGES=$$QTOPIA_AVAILABLE_LANGUAGES
LANGUAGES=$$QTOPIA_LANGUAGES
UNIFIED_NCT_LUPDATE=1
TRANSLATION_SUBDIR=translations

pkg [
    name=camera
    desc="Camera application for Qt Extended."
    version=$$QTOPIA_VERSION
    license=$$QTOPIA_LICENSE
    maintainer=$$QTOPIA_MAINTAINER
]

FORMS=\
    camerabase.ui\
    camerasettings.ui

HEADERS=\
    mainwindow.h \
    phototimer.h \
    minsecspinbox.h\
    noeditspinbox.h\
    videocaptureview.h\
    camerastateprocessor.h\
    cameraformatconverter.h\
    zoomslider.h\
    cameravideosurface.h

SOURCES=\
    mainwindow.cpp \
    main.cpp \
    phototimer.cpp \
    minsecspinbox.cpp\
    noeditspinbox.cpp\
    videocaptureview.cpp\
    camerastateprocessor.cpp\
    cameraformatconverter.cpp\
    zoomslider.cpp\
    cameravideosurface.cpp

pictureflow [
    TYPE=CONDITIONAL_SOURCES
    CONDITION=enable_pictureflow
    HEADERS=imagebrowser.h
    SOURCES=imagebrowser.cpp
]

# Install rules

target [
    hint=sxe
    domain=trusted
]

desktop [
    hint=desktop
    files=camera.desktop
    path=/apps/Applications
]

help [
    hint=help
    source=help
    files=camera*
]

pics [
    hint=pics
    files=pics/*
    path=/pics/camera
]

service [
    hint=image
    files=services/Camera/camera
    path=/services/Camera
]

qdsservice [
    hint=image
    files=etc/qds/Camera
    path=/etc/qds
]

