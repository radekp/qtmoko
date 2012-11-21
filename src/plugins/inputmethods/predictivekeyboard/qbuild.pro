TEMPLATE=plugin
TARGET=qpredictivekeyboard

PLUGIN_FOR=qtopia
PLUGIN_TYPE=inputmethods

CONFIG+=qtopia singleexec
QTOPIA*=theming

STRING_LANGUAGE=en_US
AVAILABLE_LANGUAGES=$$QTOPIA_AVAILABLE_LANGUAGES
LANGUAGES=$$QTOPIA_LANGUAGES
TRANSLATION_SUBDIR=translations

pkg [
    name=predictivekeyboard-inputmethod
    desc="Predictive keyboard inputmethod plugin for Qt Extended."
    version=$$QTOPIA_VERSION
    license=$$QTOPIA_LICENSE
    maintainer=$$QTOPIA_MAINTAINER
]

HEADERS=\
    predictivekeyboard.h\
    predictivekeyboardimpl.h\
    keyboard.h

SOURCES=\
    predictivekeyboard.cpp\
    predictivekeyboardimpl.cpp\
    keyboard.cpp

# Install rules

pics [
    hint=pics
    files=pics/*
    path=/pics/predictivekeyboard
]

help [
    hint=help
    source=help
    files=*.html
]

settings [
    hint=image
    files=\
        etc/default/Trolltech/PredictiveKeyboardLayout.conf
    path=/etc/default/Trolltech
]

settings2 [
    hint=image optional
    files=\
        etc/default/Trolltech/PredictiveKeyboard.conf
    path=/etc/default/Trolltech
]

for(l,QTOPIA_LANGUAGES) {
    # Setup the rule in an easy-to-read way
    tmp [
        hint=image optional
        files=etc/default/Trolltech/$$l/PredictiveKeyboardLayout.conf
        path=/etc/default/Trolltech/$$l
    ]

    # Now move the rule to a unique name
    properties=hint files path
    for(p,properties) {
        eval(settings_$${l}.$${p}="$$"tmp.$${p})
        eval(tmp.$${p}=)
    }
}

