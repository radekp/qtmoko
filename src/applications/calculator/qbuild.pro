TEMPLATE=app
CONFIG+=qtopia
TARGET=calculator

CONFIG+=quicklaunch singleexec

STRING_LANGUAGE=en_US
AVAILABLE_LANGUAGES=$$QTOPIA_AVAILABLE_LANGUAGES
LANGUAGES=$$QTOPIA_LANGUAGES
UNIFIED_NCT_LUPDATE=1
TRANSLATION_SUBDIR=translations

pkg [
    name=calculator
    desc="Calculator application for Qt Extended."
    multi=libraries/qtopiacalc
    version=$$QTOPIA_VERSION
    license=$$QTOPIA_LICENSE
    maintainer=$$QTOPIA_MAINTAINER
]

# library files
HEADERS=\
    calculator.h\
    engine.h\
    data.h\
    instruction.h\
    display.h\
    interfaces/stdinputwidgets.h\
    integerdata.h\
    integerinstruction.h

SOURCES=\
    calculator.cpp\
    engine.cpp\
    data.cpp\
    instruction.cpp\
    display.cpp\
    interfaces/stdinputwidgets.cpp\
    main.cpp

# double type
HEADERS+=doubledata.h
SOURCES+=doubledata.cpp

# mobile UI
HEADERS+=\
    interfaces/phone.h\
    phoneinstruction.h\
    doubleinstruction.h\
    interfaces/simple.h

SOURCES+=\
    interfaces/phone.cpp\
    phoneinstruction.cpp\
    doubleinstruction.cpp\
    interfaces/simple.cpp

FORMS+=helperpanel.ui

# Install rules

target [
    hint=sxe
    domain=trusted
]

desktop [
    hint=desktop
    files=calculator.desktop
    path=/apps/Applications
]

help [
    hint=help
    source=help
    files=calculator*
]

pics [
    hint=pics
    files=pics/*
    path=/pics/calculator
]

