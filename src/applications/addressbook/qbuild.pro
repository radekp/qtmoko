TEMPLATE=app
CONFIG+=qtopia
TARGET=addressbook

QTOPIA*=pim mail collective
enable_telephony:QTOPIA*=phone
CONFIG+=quicklaunch singleexec
equals(QTOPIA_UI,home):MODULES*=homeui

STRING_LANGUAGE=en_US
AVAILABLE_LANGUAGES=$$QTOPIA_AVAILABLE_LANGUAGES
LANGUAGES=$$QTOPIA_LANGUAGES
UNIFIED_NCT_LUPDATE=1
TRANSLATION_SUBDIR=translations

pkg [
    name=contacts
    desc="Contacts application for Qt Extended."
    version=$$QTOPIA_VERSION
    license=$$QTOPIA_LICENSE
    maintainer=$$QTOPIA_MAINTAINER
]

FORMS=\
    actiondialog.ui

HEADERS=\
    abeditor.h\
    contactdetails.h\
    contactdocument.h\
    contactbrowser.h\
    contactmessagehistorylist.h\
    contactoverview.h\
    contactlistpane.h\
    addressbook.h\
    groupview.h\
    fieldlist.h\
    emaildialogphone.h

SOURCES=\
    abeditor.cpp\
    contactdetails.cpp\
    addressbook.cpp\
    groupview.cpp\
    contactdocument.cpp\
    contactmessagehistorylist.cpp\
    contactbrowser.cpp\
    contactoverview.cpp\
    contactlistpane.cpp\
    emaildialogphone.cpp\
    fieldlist.cpp\
    main.cpp

TEL [
    TYPE=CONDITIONAL_SOURCES
    CONDITION=enable_telephony
    HEADERS=contactcallhistorylist.h
    SOURCES=contactcallhistorylist.cpp
]

HOME [
    TYPE=CONDITIONAL_SOURCES
    CONDITION=equals(QTOPIA_UI,home)
    HEADERS=deskphonedetails.h deskphonewidgets.h deskphoneeditor.h
    SOURCES=deskphonedetails.cpp deskphonewidgets.cpp deskphoneeditor.cpp
]

dynamic [
    TYPE=CONDITIONAL_SOURCES
    CONDITION=!enable_singleexec|!contains(PROJECTS,applications/todolist)
    HEADERS=\
        ../todolist/reminderpicker.h\
        ../todolist/qdelayedscrollarea.h
    SOURCES=\
        ../todolist/reminderpicker.cpp\
        ../todolist/qdelayedscrollarea.cpp
]

dynamic_cell [
    TYPE=CONDITIONAL_SOURCES
    CONDITION=if(!enable_singleexec|!contains(PROJECTS,settings/profileedit)):enable_telephony
    HEADERS=\
        ../../settings/profileedit/ringtoneeditor.h
    SOURCES=\
        ../../settings/profileedit/ringtoneeditor.cpp
]

# Install rules

target [
    hint=sxe
    domain=trusted
]

service [
    hint=image
    files=services/Contacts/addressbook
    path=/services/Contacts
]

receiveservice [
    hint=image
    files=services/Receive/text/x-vcard/addressbook
    path=/services/Receive/text/x-vcard
]

desktop [
    hint=desktop
    files=addressbook.desktop
    path=/apps/Applications
]

help [
    hint=help
    source=help
    files=*.html
]

# pics are installed by libqtopiapim since they're shared

im [
    hint=image
    files=named_addressbook-*.conf
    path=/etc/im/pkim
]

qdsservice [
    hint=image
    files=etc/qds/Contacts
    path=/etc/qds
]

enable_cell {
    phoneservice [
        hint=image
        files=services/ContactsPhone/addressbook
        path=/services/ContactsPhone
    ]

    qdsphoneservice [
        hint=image
        files=etc/qds/ContactsPhone
        path=/etc/qds
    ]
}

ribbonconf [
    hint=image optional
    files=etc/default/Trolltech/AlphabetRibbonLayout.conf
    path=/etc/default/Trolltech
]

for(l,LANGUAGES) {
    # Setup the rule in an easy-to-read way
    settings [
        hint=image optional
        files=etc/default/Trolltech/$$l/AlphabetRibbonLayout.conf
        path=/etc/default/Trolltech/$$l
    ]

    # Now move the rule to a unique name
    properties=hint files path
    for(p,properties) {
        eval(settings_$${l}.$${p}="$$"settings.$${p})
        eval(settings.$${p}=)
    }
}

