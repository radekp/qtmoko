TEMPLATE=lib
TARGET=qtopiatheming
MODULE_NAME=qtopiatheming
VERSION=4.0.0

CONFIG+=qtopia hide_symbols singleexec no_tr

pkg [
    name=libqtopiatheming
    desc="Themeing library for Qt Extended."
    version=$$QTOPIA_VERSION
    license=$$QTOPIA_LICENSE
    maintainer=$$QTOPIA_MAINTAINER
]

HEADERS=\
    qthemedview.h\
    qthemedscene.h\
    qthemeitem.h\
    qthemerectitem.h\
    qthemeexclusiveitem.h\
    qthemeimageitem.h\
    qthemelayoutitem.h\
    qthemepageitem.h\
    qthemetextitem.h\
    qthemestatusitem.h\
    qthemelevelitem.h\
    qthemewidgetitem.h\
    qthemetemplateitem.h\
    qthemelistitem.h\
    qthemegroupitem.h\
    qthemepluginitem.h\
    qthemeitemattribute.h\
    qthemeitemfactory.h

PRIVATE_HEADERS=\
    qthemeitem_p.h\
    qthemelayoutitem_p.h\
    qthemerectitem_p.h\
    qthemetextitem_p.h\
    qthemelevelitem_p.h\
    qthemestatusitem_p.h 

SOURCES=\
    qthemedview.cpp\
    qthemedscene.cpp\
    qthemeitem.cpp\
    qthemerectitem.cpp\
    qthemeexclusiveitem.cpp\
    qthemeimageitem.cpp\
    qthemelayoutitem.cpp\
    qthemepageitem.cpp\
    qthemetextitem.cpp\
    qthemestatusitem.cpp\
    qthemelevelitem.cpp\
    qthemewidgetitem.cpp\
    qthemetemplateitem.cpp\
    qthemelistitem.cpp\
    qthemegroupitem.cpp\
    qthemepluginitem.cpp\
    qthemeitemattribute.cpp\
    qthemeitemfactory.cpp

