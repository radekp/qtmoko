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

// Write string count bytes long to file
static int write_file(const char *path, const char *value, size_t count)
{
    int fd = open(path, O_WRONLY);
    if (fd < 0)
        return - 1;
    int res = write(fd, value, count);
    close(fd);
    return res;
}


QTOPIABASE_EXPORT void qpe_setBrightness(int b)
{
    char str[8];
    int n = sprintf(str, "%d", b);
    for(int i = 10; i >= 0; i--) {
        if(write_file("/sys/class/backlight/pwm-backlight/brightness", str, n) == n)
            return;
        perror("qpe_setBrightness failed");
    }
}
