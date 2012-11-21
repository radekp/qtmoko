TEMPLATE=app
CONFIG+=qtopia
TARGET=mediarecorder

QTOPIA*=audio
CONFIG+=quicklaunch singleexec

STRING_LANGUAGE=en_US
AVAILABLE_LANGUAGES=$$QTOPIA_AVAILABLE_LANGUAGES
LANGUAGES=$$QTOPIA_LANGUAGES
UNIFIED_NCT_LUPDATE=1
TRANSLATION_SUBDIR=translations

pkg [
    name=voicenotes
    desc="Voice notes application for Qt Extended."
    version=$$QTOPIA_VERSION
    license=$$QTOPIA_LICENSE
    maintainer=$$QTOPIA_MAINTAINER
]

FORMS=\
    mediarecorderbase.ui

HEADERS=\
    mediarecorder.h\
    pluginlist.h\
    samplebuffer.h\
    timeprogressbar.h\
    confrecorder.h\
    waveform.h\
    audioparameters_p.h

SOURCES=\
    mediarecorder.cpp\
    pluginlist.cpp\
    samplebuffer.cpp\
    timeprogressbar.cpp\
    confrecorder.cpp\
    waveform.cpp\
    main.cpp\
    audioparameters.cpp

# Install rules

target [
    hint=sxe
    domain=trusted
]

desktop [
    hint=desktop
    files=mediarecorder.desktop
    path=/apps/Applications
]

help [
    hint=help
    source=help
    files=*.html
]

pics [
    hint=pics
    files=pics/*
    path=/pics/mediarecorder
]

voicerecorderservice [
    hint=image
    files=services/VoiceRecording/mediarecorder
    path=/services/VoiceRecording
]

qdsservice [
    hint=image
    files=etc/qds/VoiceRecording
    path=/etc/qds
]

