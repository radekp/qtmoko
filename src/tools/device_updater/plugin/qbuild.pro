TEMPLATE=plugin
TARGET=device_updater

PLUGIN_FOR=qtopia
PLUGIN_TYPE=qdsync

CONFIG+=qtopia singleexec
QTOPIA=base
MODULES*=qdsync_common

STRING_LANGUAGE=en_US
AVAILABLE_LANGUAGES=$$QTOPIA_AVAILABLE_LANGUAGES
LANGUAGES=$$QTOPIA_LANGUAGES
TRANSLATION_SUBDIR=translations

pkg [
    name=device_updater-plugin
    desc="Device updater plugin for Qt Extended."
    version=$$QTOPIA_VERSION
    license=$$QTOPIA_LICENSE
    maintainer=$$QTOPIA_MAINTAINER
]

SOURCES=\
    main.cpp

