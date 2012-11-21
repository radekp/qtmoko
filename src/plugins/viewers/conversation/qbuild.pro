TEMPLATE=plugin
TARGET=conversationviewer

PLUGIN_FOR=qtopia
PLUGIN_TYPE=viewers

CONFIG+=qtopia singleexec
QTOPIA*=mail
equals(QTOPIA_UI,home):MODULES*=homeui

STRING_LANGUAGE=en_US
AVAILABLE_LANGUAGES=$$QTOPIA_AVAILABLE_LANGUAGES
LANGUAGES=$$QTOPIA_LANGUAGES
TRANSLATION_SUBDIR=translations

pkg [
    name=conversation-viewer
    desc="Conversation viewer plugin for Qt Extended."
    version=$$QTOPIA_VERSION
    license=$$QTOPIA_LICENSE
    maintainer=$$QTOPIA_MAINTAINER
]

HEADERS=\
    conversationviewer.h\
    conversationdelegate.h

SOURCES=\
    conversationviewer.cpp\
    conversationdelegate.cpp

