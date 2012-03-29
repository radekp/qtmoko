TEMPLATE=plugin
TARGET=dbmigrate

PLUGIN_FOR=qtopia
PLUGIN_TYPE=qtopiasqlmigrate

CONFIG+=qtopia singleexec
MODULES*=sqlite

STRING_LANGUAGE=en_US
AVAILABLE_LANGUAGES=$$QTOPIA_AVAILABLE_LANGUAGES
LANGUAGES=$$QTOPIA_LANGUAGES
UNIFIED_NCT_LUPDATE=1
TRANSLATION_SUBDIR=translations

pkg [
    name=dbmigrate-plugin
    desc="Database migration plugin for Qt Extended."
    version=$$QTOPIA_VERSION
    license=$$QTOPIA_LICENSE
    maintainer=$$QTOPIA_MAINTAINER
]

RESOURCES=\
    qtopiapim/pimmigrate.qrc\
    qtopiaphone/phonemigrate.qrc

SOURCES=\
    migrateengine.cpp\
    qtopiapim/pimmigrate.cpp\
    qtopiaphone/phonemigrate.cpp

HEADERS=\
    migrateengine.h\
    qtopiapim/pimmigrate.h\
    qtopiaphone/phonemigrate.h

# If we're not building singleexec, we need to pull resources out of the Qtopia libs
qtopia [
    TYPE=CONDITIONAL_SOURCES
    CONDITION=!enable_singleexec
    RESOURCES=\
        /src/libraries/qtopia/qtopia.qrc\
        /src/libraries/qtopiapim/qtopiapim.qrc
]

