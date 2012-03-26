TEMPLATE=lib
TARGET=qtopiapim
MODULE_NAME=qtopiapim
VERSION=4.0.0

CONFIG+=qtopia hide_symbols singleexec
QTOPIA*=comm collective
enable_cell:QTOPIA*=phone
MODULES*=sqlite vobject

STRING_LANGUAGE=en_US
AVAILABLE_LANGUAGES=$$QTOPIA_AVAILABLE_LANGUAGES
LANGUAGES=$$QTOPIA_LANGUAGES
UNIFIED_NCT_LUPDATE=1
TRANSLATION_SUBDIR=translations

pkg [
    name=libqtopiapim
    desc="PIM library for Qt Extended."
    version=$$QTOPIA_VERSION
    license=$$QTOPIA_LICENSE
    maintainer=$$QTOPIA_MAINTAINER
]

RESOURCES=\
    qtopiapim.qrc

HEADERS=\
    qpimrecord.h\
    qtask.h\
    qappointment.h\
    qcontact.h\
    qpimmodel.h\
    qtaskmodel.h\
    qappointmentmodel.h\
    qcontactmodel.h\
    qappointmentview.h\
    qcontactview.h\
    qtaskview.h\
    qpimsource.h\
    qpimsourcemodel.h\
    qpimsourcedialog.h\
    qpimdelegate.h\
    qphonenumber.h\
    qfielddefinition.h

SEMI_PRIVATE_HEADERS=\
    qgooglecontext_p.h\
    qpimsqlio_p.h

PRIVATE_HEADERS=\
    qannotator_p.h\
    qsqlpimtablemodel_p.h\
    qappointmentsqlio_p.h\
    qpreparedquery_p.h\
    qtasksqlio_p.h\
    qcontactsqlio_p.h\
    qdependentcontexts_p.h\
    qpimdependencylist_p.h

SOURCES=\
    qannotator.cpp\
    qsqlpimtablemodel.cpp\
    qpreparedquery.cpp\
    qpimrecord.cpp\
    qtask.cpp\
    qappointment.cpp\
    qappointmentsqlio.cpp\
    qcontact.cpp\
    qpimmodel.cpp\
    qtaskmodel.cpp\
    qcontactmodel.cpp\
    qappointmentmodel.cpp\
    qappointmentview.cpp\
    qcontactview.cpp\
    qtaskview.cpp\
    qpimsource.cpp\
    qpimsqlio.cpp\
    qtasksqlio.cpp\
    qcontactsqlio.cpp\
    qpimsourcemodel.cpp\
    qpimsourcedialog.cpp\
    qphonenumber.cpp\
    qpimdelegate.cpp\
    qdependentcontexts.cpp\
    qfielddefinition.cpp\
    qpimdependencylist.cpp\
    qgooglecontext.cpp

CELL [
    TYPE=CONDITIONAL_SOURCES
    CONDITION=enable_cell
    SEMI_PRIVATE_HEADERS=qsimcontext_p.h qsimsync_p.h
    SOURCES=qsimcontext.cpp qsimsync.cpp
]

# Install rules

pkg_qtopiapim_settings [
    hint=image
    files=etc/default/Trolltech/Contacts.conf
    path=/etc/default/Trolltech
]

apics [
    hint=pics
    files=pics/addressbook/*
    path=/pics/addressbook
]

dpics [
    hint=pics
    files=pics/datebook/*
    path=/pics/datebook
]

tpcs [
    hint=pics
    files=pics/todolist/*
    path=/pics/todolist
]

