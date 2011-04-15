STRING_LANGUAGE=en_US
AVAILABLE_LANGUAGES=$$QTOPIA_AVAILABLE_LANGUAGES
LANGUAGES=$$QTOPIA_LANGUAGES
UNIFIED_NCT_LUPDATE=1

pkg [
    name=faenqomod-theme
    desc="Faenqomod theme for QtMoko."
    version=$$QTOPIA_VERSION
    license=$$QTOPIA_LICENSE
    maintainer=$$QTOPIA_MAINTAINER
]

conf [
    hint=themecfg
    files=faenqomod.conf
    path=/etc/themes
    trtarget=Theme-Faenqomod
]

data [
    hint=image
    files=*.xml *rc
    path=/etc/themes/faenqomod
]

pics [
    hint=pics
    files=pics/*
    path=/pics/themes/faenqomod
]

bgimage [
    hint=background
    files=pics/Abstract_r.png
    path=/pics/themes/faenqomod
]

