TEMPLATE=app
CONFIG+=qtopia
TARGET=clock

CONFIG+=quicklaunch singleexec

STRING_LANGUAGE=en_US
AVAILABLE_LANGUAGES=$$QTOPIA_AVAILABLE_LANGUAGES
LANGUAGES=$$QTOPIA_LANGUAGES
UNIFIED_NCT_LUPDATE=1
TRANSLATION_SUBDIR=translations

pkg [
    name=clock
    desc="Clock application for Qt Extended."
    version=$$QTOPIA_VERSION
    license=$$QTOPIA_LICENSE
    maintainer=$$QTOPIA_MAINTAINER
]

FORMS=\
    clockbase.ui\
    stopwatchbase.ui\
    alarmbase.ui

HEADERS=\
    clockmain.h\
    clock.h\
    stopwatch.h\
    alarm.h\
    alarmdaysedit.h\
    ringcontrol.h

SOURCES=\
    clockmain.cpp\
    clock.cpp\
    stopwatch.cpp\
    alarm.cpp\
    alarmdaysedit.cpp\
    ringcontrol.cpp\
    main.cpp

# Install rules

target [
    hint=sxe
    domain=trusted
]

desktop [
    hint=desktop
    files=clock.desktop
    path=/apps/Applications
]

servicea [
    hint=image
    files=services/Alarm/clock
    path=/services/Alarm
]

serviceb [
    hint=image
    files=services/Clock/clock
    path=/services/Clock
]

timeservice [
    hint=image
    files=services/TimeMonitor/clock
    path=/services/TimeMonitor
]

sound [
    hint=image
    files=sounds/*
    path=/sounds
]

help [
    hint=help
    source=help
    files=*.html
]

pics [
    hint=pics
    files=pics/*
    path=/pics/clock
]

