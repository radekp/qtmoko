TEMPLATE=plugin
TARGET=qdockedkeyboard

PLUGIN_FOR=qtopia
PLUGIN_TYPE=inputmethods

CONFIG+=qtopia singleexec

STRING_LANGUAGE=en_US
AVAILABLE_LANGUAGES=$$QTOPIA_AVAILABLE_LANGUAGES
LANGUAGES=$$QTOPIA_LANGUAGES
TRANSLATION_SUBDIR=translations

# packaged with the keyboard input method

HEADERS=\
    dockedkeyboard.h\
    dockedkeyboardimpl.h

SOURCES=\
    dockedkeyboard.cpp\
    dockedkeyboardimpl.cpp

!enable_singleexec|!contains(PROJECTS,plugins/inputmethods/keyboard):SOURCEPATH+=../keyboard
KEYBOARD [
    TYPE=CONDITIONAL_SOURCES
    CONDITION=!enable_singleexec|!contains(PROJECTS,plugins/inputmethods/keyboard)
    HEADERS=\
        keyboard.h\
        pickboardcfg.h\
        pickboardpicks.h\
        keyboardframe.h
    SOURCES=\
        keyboard.cpp\
        pickboardcfg.cpp\
        pickboardpicks.cpp\
        keyboardframe.cpp
]

