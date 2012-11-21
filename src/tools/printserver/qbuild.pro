TEMPLATE=app
TARGET=printserver

CONFIG+=qtopia singleexec
QTOPIA*=printing

STRING_LANGUAGE=en_US
AVAILABLE_LANGUAGES=$$QTOPIA_AVAILABLE_LANGUAGES
LANGUAGES=$$QTOPIA_LANGUAGES
UNIFIED_NCT_LUPDATE=1
TRANSLATION_SUBDIR=translations

pkg [
    name=printserver
    desc="Print server for Qt Extended."
    version=$$QTOPIA_VERSION
    license=$$QTOPIA_LICENSE
    maintainer=$$QTOPIA_MAINTAINER
]

HEADERS=\
    printserver.h

SOURCES=\
    main.cpp\
    printserver.cpp

# Install rules

target [
    hint=sxe
    domain=trusted
]

service [
    files=services/Print/printserver
    path=/services/Print
]

help [
    hint=help
    source=help
    files=*.html
]

