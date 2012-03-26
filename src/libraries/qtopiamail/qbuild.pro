TEMPLATE=lib
TARGET=qtopiamail
MODULE_NAME=qtopiamail
VERSION=4.0.0

CONFIG+=qtopia hide_symbols singleexec
QTOPIA*=pim
equals(QTOPIA_UI,home):MODULES*=homeui

STRING_LANGUAGE=en_US
AVAILABLE_LANGUAGES=$$QTOPIA_AVAILABLE_LANGUAGES
LANGUAGES=$$QTOPIA_LANGUAGES
TRANSLATION_SUBDIR=translations

pkg [
    name=libqtopiamail
    desc="Mail library for Qt Extended."
    version=$$QTOPIA_VERSION
    license=$$QTOPIA_LICENSE
    maintainer=$$QTOPIA_MAINTAINER
]

enable_cell:contains(PROJECTS,libraries/qtopiasmil):CONFIG+=enable_mms
else:DEFINES+=QTOPIA_NO_SMS QTOPIA_NO_MMS

enable_mms {
    qtopia_depot:DEFINES+=ENABLE_UNCONDITIONAL_MMS_SEND
}

RESOURCES=\
    qtopiamail.qrc

HEADERS=\
    qmailaddress.h\
    qmailcodec.h\
    qmaildatacomparator.h\
    qmailmessage.h\
    qmailtimestamp.h\
    qmailfolder.h\
    qmailfolderkey.h\
    qmailfoldersortkey.h\
    qmailstore.h\
    qmailmessagekey.h\
    qmailmessageserver.h\
    qmailmessagesortkey.h\
    qmailcomposer.h\
    qmailcomposerplugin.h\
    qmailviewer.h\
    qmailviewerplugin.h\
    qmailid.h\
    qmailaccount.h\
    qmailmessagelistmodel.h\
    qmailaccountlistmodel.h\
    qmailmessagedelegate.h\
    qmailaccountkey.h\
    qmailaccountsortkey.h\
    qmailmessageremovalrecord.h\
    qmailserviceaction.h\
    qmailnewmessagehandler.h\
    qmailmessageset.h\
    qprivateimplementation.h\
    qprivateimplementationdef.h

PRIVATE_HEADERS=\
    bind_p.h\
    qmailfolderkey_p.h\
    qmailfoldersortkey_p.h\
    qmailstore_p.h\
    qmailmessagekey_p.h\
    qmailmessagesortkey_p.h\
    qmailaccountkey_p.h\
    qmailaccountsortkey_p.h\
    qmailkeyargument_p.h\
    semaphore_p.h

SEMI_PRIVATE_HEADERS=\
    accountconfiguration_p.h\
    longstream_p.h\
    longstring_p.h \
    detailspage_p.h\
    addressselectorwidget_p.h

SOURCES=\
    accountconfiguration.cpp\
    qmailaddress.cpp\
    qmailcodec.cpp\
    qmaildatacomparator.cpp\
    qmailmessage.cpp\
    qmailtimestamp.cpp\
    longstring.cpp \
    longstream.cpp\
    qmailfolder.cpp\
    qmailfolderkey.cpp\
    qmailfoldersortkey.cpp\
    qmailmessageserver.cpp\
    qmailmessagesortkey.cpp\
    qmailstore.cpp\
    qmailstore_p.cpp\
    qmailmessagekey.cpp\
    qmailcomposer.cpp\
    qmailcomposerplugin.cpp\
    qmailviewer.cpp\
    qmailviewerplugin.cpp\
    qmailid.cpp\
    qprivateimplementation.cpp\
    qmailaccount.cpp\
    qmailmessagelistmodel.cpp\
    qmailaccountlistmodel.cpp\
    qmailmessagedelegate.cpp\
    qmailaccountkey.cpp\
    qmailaccountsortkey.cpp\
    qmailmessageremovalrecord.cpp\
    detailspage.cpp\
    semaphore.cpp\
    addressselectorwidget.cpp\
    qmailserviceaction.cpp\
    qmailnewmessagehandler.cpp\
    qmailmessageset.cpp

