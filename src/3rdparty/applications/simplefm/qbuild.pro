#qtopia_project(qtopia app)
TEMPLATE=app
TARGET=simplefm
CONFIG+=qtopia no_quicklaunch no_singleexec
DEFINES+=QTOPIA_PHONE

# I18n info
STRING_LANGUAGE=en_US
AVAILABLE_LANGUAGES=$$QTOPIA_AVAILABLE_LANGUAGES
LANGUAGES=$$QTOPIA_LANGUAGES
TRANSLATION_SUBDIR=translations

HEADERS = simplefm.h \
		qurlview.h \
		qabstracturlmodel.h \
		qlocalurlmodel.h \
		qsfinputdialog.h
SOURCES = main.cpp \
		simplefm.cpp \
		qurlview.cpp \
		qabstracturlmodel.cpp \
		qlocalurlmodel.cpp \
		qsfinputdialog.cpp

desktop [
files=simplefm.desktop
path=/apps/Applications
hint=desktop nct
trtarget=simplefm-nct
]
#INSTALLS+=desktop
pics [
files=pics/*
path=/pics/simplefm/
hint=pics
]
#INSTALLS+=pics
pkg [
name=qpe-simplefm
desc=File Manager for Qtopia
domain=trusted
]
