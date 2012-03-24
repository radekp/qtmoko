REQUIRES=enable_cell
QTOPIA*=phone
MODULES*=qofono

SERVER_DEPS*=\
    /src/server/core_server\
    /src/server/phone/telephony/phoneserver/base\

HEADERS+=\
        ofonoutil.h\
        ofonotelephonyservice.h\
        ofonotelephonyservicefactory.h\
        ofononetworkregistration.h\
        ofonosupplementaryservices.h\
        ofonoservicechecker.h\
        ofonorffunctionality.h\
        ofonocallprovider.h\
        ofonophonecall.h\
        ofonosmssender.h\
        ofonosmsreader.h\
        ofonosiminfo.h\
        ofonophonebook.h \
        llindicators.h

SOURCES+=\
        ofonoutil.cpp\
        ofonotelephonyservicefactory.cpp\
        ofonotelephonyservice.cpp\
        ofononetworkregistration.cpp\
        ofonosupplementaryservices.cpp\
        ofonoservicechecker.cpp\
        ofonorffunctionality.cpp\
        ofonocallprovider.cpp\
        ofonophonecall.cpp\
        ofonosmssender.cpp\
        ofonosmsreader.cpp\
        ofonosiminfo.cpp\
        ofonophonebook.cpp\
        llindicators.cpp

