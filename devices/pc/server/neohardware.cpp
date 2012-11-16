/****************************************************************************
**
** This file is part of the Qt Extended Opensource Package.
**
** Copyright (C) 2009 Trolltech ASA.
** Copyright (C) 2012 Radek Polak.
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

#include "neohardware.h"

#include <QSocketNotifier>
#include <QTimer>
#include <QLabel>
#include <QDesktopWidget>
#include <QProcess>
#include <QPowerSourceProvider>

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

// Setup netlink socket for watching usb cable and battery events
static int openNetlink()
{
    struct sockaddr_nl snl;
    memset(&snl, 0x00, sizeof(struct sockaddr_nl));
    snl.nl_family = AF_NETLINK;
    snl.nl_pid = getpid();
    snl.nl_groups = 1;
    snl.nl_groups = 0x1;

    int fd = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);
    if (fd == -1) {
        qLog(Hardware) << "error getting uevent socket: " << strerror(errno);
        return -1;
    }
    if (bind(fd, (struct sockaddr *)&snl, sizeof(struct sockaddr_nl)) < 0) {
        qLog(Hardware) << "uevent bind failed: " << strerror(errno);
        return -1;
    }
    return fd;
}

NeoHardware::NeoHardware()
:
    ueventSocket(this)
{
    qLog(Hardware) << "pc hardware";

    int netlinkFd = openNetlink();
    if (netlinkFd >= 0) {
        ueventSocket.setSocketDescriptor(netlinkFd);
        connect(&ueventSocket, SIGNAL(readyRead()), this, SLOT(uevent()));
    }
}

NeoHardware::~NeoHardware()
{
}

#define UEVENT_BUFFER_SIZE 1024
void NeoHardware::uevent()
{
    char buffer[1024];
    if (ueventSocket.read(buffer, UEVENT_BUFFER_SIZE) <= 0) {
        return;
    }
}