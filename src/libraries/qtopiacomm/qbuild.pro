TEMPLATE=lib
TARGET=qtopiacomm
MODULE_NAME=qtopiacomm
VERSION=4.0.0

CONFIG+=qtopia hide_symbols singleexec
QT*=network

STRING_LANGUAGE=en_US
AVAILABLE_LANGUAGES=$$QTOPIA_AVAILABLE_LANGUAGES
LANGUAGES=$$QTOPIA_LANGUAGES
TRANSLATION_SUBDIR=translations

pkg [
    name=libcomm
    desc="Communcations library for Qt Extended."
    version=$$QTOPIA_VERSION
    license=$$QTOPIA_LICENSE
    maintainer=$$QTOPIA_MAINTAINER
]

HEADERS=\
    qcommdevicecontroller.h\
    qcommdevicesession.h\
    qcomminterface.h\
    qcommservicemanager.h

SOURCES=\
    qcommdevicecontroller.cpp\
    qcommdevicesession.cpp\
    qcomminterface.cpp\
    qcommservicemanager.cpp

contains(PROJECTS,3rdparty/libraries/openobex) {
    MODULES*=openobex
    VPATH+=obex
    DEFINES+=QTOPIA_OBEX
    CONFIG+=enable_obex
}
OBEX [
    TYPE=CONDITIONAL_SOURCES
    CONDITION=enable_obex
    HEADERS=\
        qobexglobal.h\
        qobexpushclient.h\
        qobexpushservice.h\
        qobexnamespace.h\
        qobexheader.h\
        qobexserversession.h\
        qobexclientsession.h\
        qobexauthenticationchallenge.h\
        qobexauthenticationresponse.h\
        qobexfolderlistingentryinfo.h\
        qobexftpclient.h
    PRIVATE_HEADERS=\
        qobexcommand_p.h\
        qobexheader_p.h\
        qobexauthentication_p.h\
        qobexauthenticationchallenge_p.h\
        qobexauthenticationresponse_p.h\
        qobexsocket_p.h\
        qobexclientsession_p.h\
        qobexserversession_p.h\
        qobexfolderlisting_p.h
    SOURCES=\
        qobexpushclient.cpp\
        qobexpushservice.cpp\
        qobexsocket.cpp\
        qobexcommand_p.cpp\
        qobexheader.cpp\
        qobexserversession.cpp\
        qobexclientsession.cpp\
        qobexauthenticationchallenge.cpp\
        qobexauthenticationresponse.cpp\
        qobexauthentication_p.cpp\
        qobexfolderlistingentryinfo.cpp\
        qobexfolderlisting.cpp\
        qobexftpclient.cpp 
]

enable_bluetooth {
    VPATH+=bluetooth
    QT+=dbus
}
BLUETOOTH [
    TYPE=CONDITIONAL_SOURCES
    CONDITION=enable_bluetooth
    HEADERS=\
        qbluetoothaddress.h\
        qbluetoothdbus.h\
        qbluetoothlocaldevice.h\
        qbluetoothlocaldevicemanager.h\
        qbluetoothnamespace.h\
        qbluetoothpasskeyagent.h\
        qbluetoothdefaultpasskeyagent.h\
        qbluetoothpasskeyrequest.h\
        qbluetoothremotedevice.h\
        qbluetoothabstractserver.h\
        qbluetoothscoserver.h\
        qbluetoothrfcommserver.h\
        qbluetoothabstractsocket.h\
        qbluetoothscosocket.h\
        qbluetoothrfcommsocket.h\
        qbluetoothsdpquery.h\
        qbluetoothsdprecord.h\
        qbluetoothsdpuuid.h\
        qbluetoothremotedevicedialog.h\
        qbluetoothrfcommserialport.h\
        qbluetoothabstractservice.h\
        qbluetoothservicecontroller.h\
        qbluetoothaudiogateway.h\
        qbluetoothl2capsocket.h\
        qbluetoothl2capserver.h\
        qbluetoothl2capdatagramsocket.h\
        qbluetoothglobal.h\
        qbluetoothauthorizationagent.h
    PRIVATE_HEADERS=\
        qbluetoothremotedevicedialog_p.h\
        qsdpxmlgenerator_p.h\
        qbluetoothabstractsocket_p.h\
        qbluetoothabstractserver_p.h
    SEMI_PRIVATE_HEADERS=\
        qbluetoothnamespace_p.h\
        qsdpxmlparser_p.h\
        qbluetoothremotedeviceselector_p.h
    SOURCES=\
        qbluetoothaddress.cpp\
        qbluetoothdbus.cpp\
        qbluetoothlocaldevice.cpp\
        qbluetoothlocaldevicemanager.cpp\
        qbluetoothnamespace.cpp\
        qbluetoothpasskeyagent.cpp\
        qbluetoothdefaultpasskeyagent.cpp\
        qbluetoothpasskeyrequest.cpp\
        qbluetoothabstractserver.cpp\
        qbluetoothscoserver.cpp\
        qbluetoothrfcommserver.cpp\
        qbluetoothabstractsocket.cpp\
        qbluetoothscosocket.cpp\
        qbluetoothrfcommsocket.cpp\
        qbluetoothremotedevice.cpp\
        qbluetoothsdpquery.cpp\
        qbluetoothsdprecord.cpp\
        qbluetoothsdpuuid.cpp\
        qbluetoothremotedeviceselector.cpp\
        qbluetoothremotedevicedialog.cpp\
        qbluetoothrfcommserialport.cpp\
        qbluetoothabstractservice.cpp\
        qbluetoothservicecontroller.cpp\
        qbluetoothaudiogateway.cpp\
        qsdpxmlparser.cpp\
        qsdpxmlgenerator.cpp\
        qbluetoothl2capsocket.cpp\
        qbluetoothl2capserver.cpp\
        qbluetoothl2capdatagramsocket.cpp\
        qbluetoothauthorizationagent.cpp
]

BLUETOOTH_UNIX [
    TYPE=CONDITIONAL_SOURCES
    CONDITION=enable_bluetooth:unix
    SOURCES=qbluetoothsocketengine_unix.cpp
]

enable_infrared:VPATH+=ir
IR [
    TYPE=CONDITIONAL_SOURCES
    CONDITION=enable_infrared
    HEADERS=\
        qiriasdatabase.h\
        qirlocaldevice.h\
        qirnamespace.h\
        qirremotedevice.h \
        qirremotedevicewatcher.h \
        qirserver.h \
        qirsocket.h \
        qirglobal.h
    PRIVATE_HEADERS=\
        qirnamespace_p.h \
        qirsocketengine_p.h
    SOURCES=\
        qiriasdatabase.cpp\
        qirlocaldevice.cpp\
        qirnamespace.cpp\
        qirremotedevice.cpp \
        qirremotedevicewatcher.cpp \
        qirserver.cpp \
        qirsocket.cpp
]

IR_UNIX [
    TYPE=CONDITIONAL_SOURCES
    CONDITION=enable_infrared:unix
    SOURCES=qirsocketengine_unix.cpp
]

VPATH+=network
FORMS+=\
    proxiesconfigbase.ui

HEADERS+=\
    ipconfig.h\
    accountconfig.h\
    proxiesconfig.h\
    qnetworkstate.h\
    scriptthread.h\
    qnetworkdevice.h\
    ipvalidator.h \
    hexkeyvalidator.h\
    qtopiahttp.h\
    qwapaccount.h\
    qwlanregistration.h\
    qnetworkconnection.h

MOC_COMPILE_EXCEPTIONS+=\
    qnetworkconnection.h\
    accountconfig.h

SOURCES+=\
    ipconfig.cpp\
    accountconfig.cpp\
    proxiesconfig.cpp\
    qnetworkstate.cpp\
    scriptthread.cpp\
    qnetworkdevice.cpp\
    ipvalidator.cpp\
    hexkeyvalidator.cpp\
    qtopiahttp.cpp\
    qwapaccount.cpp\
    qwlanregistration.cpp\
    qnetworkconnection.cpp

VPATH+=serial
HEADERS+=\
    qgsm0710multiplexer.h\
    qserialiodevicemultiplexer.h\
    qserialiodevicemultiplexerplugin.h\
    qmultiportmultiplexer.h\
    qserialiodevice.h\
    qserialport.h\
    qserialsocket.h\
    qatchat.h\
    qatresult.h\
    qatresultparser.h\
    qatchatscript.h\
    qretryatchat.h\
    qatutils.h\
    qgsmcodec.h

PRIVATE_HEADERS+=\
    qserialiodevice_p.h\
    qprefixmatcher_p.h\
    qatchat_p.h\
    qpassthroughserialiodevice_p.h\
    gsm0710_p.h

SOURCES+=\
    qgsm0710multiplexer.cpp\
    qserialiodevicemultiplexer.cpp\
    qserialiodevicemultiplexerplugin.cpp\
    qmultiportmultiplexer.cpp\
    qserialiodevice.cpp\
    qserialport.cpp\
    qserialsocket.cpp\
    qatchat.cpp\
    qatresult.cpp\
    qatresultparser.cpp\
    qatchatscript.cpp\
    qprefixmatcher.cpp\
    qretryatchat.cpp\
    qatutils.cpp\
    qgsmcodec.cpp\
    qpassthroughserialiodevice.cpp\
    gsm0710.c


enable_vpn:VPATH+=vpn
VPN [
    TYPE=CONDITIONAL_SOURCES
    CONDITION=enable_vpn
    FORMS=\
        generalopenvpnbase.ui\
        certificateopenvpnbase.ui\
        optionsopenvpnbase.ui\
        deviceopenvpnbase.ui
    HEADERS=\
        qvpnclient.h\
        qvpnfactory.h 
    PRIVATE_HEADERS+=\
        qvpnclientprivate_p.h\
        qopenvpn_p.h\
        qipsec_p.h\
        qopenvpngui_p.h
    SOURCES+=\
        qvpnclient.cpp\
        qvpnfactory.cpp\
        qopenvpn.cpp\
        qopenvpngui.cpp\
        qipsec.cpp
]

VPATH+=usb
HEADERS+=\
    qusbmanager.h\
    qusbgadget.h\
    qusbethernetgadget.h\
    qusbserialgadget.h\
    qusbstoragegadget.h

SOURCES+=\
    qusbmanager.cpp\
    qusbgadget.cpp\
    qusbethernetgadget.cpp\
    qusbserialgadget.cpp\
    qusbstoragegadget.cpp

# Install rules

enable_bluetooth {
    bluetoothpics [
        hint=pics
        files=pics/bluetooth/*
        path=/pics/bluetooth
    ]
}

