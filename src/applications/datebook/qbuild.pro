TEMPLATE=app
CONFIG+=qtopia
TARGET=datebook

QTOPIA*=pim
CONFIG+=quicklaunch singleexec

STRING_LANGUAGE=en_US
AVAILABLE_LANGUAGES=$$QTOPIA_AVAILABLE_LANGUAGES
LANGUAGES=$$QTOPIA_LANGUAGES
UNIFIED_NCT_LUPDATE=1
TRANSLATION_SUBDIR=translations

pkg [
    name=calendar
    desc="Calendar application for Qt Extended."
    version=$$QTOPIA_VERSION
    license=$$QTOPIA_LICENSE
    maintainer=$$QTOPIA_MAINTAINER
]

FORMS=\
    findwidgetbase_p.ui\
    exceptiondialogbase.ui

HEADERS=\
    dayview.h\
    datebook.h\
    entrydialog.h\
    dayviewheaderimpl.h\
    datebooksettings.h\
    datebookcategoryselector.h\
    monthview.h\
    timedview.h\
    finddialog.h\
    findwidget_p.h\
    appointmentpicker.h\
    exceptiondialog.h\
    alarmview.h\
    appointmentdetails.h\
    appointmentlist.h\
    googleaccount.h\
    accounteditor.h

SOURCES=\
    dayview.cpp\
    datebook.cpp\
    entrydialog.cpp\
    dayviewheaderimpl.cpp\
    datebooksettings.cpp\
    monthview.cpp\
    timedview.cpp\
    finddialog.cpp\
    findwidget_p.cpp\
    appointmentpicker.cpp\
    exceptiondialog.cpp\
    alarmview.cpp\
    appointmentdetails.cpp\
    appointmentlist.cpp\
    main.cpp\
    googleaccount.cpp\
    accounteditor.cpp

dynamic [
    TYPE=CONDITIONAL_SOURCES
    CONDITION=!enable_singleexec|!contains(PROJECTS,applications/todolist)
    SOURCES=\
        ../todolist/reminderpicker.cpp \
        ../todolist/recurrencedetails.cpp\
        ../todolist/qdelayedscrollarea.cpp\
        ../todolist/qtopiatabwidget.cpp
    HEADERS=\
        ../todolist/reminderpicker.h \
        ../todolist/recurrencedetails.h\
        ../todolist/qdelayedscrollarea.h\
        ../todolist/qtopiatabwidget.h
]

# Install rules

target [
    hint=sxe
    domain=trusted
]

desktop [
    hint=desktop
    files=datebook.desktop
    path=/apps/Applications
]

calservice [
    hint=image
    files=services/Calendar/datebook
    path=/services/Calendar
]

timeservice [
    hint=image
    files=services/TimeMonitor/datebook
    path=/services/TimeMonitor
]

recservice [
    hint=image
    files=services/Receive/text/x-vcalendar-Events/datebook
    path=/services/Receive/text/x-vcalendar-Events
]

qdsservice [
    hint=image
    files=etc/qds/Calendar
    path=/etc/qds
]

help [
    hint=help
    source=help
    files=*.html
]

# pics are installed by libqtopiapim since they're shared

