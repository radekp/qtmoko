TEMPLATE=lib
TARGET=accelerometer
MODULE_NAME=accelerometer
VERSION=4.0.0

CONFIG+=qtopia hide_symbols singleexec

pkg [
    name=libaccelerometer
    desc="Library for controlling and using an accelerometer."
    version=$$QTOPIA_VERSION
    license=$$QTOPIA_LICENSE
    maintainer=$$QTOPIA_MAINTAINER
]

HEADERS=\
    accelerometers.h\
    rotatehelper.h

SOURCES=\
    accelerometers.cpp\
    rotatehelper.cpp
