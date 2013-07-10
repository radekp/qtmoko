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

#include <custom.h>
#include <qtopianamespace.h>
#include <QtopiaServiceRequest>
#include <QtopiaIpcEnvelope>

#include <qwindowsystem_qws.h>
#include <QValueSpaceItem>
#include <QValueSpaceObject>
#include <stdio.h>
#include <stdlib.h>
#include <QProcess>
#include <QFile>
#include <QFileInfo>

#include <QTextStream>
#include <QDebug>

QTOPIABASE_EXPORT int qpe_sysBrightnessSteps()
{
    QFile f("/sys/class/backlight/gta02-bl/max_brightness");
    if(!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "qpe_sysBrightnessSteps: " + f.errorString();
        return 0;
    }
    QTextStream in(&f);
    QString str;
    in >> str;
    f.close();
    return str.toInt();
}

QTOPIABASE_EXPORT void qpe_setBrightness(int b)
{
    int brightessSteps = qpe_sysBrightnessSteps();
    if(b > brightessSteps)
        b = brightessSteps;

    if(b == 1) {
        // dim
        b = brightessSteps / 4;
    } else if (b == -1) {
        //bright
        b = brightessSteps;
    }

    QFile f("/sys/class/backlight/gta02-bl/brightness");
    if(!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        qWarning() << "qpe_setBrightness: " + f.errorString();
	return;
    }
    QTextStream out(&f);
    out << QString::number(b);
    f.close();
}

