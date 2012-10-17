#!/bin/sh

# file to restart wifi after resume

. /opt/qtmoko/qpe.env

# run only if wifi is active
if [ -e /home/root/Documents/eth0.txt ]; then
    qcop QPE/Network 'privilegedInterfaceStop(QString)' "/home/root/Applications/Network/config/wlan0.conf"
    sleep 4;
    qcop QPE/Network 'startInterface(QString,QString)' "netsetup", "/home/root/Applications/Network/config/wlan0.conf"
fi
