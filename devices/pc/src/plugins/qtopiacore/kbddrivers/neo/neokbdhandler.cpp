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

#include "neokbdhandler.h"

#ifdef QT_QWS_GTA04
#include <QFile>
#include <QTextStream>
#include <QScreen>
#include <QSocketNotifier>
#include <QtCore/QDebug>
#include <QDir>

#include <qtopialog.h>
#include <QtopiaIpcEnvelope>
#include <QtopiaServiceRequest>
#include <QValueSpaceObject>
#include <QValueSpaceItem>

#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <QProcess>
#include <QFileInfo>

#include <linux/input.h>


bool operator==(const input_id& left, const input_id& right)
{
    return
        left.bustype == right.bustype &&
        left.vendor  == right.vendor  &&
        left.product == right.product &&
        left.version == right.version;
}

 FicLinuxInputEventHandler::FicLinuxInputEventHandler(QObject* parent)
    : QObject(parent),
      m_fd(-1),
      m_notifier(0)

{
}

bool FicLinuxInputEventHandler::openByPhysicalBus(const QByteArray& physical)
{
    return internalOpen(EVIOCGPHYS(4096), 4096, physical);
}

bool FicLinuxInputEventHandler::openByName(const QByteArray& name)
{
    return internalOpen(EVIOCGNAME(4096), 4096, name);
}

bool FicLinuxInputEventHandler::openById(const struct input_id& id)
{
    return internalOpen(EVIOCGID, 0, QByteArray(), &id);
}

bool FicLinuxInputEventHandler::internalOpen(unsigned request, int length, const QByteArray& match, struct input_id const *matchId)
{
    if (m_fd >= 0) {
        ::close(m_fd);
        delete m_notifier;
        m_notifier = 0;
        m_fd = -1;
    }

    const bool cgidRequest = request == EVIOCGID;
    QByteArray deviceData(length, 0);
    struct input_id deviceId;

    // Find a suitable device, might want to add caching
    QDir dir(QLatin1String("/dev/input/"), QLatin1String("event*"));
    foreach(QFileInfo fileInfo, dir.entryInfoList(QDir::Files|QDir::System)) {
        m_fd = ::open(QFile::encodeName(fileInfo.filePath()), O_RDONLY|O_NDELAY);
        if (m_fd < 0)
            continue;

        int ret = cgidRequest ?
                    ioctl(m_fd, request, &deviceId) :
                    ioctl(m_fd, request, deviceData.data());

        if (ret < 0) {
            close(m_fd);
            continue;
        }

        // match the string we got with what we wanted
        if (cgidRequest && *matchId == deviceId) {
            break;
        } else if (!cgidRequest && strcmp(deviceData.constData(), match.constData()) == 0) {
            break;
        } else {
            close(m_fd);
            m_fd = -1;
        }
    }

    if (m_fd >= 0) {
        m_notifier = new QSocketNotifier(m_fd, QSocketNotifier::Read, this);
        connect(m_notifier, SIGNAL(activated(int)), this, SLOT(readData()));
    }

    return m_fd >= 0;
}

void FicLinuxInputEventHandler::readData()
{
    struct input_event event;

    int n = read(m_fd, &event, sizeof(struct input_event));
    if(n != (int)sizeof(struct input_event))
        return;

    emit inputEvent(event);
}

NeoKbdHandler::NeoKbdHandler()

{
    bool ok;

    qLog(Input) << "Loaded Neo keypad plugin";
    setObjectName( "Neo Keypad Handler" );

    auxHandler = new FicLinuxInputEventHandler(this);
    ok = auxHandler->openByName("gpio-keys");

    if(ok) {
        connect(auxHandler, SIGNAL(inputEvent(struct input_event&)),
                SLOT(inputEvent(struct input_event&)));
    } else {
        qWarning("Cannot open device for AUX button");
        delete auxHandler;
        auxHandler = 0;
    }

    powerHandler = new FicLinuxInputEventHandler(this);
    ok = powerHandler->openByName("twl4030_pwrbutton");

    if (ok) {
        connect(powerHandler, SIGNAL(inputEvent(struct input_event&)),
                SLOT(inputEvent(struct input_event&)));
    } else {
        qWarning("Cannot open device for POWER button");
        delete powerHandler;
        powerHandler = 0;
    }

    jackHandler = new FicLinuxInputEventHandler(this);
    ok = jackHandler->openByName("gta04 Headset Jack");

    if (ok) {
        connect(jackHandler, SIGNAL(inputEvent(struct input_event&)),
                SLOT(inputEvent(struct input_event&)));
    } else {
        qWarning("Cannot open input device for Headset Jack");
        delete jackHandler;
        jackHandler = 0;
    }
}

NeoKbdHandler::~NeoKbdHandler()
{
}

void NeoKbdHandler::inputEvent(struct input_event& event)
{
    if(event.type ==  EV_SYN/* || event.type ==  EV_SW */)
        return;

    bool isPress = (event.value);
    int unicode = 0xffff;
    int qtKeyCode = 0;

    qLog(Input) << "keypressed: type=" << event.type
                   << ", code=" << event.code
                   << ", value=" << event.value;

    switch(event.code) {
    case 0xA9:
        qtKeyCode = Qt::Key_F7;
        break;

    case 0x74: //116
        qtKeyCode = Qt::Key_Hangup;
        break;

    case SW_HEADPHONE_INSERT: //x02
    {
        QtopiaIpcEnvelope e("QPE/NeoHardware", "headphonesInserted(bool)");
        e <<  isPress;
    }
    break;
    case KEY_POWER2:
    {
        QtopiaIpcEnvelope e2("QPE/NeoHardware", "cableConnected(bool)");
        e2 <<  ((isPress)!=0);
    }
    break;
    default:
        break;
    };

    processKeyEvent(unicode, qtKeyCode, Qt::NoModifier, isPress, false);

}



#endif // QT_QWS_GTA04
