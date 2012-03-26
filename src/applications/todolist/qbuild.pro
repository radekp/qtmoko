TEMPLATE=app
CONFIG+=qtopia
TARGET=todolist

QTOPIA*=pim
CONFIG+=quicklaunch singleexec

STRING_LANGUAGE=en_US
AVAILABLE_LANGUAGES=$$QTOPIA_AVAILABLE_LANGUAGES
LANGUAGES=$$QTOPIA_LANGUAGES
UNIFIED_NCT_LUPDATE=1
TRANSLATION_SUBDIR=translations

pkg [
    name=tasks
    desc="Tasks application for Qt Extended."
    version=$$QTOPIA_VERSION
    license=$$QTOPIA_LICENSE
    maintainer=$$QTOPIA_MAINTAINER
]

HEADERS=\
    todotable.h\
    todoentryimpl.h\
    todocategoryselector.h\
    mainwindow.h\
    recurrencedetails.h\
    reminderpicker.h\
    listpositionbar.h\
    qdelayedscrollarea.h\
    qtopiatabwidget.h

SOURCES=\
    todotable.cpp\
    todoentryimpl.cpp\
    mainwindow.cpp\
    main.cpp\
    recurrencedetails.cpp\
    reminderpicker.cpp\
    listpositionbar.cpp\
    qdelayedscrollarea.cpp\
    qtopiatabwidget.cpp

# Install rules

target [
    hint=sxe
    domain=trusted
]

service [
    hint=image
    files=services/Tasks/todolist
    path=/services/Tasks
]

qdsservice [
    hint=image
    files=etc/qds/Tasks
    path=/etc/qds
]

recservice [
    hint=image
    files=services/Receive/text/x-vcalendar-Tasks/todolist
    path=/services/Receive/text/x-vcalendar-Tasks
]

desktop [
    hint=desktop
    files=todolist.desktop
    path=/apps/Applications
]

help [
    hint=help
    source=help
    files=*.html
]

# pics are installed by libqtopiapim since they're shared

