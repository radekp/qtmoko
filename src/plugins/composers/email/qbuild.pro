TEMPLATE=plugin
TARGET=emailcomposer

PLUGIN_FOR=qtopia
PLUGIN_TYPE=composers

CONFIG+=qtopia singleexec
QTOPIA*=mail

STRING_LANGUAGE=en_US
AVAILABLE_LANGUAGES=$$QTOPIA_AVAILABLE_LANGUAGES
LANGUAGES=$$QTOPIA_LANGUAGES
TRANSLATION_SUBDIR=translations

pkg [
    name=email-composer
    desc="Email composer plugin for Qt Extended."
    version=$$QTOPIA_VERSION
    license=$$QTOPIA_LICENSE
    maintainer=$$QTOPIA_MAINTAINER
]

HEADERS=\
    addatt.h\
    addattdialogphone.h

SOURCES=\
    addatt.cpp

HOME [
    TYPE=CONDITIONAL_SOURCES
    CONDITION=equals(QTOPIA_UI,home)
    HEADERS=deskphone_emailcomposer.h
    SOURCES=deskphone_emailcomposer.cpp
]

NONHOME [
    TYPE=CONDITIONAL_SOURCES
    CONDITION=!equals(QTOPIA_UI,home)
    HEADERS=emailcomposer.h
    SOURCES=emailcomposer.cpp
]

