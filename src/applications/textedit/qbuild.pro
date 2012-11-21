TEMPLATE=app
CONFIG+=qtopia
TARGET=textedit

CONFIG+=quicklaunch singleexec

STRING_LANGUAGE=en_US
AVAILABLE_LANGUAGES=$$QTOPIA_AVAILABLE_LANGUAGES
LANGUAGES=$$QTOPIA_LANGUAGES
UNIFIED_NCT_LUPDATE=1
TRANSLATION_SUBDIR=translations

pkg [
    name=notes
    desc="Notes application for Qt Extended."
    version=$$QTOPIA_VERSION
    license=$$QTOPIA_LICENSE
    maintainer=$$QTOPIA_MAINTAINER
]

HEADERS=\
    textedit.h

SOURCES=\
    textedit.cpp\
    main.cpp

# Install rules

target [
    hint=sxe
    domain=trusted
]

help [
    hint=help
    source=help
    files=*.html
]

desktop [
    hint=desktop
    files=textedit.desktop
    path=/apps/Applications
]

pics [
    hint=pics
    files=pics/*
    path=/pics/textedit
]

openservice [
    hint=image
    files=services/Open/text/plain/textedit
    path=/services/Open/text/plain
]

viewservice [
    hint=image
    files=services/View/text/plain/textedit
    path=/services/View/text/plain
]

