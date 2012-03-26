TEMPLATE=app
CONFIG+=qtopia
TARGET=qtmail

QTOPIA*=mail pim
enable_telephony:QTOPIA*=phone
CONFIG+=quicklaunch singleexec
equals(QTOPIA_UI,home):MODULES*=homeui

STRING_LANGUAGE=en_US
AVAILABLE_LANGUAGES=$$QTOPIA_AVAILABLE_LANGUAGES
LANGUAGES=$$QTOPIA_LANGUAGES
UNIFIED_NCT_LUPDATE=1
TRANSLATION_SUBDIR=translations

pkg [
    name=messages
    desc="Messages application for Qt Extended."
    version=$$QTOPIA_VERSION
    license=$$QTOPIA_LICENSE
    maintainer=$$QTOPIA_MAINTAINER
]

enable_cell:contains(PROJECTS,libraries/qtopiasmil):CONFIG+=enable_mms
else:DEFINES+=QTOPIA_NO_SMS QTOPIA_NO_MMS
!enable_telephony:DEFINES+=QTOPIA_NO_COLLECTIVE

FORMS=\
    editaccountbasephone.ui

HEADERS=\
    accountsettings.h\
    editaccount.h\
    emailclient.h\
    emailservice.h\
    emailpropertysetter.h\
    icontype.h\
    maillist.h\
    messagefolder.h\
    messagelistview.h\
    messagesservice.h\
    messagestore.h\
    qtmailwindow.h\
    readmail.h\
    selectfolder.h\
    smsservice.h\
    statusdisplay.h\
    viewatt.h\
    writemail.h\
    selectcomposerwidget.h\
    folderdelegate.h\
    foldermodel.h\
    folderview.h\
    actionfoldermodel.h\
    actionfolderview.h\
    emailfoldermodel.h\
    emailfolderview.h

SOURCES=\
    accountsettings.cpp\
    editaccount.cpp\
    emailclient.cpp\
    emailservice.cpp\
    emailpropertysetter.cpp\
    maillist.cpp\
    main.cpp\
    messagefolder.cpp\
    messagelistview.cpp\
    messagesservice.cpp\
    messagestore.cpp\
    qtmailwindow.cpp\
    readmail.cpp\
    selectfolder.cpp\
    smsservice.cpp\
    statusdisplay.cpp\
    viewatt.cpp\
    writemail.cpp\
    selectcomposerwidget.cpp\
    folderdelegate.cpp\
    foldermodel.cpp\
    folderview.cpp\
    actionfoldermodel.cpp\
    actionfolderview.cpp\
    emailfoldermodel.cpp\
    emailfolderview.cpp

MMS [
    TYPE=CONDITIONAL_SOURCES
    CONDITION=enable_mms
    FORMS=mmseditaccountbase.ui
    HEADERS=mmseditaccount.h
    SOURCES=mmseditaccount.cpp
]

COLLECTIVE [
    TYPE=CONDITIONAL_SOURCES
    CONDITION=enable_telephony
    HEADERS=instantmessageservice.h
    SOURCES=instantmessageservice.cpp
]

NONHOME [
    TYPE=CONDITIONAL_SOURCES
    CONDITION=!equals(QTOPIA_UI,home)
    FORMS=searchviewbasephone.ui
    HEADERS=searchview.h
    SOURCES=searchview.cpp
]

# Install rules

target [
    hint=sxe
    domain=trusted
]

pics [
    hint=pics
    files=pics/*
    path=/pics/qtmail
]

desktop [
    hint=desktop
    files=qtmail.desktop
    path=/apps/Applications
]

emailservice [
    hint=image
    files=services/Email/qtmail
    path=/services/Email
]

qdsemailservice [
    hint=image
    files=etc/qds/Email
    path=/etc/qds
]

help [
    hint=help
    source=help
    files=*.html
]

messageservice [
    hint=image
    files=services/Messages/qtmail
    path=/services/Messages
]

newsystemmessagearrivalservice [
    hint=image
    files=services/NewSystemMessageArrival/qtmail
    path=/services/NewSystemMessageArrival
]

newemailarrivalservice [
    hint=image
    files=services/NewEmailArrival/qtmail
    path=/services/NewEmailArrival
]

enable_cell {
    smsservice [
        hint=image
        files=services/SMS/qtmail
        path=/services/SMS
    ]

    qdssmsservice [
        hint=image
        files=etc/qds/SMS
        path=/etc/qds
    ]

    newsmsarrivalservice [
        hint=image
        files=services/NewSmsArrival/qtmail
        path=/services/NewSmsArrival
    ]

    newmmsarrivalservice [
        hint=image
        files=services/NewMmsArrival/qtmail
        path=/services/NewMmsArrival
    ]
}

enable_telephony {
    instantmessageservice [
        hint=image
        files=services/InstantMessage/qtmail
        path=/services/InstantMessage
    ]

    newinstantmessagearrivalservice [
        hint=image
        files=services/NewInstantMessageArrival/qtmail
        path=/services/NewInstantMessageArrival
    ]
}

