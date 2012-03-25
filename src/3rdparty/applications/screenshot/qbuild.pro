TEMPLATE=app
TARGET=screenshot
CONFIG+=qtopia no_singleexec no_quicklaunch

# I18n info
STRING_LANGUAGE=en_US
AVAILABLE_LANGUAGES=$$QTOPIA_AVAILABLE_LANGUAGES
LANGUAGES=$$QTOPIA_LANGUAGES

HEADERS             = screenshot.h
SOURCES             = main.cpp \
                      screenshot.cpp
desktop [
	files=screenshot.desktop
	path=/apps/Applications
	hint=desktop
]

pics [
	files=pics/*
	path=/pics/screenshot
	hint=pics
]

pkg [
	name=screenshot
	desc="Screenshot Application"
    version=$$QTOPIA_VERSION
    license=$$QTOPIA_LICENSE
    maintainer=$$QTOPIA_MAINTAINER
]

target [
    hint=sxe
    domain=trusted
]
