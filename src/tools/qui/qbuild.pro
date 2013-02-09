TEMPLATE=app
TARGET=qui

CONFIG+=qtopia singleexec

DEFINES+=QTOPIA

pkg [
    name=qui
    desc="Tool for showing simple dialogs from command line."
    version=$$QTOPIA_VERSION
    license=$$QTOPIA_LICENSE
    maintainer=$$QTOPIA_MAINTAINER
]

SOURCES=\
    main.cpp

# Install rules
target [
    hint=sxe
    domain=trusted
]

