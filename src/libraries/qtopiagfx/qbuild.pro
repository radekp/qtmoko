TEMPLATE=lib
TARGET=qtopiagfx
MODULE_NAME=qtopiagfx
VERSION=4.0.0

CONFIG+=qt embedded hide_symbols singleexec
DEPENDS*=/src/3rdparty/libraries/easing::persisted
LIBS += -ldl

pkg [
    name=qtopiagfx
    desc="GFX library for Qt Extended."
    version=$$QTOPIA_VERSION
    license=$$QTOPIA_LICENSE
    maintainer=$$QTOPIA_MAINTAINER
]

#DEFINES+=QT_GREENPHONE_OPT

HEADERS=\
    routines.h\
    gfx.h\
    def_blur.h\
    def_color.h\
    def_blend.h\
    gfxpainter.h\
    def_memory.h\
    gfxparticles.h\
    gfximage.h\
    def_transform.h\
    def_blendhelper.h\
    gfxtimeline.h\
    gfxmipimage.h\
    gfxeasing.h

SOURCES=\
    routines.cpp\
    gfx.cpp\
    def_blur.cpp\
    def_color.cpp\
    def_blend.cpp\
    def_memory.cpp\
    gfxpainter.cpp\
    gfxparticles.cpp\
    gfximage.cpp\
    def_transform.cpp\
    gfxtimeline.cpp\
    gfxmipimage.cpp\
    gfxeasing.cpp

