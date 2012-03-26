TEMPLATE=app
CONFIG+=qtopia
TARGET=photoedit

CONFIG+=quicklaunch singleexec

STRING_LANGUAGE=en_US
AVAILABLE_LANGUAGES=$$QTOPIA_AVAILABLE_LANGUAGES
LANGUAGES=$$QTOPIA_LANGUAGES
UNIFIED_NCT_LUPDATE=1
TRANSLATION_SUBDIR=translations

pkg [
    name=pictures
    desc="Pictures application for Qt Extended."
    version=$$QTOPIA_VERSION
    license=$$QTOPIA_LICENSE
    maintainer=$$QTOPIA_MAINTAINER
]

HEADERS=\
    photoeditui.h\
    imageviewer.h\
    thumbnailmodel.h\
    editor/imageui.h\
    editor/slider.h\
    editor/navigator.h\
    editor/regionselector.h\
    editor/matrix.h\
    editor/imageprocessor.h\
    editor/imageio.h\
    slideshow/slideshowui.h\
    slideshow/slideshowdialog.h\
    slideshow/slideshow.h\
    colorpicker.h\
    effectdialog.h\
    effectmodel.h\
    effectsettingswidget.h\
    photoediteffect.h

SOURCES=\
    main.cpp\
    photoeditui.cpp\
    imageviewer.cpp\
    thumbnailmodel.cpp\
    editor/imageui.cpp\
    editor/slider.cpp\
    editor/navigator.cpp\
    editor/regionselector.cpp\
    editor/imageprocessor.cpp\
    editor/imageio.cpp\
    slideshow/slideshowui.cpp\
    slideshow/slideshowdialog.cpp\
    slideshow/slideshow.cpp\
    colorpicker.cpp\
    effectdialog.cpp\
    effectmodel.cpp\
    effectsettingswidget.cpp\
    photoediteffect.cpp

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
    files=photoedit.desktop
    path=/apps/Applications
]

pics [
    hint=pics
    files=pics/*
    path=/pics/photoedit
]

service [
    hint=image
    files=services/PhotoEdit/photoedit
    path=/services/PhotoEdit
]

qdsservice [
    hint=image
    files=etc/qds/PhotoEdit
    path=/etc/qds
]

