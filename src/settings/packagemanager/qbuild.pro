TEMPLATE=app
TARGET=packagemanager

CONFIG+=qtopia singleexec quicklaunch
enable_sxe:QTOPIA*=security
MODULES*=tar md5
LIBS += -lz

STRING_LANGUAGE=en_US
AVAILABLE_LANGUAGES=$$QTOPIA_AVAILABLE_LANGUAGES
LANGUAGES=$$QTOPIA_LANGUAGES
UNIFIED_NCT_LUPDATE=1

pkg [
    name=packagemanager
    desc="Package installer for Qt Extended."
    version=$$QTOPIA_VERSION
    license=$$QTOPIA_LICENSE
    maintainer=$$QTOPIA_MAINTAINER
]

# Give us a direct connection to the document system
DEFINES+=QTOPIA_DIRECT_DOCUMENT_SYSTEM_CONNECTION

FORMS=\
    serveredit.ui\
    packagedetails.ui

HEADERS=\
    packageview.h\
    packagemodel.h\
    packagecontroller.h\
    packageinformationreader.h\
    installcontrol.h\
    serveredit.h\
    httpfetcher.h\
    installedpackagescanner.h\
    targz.h\
    sandboxinstall.h\
    md5file.h\
    packagemanagerservice.h\
    packageversion.h\
    utils.h

SOURCES=\
    main.cpp\
    packageview.cpp\
    packagemodel.cpp\
    packagecontroller.cpp\
    packageinformationreader.cpp\
    installcontrol.cpp\
    serveredit.cpp\
    httpfetcher.cpp\
    installedpackagescanner.cpp\
    targz.cpp\
    sandboxinstall.cpp\
    md5file.cpp\
    packagemanagerservice.cpp\
    packageversion.cpp\
    utils.cpp

sxe [
    TYPE=CONDITIONAL_SOURCES
    CONDITION=enable_sxe
    HEADERS=domaininfo.h
    SOURCES=domaininfo.cpp
]

# Install rules

target [
    hint=sxe
    domain=trusted
]

help [
    hint=help
    source=help
    files=*.html
]

desktop [
    hint=desktop
    files=packagemanager.desktop
    path=/apps/Settings
]

pics [
    hint=pics
    files=pics/*
    path=/pics/packagemanager
]

secsettings [
    hint=image
    files=etc/default/Trolltech/PackageManager.conf
    path=/etc/default/Trolltech
]

packagemanagerservice [
    hint=image
    files=services/PackageManager/packagemanager
    path=/services/PackageManager
]

qdspackagemanagerservice [
    hint=image
    files=etc/qds/PackageManager
    path=/etc/qds
]

packages_category [
    hint=desktop prep_db
    files=packages.directory
    path=/apps/Packages
]

