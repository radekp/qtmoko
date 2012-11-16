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

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <custom.h>
#include <sys/stat.h> 
#include <qtopianamespace.h>
#include <QtopiaServiceRequest>
#include <QtopiaIpcEnvelope>
#include <qwindowsystem_qws.h>
#include <QValueSpaceItem>
#include <QValueSpaceObject>
#include <QDebug>

QTOPIABASE_EXPORT int qpe_sysBrightnessSteps()
{
    return 100;
}

QTOPIABASE_EXPORT void qpe_setBrightness(int b)
{
}
