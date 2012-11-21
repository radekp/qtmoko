TEMPLATE=app
TARGET=messageserver

CONFIG+=qtopia singleexec
QTOPIA*=mail

enable_cell:contains(PROJECTS,libraries/qtopiasmil):CONFIG+=enable_mms
else:DEFINES+=QTOPIA_NO_SMS QTOPIA_NO_MMS

enable_telephony {
    QTOPIA*=collective phone
} else {
    DEFINES+=QTOPIA_NO_COLLECTIVE
}

STRING_LANGUAGE=en_US
AVAILABLE_LANGUAGES=$$QTOPIA_AVAILABLE_LANGUAGES
LANGUAGES=$$QTOPIA_LANGUAGES
UNIFIED_NCT_LUPDATE=1
TRANSLATION_SUBDIR=translations

pkg [
    name=messageserver
    desc="Message server for Qt Extended."
    version=$$QTOPIA_VERSION
    license=$$QTOPIA_LICENSE
    maintainer=$$QTOPIA_MAINTAINER
]

HEADERS=\
    client.h\
    emailhandler.h\
    imapclient.h\
    imapprotocol.h\
    mailmessageclient.h\
    mailtransport.h\
    messagearrivalservice.h\
    messageclassifier.h\
    messageserver.h\
    popclient.h\
    smsclient.h\
    smsdecoder.h\
    smtpclient.h\
    systemclient.h

SOURCES=\
    client.cpp\
    emailhandler.cpp\
    imapclient.cpp\
    imapprotocol.cpp\
    mailmessageclient.cpp\
    mailtransport.cpp\
    main.cpp\
    messagearrivalservice.cpp\
    messageclassifier.cpp\
    messageserver.cpp\
    popclient.cpp\
    smsclient.cpp\
    smsdecoder.cpp\
    smtpclient.cpp\
    systemclient.cpp

MMS [
    TYPE=CONDITIONAL_SOURCES
    CONDITION=enable_mms
    SOURCES=\
        mmsclient.cpp\
        mmscomms.cpp\
        mmsmessage.cpp
    HEADERS=\
        mmsclient.h\
        mmscomms.h\
        mmsmessage.h
]

MMSCOMMS [
    TYPE=CONDITIONAL_SOURCES
    CONDITION=enable_mms
    HEADERS=mmscomms_http.h
    SOURCES=mmscomms_http.cpp
]
enable_mms:DEFINES+=MMSCOMMS_HTTP

COLLECTIVE [
    TYPE=CONDITIONAL_SOURCES
    CONDITION=enable_telephony
    HEADERS=collectiveclient.h
    SOURCES=collectiveclient.cpp
]

# Install rules

target [
    hint=sxe
    domain=trusted
]

daemon [
    hint=image
    files=etc/daemons/messageserver.conf
    path=/etc/daemons
]

qdsnewsystemmessagearrivalservice [
    hint=image
    files=etc/qds/NewSystemMessageArrival
    path=/etc/qds
]

qdsnewemailarrivalservice [
    hint=image
    files=etc/qds/NewEmailArrival
    path=/etc/qds
]

enable_cell {
    messagearrivalservice [
        hint=image
        files=services/MessageArrival/messageserver
        path=/services/MessageArrival
    ]

    qdsmessagearrivalservice [
        hint=image
        files=etc/qds/MessageArrival
        path=/etc/qds
    ]

    qdsnewsmsarrivalservice [
        hint=image
        files=etc/qds/NewSmsArrival
        path=/etc/qds
    ]

    qdsnewmmsarrivalservice [
        hint=image
        files=etc/qds/NewMmsArrival
        path=/etc/qds
    ]
}

enable_telephony {
    qdsnewinstantmessagearrivalservice [
        hint=image
        files=etc/qds/NewInstantMessageArrival
        path=/etc/qds
    ]
}

