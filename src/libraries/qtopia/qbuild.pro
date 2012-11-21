TEMPLATE=lib
TARGET=qtopia
MODULE_NAME=qtopia
VERSION=4.0.0

CONFIG+=qtopia hide_symbols singleexec
QTOPIA=base gfx
enable_sxe:QTOPIA*=security
QT*=sql xml svg
MODULES*=md5 realtime sqlite zlib

STRING_LANGUAGE=en_US
AVAILABLE_LANGUAGES=$$QTOPIA_AVAILABLE_LANGUAGES
LANGUAGES=$$QTOPIA_LANGUAGES
UNIFIED_NCT_LUPDATE=1
TRANSLATION_SUBDIR=translations

pkg [
    name=libqtopia
    desc="Main library for Qt Extended."
    version=$$QTOPIA_VERSION
    license=$$QTOPIA_LICENSE
    maintainer=$$QTOPIA_MAINTAINER
]

DEFINES+=QTOPIA_PAGE_SIZE=$$define_value($$QTOPIA_PAGE_SIZE)
DEFINES+=QTOPIA_PAGE_MASK=$$define_value($$QTOPIA_PAGE_MASK)
# Needed for includes from qtextengine_p.h
INCLUDEPATH+=$$QT_DEPOT_PATH/src/3rdparty/harfbuzz/src

FORMS=\
    passwordbase_p.ui

HEADERS=\
    qmimetype.h\
    qdocumentselector.h\
    qtopiaapplication.h\
    qterminationhandler.h\
    qtimezoneselector.h\
    qworldmapdialog.h\
    qworldmap.h\
    qpassworddialog.h\
    qtimestring.h\
    qcategorymanager.h\
    qcategoryselector.h\
    qwindowdecorationinterface.h\
    mediarecorderplugininterface.h\
    inputmethodinterface.h\
    qapplicationplugin.h\
    qpluginmanager.h\
    qwaitwidget.h\
    qtimezone.h\
    qthumbnail.h\
    qimagedocumentselector.h\
    qtopiasql.h\
    qiconselector.h\
    inheritedshareddata.h\
    qtopiaserviceselector.h\
    qcolorselector.h\
    qdocumentproperties.h\
    qdevicebutton.h\
    qdevicebuttonmanager.h\
    qdl.h\
    qdlbrowserclient.h\
    qdlclient.h\
    qdleditclient.h\
    qdllink.h\
    qdsaction.h\
    qdsservices.h\
    qdsserviceinfo.h\
    qdsactionrequest.h\
    qdsdata.h\
    qtopianetwork.h\
    qtopianetworkinterface.h\
    qtopiastyle.h\
    qphonestyle.h\
    qsoftmenubar.h\
    qspeeddial.h\
    qfavoriteservicesmodel.h\
    qstoragedeviceselector.h\
    qconstcstring.h\
    qtopiafeatures.h\
    qdeviceindicators.h\
    themedview.h\
    qphoneprofile.h\
    qhardwareinterface.h\
    qhardwaremanager.h\
    qpowersource.h\
    qbootsourceaccessory.h\
    qvibrateaccessory.h\
    qkeypadlightaccessory.h\
    qscreeninformation.h\
    qtopiasendvia.h\
    qsignalsource.h \
    qanalogclock.h \
    qcontentsortcriteria.h \
    qsoftkeylabelhelper.h \
    qtextentryproxy.h \
    qtopiadocumentserver.h \
    qaudiosourceselector.h \
    qimagesourceselector.h \
    qcontent.h \
    qcontentset.h \
    qdrmcontent.h \
    qcontentplugin.h \
    qcontentfilter.h \
    qdrmcontentplugin.h \
    qdrmrights.h \
    qcontentfiltermodel.h \
    qcontentfilterselector.h\
    qperformancelog.h \
    qdocumentselectorservice.h \
    qtopiaitemdelegate.h\
    qtopiaservicehistorymodel.h\
    qexifimageheader.h \
    servertaskplugin.h\
    qsmoothlist.h\
    qcontentproperties.h

PRIVATE_HEADERS=\
    ezxphonestyle_p.h\
    qimagedocumentselector_p.h\
    thumbnailview_p.h\
    singleview_p.h\
    qtopiaresource_p.h\
    qdl_p.h\
    qdlsourceselector_p.h\
    qdlwidgetclient_p.h\
    qds_p.h\
    qdsserviceinfo_p.h\
    qdsactionrequest_p.h\
    qdsaction_p.h\
    qdsdata_p.h\
    qhardwareinterface_p.h\
    qworldmap_sun_p.h\
    qworldmap_stylusnorm_p.h \
    qtopiasql_p.h \
    qcontentengine_p.h \
    qcontentstore_p.h \
    qcontentsetengine_p.h \
    qfscontentengine_p.h \
    qsqlcontentsetengine_p.h \
    qdrmcontentengine_p.h \
    qcategorystore_p.h \
    qsqlcategorystore_p.h \
    qmimetypedata_p.h \
    qdocumentserverchannel_p.h \
    qdocumentservercategorystore_p.h \
    qcategorystoreserver_p.h \
    qdocumentservercontentstore_p.h \
    qdocumentservercontentsetengine_p.h \
    qcontentstoreserver_p.h \
    qdocumentservercontentengine_p.h \
    qsqlcontentstore_p.h \
    qsoftkeylabelhelper_p.h \
    resourcesourceselector_p.h \
    drmcontent_p.h \
    qdocumentselectorsocketserver_p.h \
    qcontentfilterselector_p.h \
    qsparselist_p.h \
    qtopiamessagehandler_p.h \
    qtopiaservicehistorymodel_p.h\
    qtagmap_p.h \
    keyboard_p.h

SEMI_PRIVATE_HEADERS=\
    qterminationhandler_p.h\
    themedviewinterface_p.h\
    qcontent_p.h\
    contextkeymanager_p.h\
    contentpluginmanager_p.h \
    qsmoothlistwidget_p.h\
    qthumbstyle_p.h\
    pred_p.h\
    qtopiainputdialog_p.h\
    qtopiasqlmigrateplugin_p.h

MOC_COMPILE_EXCEPTIONS+=qtopiainputdialog_p.h

SOURCES=\
    qmimetype.cpp\
    qdocumentselector.cpp\
    qterminationhandler.cpp\
    qtopiaapplication.cpp\
    qtimezoneselector.cpp\
    qworldmapdialog.cpp\
    qworldmap.cpp\
    qworldmap_sun.cpp\
    qpassworddialog.cpp\
    qtimestring.cpp\
    qcategoryselector.cpp\
    qwindowdecorationinterface.cpp\
    mediarecorderplugininterface.cpp\
    qapplicationplugin.cpp\
    inputmethodinterface.cpp\
    qpluginmanager.cpp\
    qcategorymanager.cpp\
    qwaitwidget.cpp\
    qtimezone.cpp\
    qthumbnail.cpp\
    qimagedocumentselector.cpp\
    qimagedocumentselector_p.cpp\
    thumbnailview_p.cpp\
    singleview_p.cpp\
    qtopiasql.cpp\
    qiconselector.cpp\
    qtopiaresource.cpp\
    qtopiaserviceselector.cpp\
    qtopiaservicehistorymodel.cpp\
    qtopiaservicehistorymodel_p.cpp\
    qcolorselector.cpp\
    qdocumentproperties.cpp\
    qstoragedeviceselector.cpp\
    qdevicebutton.cpp\
    qdevicebuttonmanager.cpp\
    qtopiastyle.cpp\
    qdl.cpp\
    qdlbrowserclient.cpp\
    qdlclient.cpp\
    qdleditclient.cpp\
    qdllink.cpp\
    qdlsourceselector.cpp\
    qdlwidgetclient.cpp\
    qdsaction.cpp\
    qdsservices.cpp\
    qdsserviceinfo.cpp\
    qdsactionrequest.cpp\
    qdsdata.cpp\
    qtopiamessagehandler.cpp\
    qtopianetwork.cpp\
    qtopianetworkinterfaceimpl.cpp\
    qtopiafeatures.cpp\
    qdeviceindicators.cpp\
    qphoneprofile.cpp\
    qcontent_p.cpp\
    qhardwareinterface.cpp\
    qhardwaremanager.cpp\
    qpowersource.cpp\
    qbootsourceaccessory.cpp\
    qvibrateaccessory.cpp\
    qkeypadlightaccessory.cpp\
    qscreeninformation.cpp\
    qtopiasendvia.cpp\
    qanalogclock.cpp\
    qsignalsource.cpp \
    qtopiasql_p.cpp\
    qcontentengine.cpp \
    qcontentstore.cpp \
    qsqlcontentstore.cpp \
    qcontentsetengine.cpp \
    qsqlcontentsetengine.cpp \
    qdrmcontentengine.cpp \
    qcategorystore.cpp \
    qsqlcategorystore.cpp \
    qmimetypedata.cpp \
    qdocumentserverchannel.cpp \
    qdocumentservercategorystore.cpp \
    qcategorystoreserver.cpp \
    qdocumentservercontentstore.cpp \
    qdocumentservercontentsetengine.cpp \
    qcontentstoreserver.cpp \
    qdocumentservercontentengine.cpp \
    qcontentsortcriteria.cpp \
    qfscontentengine.cpp \
    qsoftkeylabelhelper.cpp \
    qtextentryproxy.cpp \
    qtopiadocumentserver.cpp \
    qaudiosourceselector.cpp \
    qimagesourceselector.cpp \
    resourcesourceselector.cpp \
    themedview.cpp \
    themedviewinterface.cpp \
    qcontent.cpp \
    qcontentset.cpp \
    qdrmcontent.cpp \
    drmcontent_p.cpp \
    qcontentplugin.cpp \
    contentpluginmanager_p.cpp \
    qcontentfilter.cpp \
    qdrmcontentplugin.cpp \
    qdrmrights.cpp \
    qcontentfiltermodel.cpp \
    qcontentfilterselector.cpp \
    qcontentproperties.cpp \
    contextkeymanager.cpp \
    qsoftmenubar.cpp \
    qphonestyle.cpp\
    ezxphonestyle.cpp\
    qspeeddial.cpp\
    qfavoriteservicesmodel.cpp\
    qperformancelog.cpp \
    qdocumentselectorservice.cpp \
    qdocumentselectorsocketserver.cpp \
    qtopiaitemdelegate.cpp \
    qthumbstyle_p.cpp \
    servertaskplugin.cpp \
    qexifimageheader.cpp \
    qsmoothlist.cpp \
    qsmoothlistwidget_p.cpp \
    qtopiainputdialog_p.cpp\
    pred_p.cpp\
    keyboard_p.cpp\
    qtopiasqlmigrateplugin.cpp

HEADERS+=\
    qexportedbackground.h

X11 [
    TYPE=CONDITIONAL_SOURCES
    CONDITION=x11
    SOURCES=qexportedbackground_x11.cpp
]

NOT_X11 [
    TYPE=CONDITIONAL_SOURCES
    CONDITION=!x11
    PRIVATE_HEADERS=\
        qpedecoration_p.h\
        phonedecoration_p.h
    SOURCES=\
        qpedecoration_qws.cpp\
        qexportedbackground.cpp\
        phonedecoration.cpp
]

RESOURCES=qtopia.qrc

# Install rules

etc [
    hint=image
    files=etc/mime.types
    path=/etc
]

colors [
    hint=image
    files=etc/colors/*
    path=/etc/colors
]

bins [
    hint=script
    files=bin/qtopia-addmimetype
    path=/bin
]

pics [
    hint=pics
    files=pics/global/*
    path=/pics
]

phonepics [
    hint=pics
    files=pics/phone/*
    path=/pics/phone
]

drmpics [
    hint=pics
    files=pics/drm/*
    path=/pics/drm
]

worldtimepics [
    hint=pics
    files=pics/libqtopia/*
    path=/pics/libqtopia
]

settings [
    hint=image
    files=\
        etc/default/Trolltech/presstick.conf\
        etc/default/Trolltech/SpeedDial.conf\
        etc/default/Trolltech/WorldTime.conf\
        etc/default/Trolltech/Log.conf\
        etc/default/Trolltech/Log2.conf\
        etc/default/Trolltech/Storage.conf\
        etc/default/Trolltech/PhoneProfile.conf
    path=/etc/default/Trolltech
]

zonetab [
    # We're really only interested in the translations for zone.tab
    hint=nct
    trtarget=timezone
    files=zone.tab
]

mainapps_category [
    hint=desktop prep_db
    files=mainapps.directory
    path=/apps/MainApplications
]

app_categories [
    hint=desktop prep_db
    files=apps.directory
    path=/apps/Applications
    # This category uses the MainApplications category
    depends=install_docapi_mainapps_category
]

settings_category [
    hint=desktop prep_db
    files=settings.directory
    path=/apps/Settings
    # This category uses the MainApplications category
    depends=install_docapi_mainapps_category
]

equals(QTOPIA_UI,home) {
    deskphonelauncher_category [
        hint=desktop prep_db
        files=deskphone.directory
        path=/apps/DeskphoneLauncher
    ]
}

games_category [
    hint=desktop prep_db
    files=games.directory
    path=/apps/Games
    # This category uses the MainApplications category
    depends=install_docapi_mainapps_category
]

