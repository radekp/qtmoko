/****************************************************************************
**
** This file is part of the Qt Extended Opensource Package.
**
** Copyright (C) 2009 Trolltech ASA.
**
** Contact: Qt Extended Information (info@qtextended.org)
**
** This file may be used under the terms of the GNU General Public License
** version 2.0 as published by the Free Software Foundation and appearing
** in the file LICENSE.GPL included in the packaging of this file.
**
** Please review the following information to ensure GNU General Public
** Licensing requirements will be met:
**     http://www.fsf.org/licensing/licenses/info/GPLv2.html.
**
**
****************************************************************************/

#ifdef QT_QWS_NEO

#include "neohardware.h"

#include <QSocketNotifier>
#include <QTimer>
#include <QLabel>
#include <QDesktopWidget>
#include <QProcess>
#include <QtopiaIpcAdaptor>

#include <qcontentset.h>
#include <qtopiaapplication.h>
#include <qtopialog.h>
#include <qtopiaipcadaptor.h>

#include <qbootsourceaccessory.h>
#include <qtopiaipcenvelope.h>

#include <qtopiaserverapplication.h>

#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <linux/input.h>

#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <linux/types.h>
#include <linux/netlink.h>
#include <QTcpSocket>
#include <QtDebug>

QTOPIA_TASK(NeoHardware, NeoHardware);


NeoHardware::NeoHardware()
    : vsoPortableHandsfree("/Hardware/Accessories/PortableHandsfree"),
      vsoUsbCable("/Hardware/UsbGadget"),
      vsoNeoHardware("/Hardware/Neo"),
      batteryVso("/UI/Battery", this)
{
    struct sockaddr_nl snl;
    adaptor = new QtopiaIpcAdaptor("QPE/NeoHardware");
    qLog(Hardware) << "neohardware";

    memset(&snl, 0x00, sizeof(struct sockaddr_nl));
    snl.nl_family = AF_NETLINK;
    snl.nl_pid = getpid();
    snl.nl_groups = 1;
    snl.nl_groups = 0x1;

    int hotplug_sock = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);
    if (hotplug_sock == -1) {
        qLog(Hardware) << "error getting uevent socket: "<< strerror(errno);
    } else {
       if ( bind(hotplug_sock, (struct sockaddr *) &snl, sizeof(struct sockaddr_nl)) < 0) {
         qLog(Hardware) << "uevent bind failed: "<< strerror(errno);
         hotplug_sock = -1;
       }else {
         ueventSocket = new QTcpSocket(this);
         ueventSocket->setSocketDescriptor(hotplug_sock);
         connect(ueventSocket, SIGNAL(readyRead()), this, SLOT(uevent()));
       }
    }

    cableConnected(getCableStatus());

    vsoPortableHandsfree.setAttribute("Present", false);
    vsoPortableHandsfree.sync();

// Handle Audio State Changes
    audioMgr = new QtopiaIpcAdaptor("QPE/AudioStateManager", this);


    QtopiaIpcAdaptor::connect(adaptor, MESSAGE(headphonesInserted(bool)),
                              this, SLOT(headphonesInserted(bool)));

    QtopiaIpcAdaptor::connect(adaptor, MESSAGE(cableConnected(bool)),
                              this, SLOT(cableConnected(bool)));
    findHardwareVersion();
}

NeoHardware::~NeoHardware()
{
}

char *NeoHardware::findAttribute(char *buf, int len, const char *token)
{
int pos=0;

  while (pos<len)
  {
    if(strncmp(&buf[pos],token,strlen(token))==0)
	return(&buf[pos+strlen(token)]);
    pos=pos+strlen(&buf[pos])+1;
  }
  return(buf);
}

static QByteArray readFile(const char *path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        qLog(PowerManagement) << "file open failed" << path << ":" <<
            f.errorString();
        return QByteArray();
    }
    QByteArray content = f.readAll();
    f.close();
    return content;
}

void NeoHardware::uevent()
{
#define UEVENT_BUFFER_SIZE 1024
char buffer[UEVENT_BUFFER_SIZE];
char *value;

  int bytesAvail = ueventSocket->bytesAvailable();
  int readCount = UEVENT_BUFFER_SIZE;
  if (bytesAvail < readCount)
      readCount = bytesAvail;
  ueventSocket->read(&buffer[0],readCount);
  if(strcmp(buffer,"change@/devices/platform/s3c2440-i2c/i2c-0/0-0073/pcf50633-mbc.0/power_supply/usb")==0)
  {
    qLog(PowerManagement)<<"usb change event";
    cableConnected(getCableStatus());
  }else if(strcmp(buffer,"change@/devices/platform/s3c2440-i2c/i2c-0/0-0073/pcf50633-mbc.0/power_supply/ac")==0)
  {
    qLog(PowerManagement)<<"ac change event";
    cableConnected(getCableStatus());
  }else if(strcmp(buffer,"change@/devices/platform/s3c2440-i2c/i2c-0/0-0073/pcf50633-mbc.0/power_supply/adapter")==0)
  {
    value=findAttribute(buffer,readCount,"POWER_SUPPLY_ONLINE=");
    qLog(PowerManagement)<<"power_supply change event; online="<<value;
  }else if(strcmp(buffer,"change@/devices/platform/s3c2440-i2c/i2c-0/0-0073/hdq/bq27000-battery.0/power_supply/battery")==0)
  {
    value=findAttribute(buffer,readCount,"POWER_SUPPLY_CAPACITY=");
    qLog(PowerManagement)<<"battery change event charge%="<<value<<"%";
  }else if(strcmp(buffer,"change@/class/switch/headset")==0)
  {
    value=findAttribute(buffer,readCount,"SWITCH_STATE=");
    qDebug()<<"headset change event, switch_state="<<value;
  }
  
    QString currentNowStr =
        readFile("/sys/class/power_supply/battery/current_now");
    int currentNow = currentNowStr.toInt() / 1000;
    batteryVso.setAttribute("current_now", QString::number(currentNow));
}

void NeoHardware::findHardwareVersion()
{
    QFile cpuinfo( "/proc/cpuinfo");
    QString inStr;
    cpuinfo.open(QIODevice::ReadOnly | QIODevice::Text);
    QTextStream in(&cpuinfo);
    QString line;
    do {
        line  = in.readLine();
        if (line.contains("Hardware") ){
            QStringList token = line.split(":");
            inStr = token.at(1).simplified();
        }
    } while (!line.isNull());

    cpuinfo.close();
    qLog(Hardware)<<"Neo"<< inStr;

    vsoNeoHardware.setAttribute("Device", inStr);
    vsoNeoHardware.sync();
}

void NeoHardware::headphonesInserted(bool b)
{
    qLog(Hardware)<< __PRETTY_FUNCTION__ << b;
    vsoPortableHandsfree.setAttribute("Present", b);
    vsoPortableHandsfree.sync();
    if (b) {
        QByteArray mode("Headphone");
        audioMgr->send("setProfile(QByteArray)", mode);
    } else {
        QByteArray mode("MediaSpeaker");
        audioMgr->send("setProfile(QByteArray)", mode);
    }


}

void NeoHardware::cableConnected(bool b)
{
    qLog(Hardware)<< __PRETTY_FUNCTION__ << b;
    vsoUsbCable.setAttribute("cableConnected", b);
    vsoUsbCable.sync();
}

void NeoHardware::shutdownRequested()
{
    qLog(PowerManagement)<< __PRETTY_FUNCTION__;

    QFile powerFile("/sys/devices/platform/s3c2440-i2c/i2c-0/0-0073/pcf50633-gpio.0/reg-fixed-voltage.1/gta02-pm-gsm.0/power_on");
    QFile btPower("/sys/devices/platform/gta02-pm-bt.0/power_on");

    if( !powerFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        qWarning()<<"File not opened";
    } else {
        QTextStream out(&powerFile);
        out << "0";
        powerFile.close();
    }

        if( !btPower.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        qWarning()<<"File not opened";
    } else {
        QTextStream out(&btPower);
        out <<  "0";
        powerFile.close();
    }


    QtopiaServerApplication::instance()->shutdown(QtopiaServerApplication::ShutdownSystem);
}

bool NeoHardware::getCableStatus()
{
    // These code from NeoBattery::isCharging()
    // Seems better than the origin method
    qLog(PowerManagement) << __PRETTY_FUNCTION__;
    QString charge;
    QFile chargeState("/sys/class/power_supply/battery/status");
    chargeState.open(QIODevice::ReadOnly | QIODevice::Text);
    QTextStream in(&chargeState);
    in >> charge;
    qLog(PowerManagement) << __PRETTY_FUNCTION__ << charge;
    // Charging  Discharging  Not charging
    // ac        battery      ac/full
    chargeState.close();
    return (charge != ("Discharging"));
}

#endif // QT_QWS_NEO

