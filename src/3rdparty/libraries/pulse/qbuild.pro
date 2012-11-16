MODULE_NAME=pulse
requires(exists(/config.tests/sound_system/pulse.pri))
include(/config.tests/sound_system/pulse.pri)

DEP.headers [
    TYPE=DEPENDS PERSISTED SDK
    EVAL=\
        "MKSPEC.CFLAGS+="$$PULSE_CFLAGS\
        "MKSPEC.CXXFLAGS+="$$PULSE_CFLAGS
]

DEP.libs [
    TYPE=DEPENDS PERSISTED SDK
    EVAL="LIBS+="$$PULSE_LIBS
]

