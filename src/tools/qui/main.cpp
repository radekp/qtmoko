/****************************************************************************
**
** This file is part of the Qt Extended Opensource Package.
**
** Copyright (C) 2012 Radek Polak <psonek2@seznam.cz>.
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

#include <stdio.h>
#ifdef QTOPIA
#include <QtopiaApplication>
#else
#include <QApplication>
#endif
#include <QMessageBox>

static int help()
{
    printf(
                "qui - show QtMoko dialogs from command line\n"
                "usage: qui <function> <arguments>\n"
                "examples:\n"
                "\n"
                "qui showmessage title text button1_text button2_text button3_text\n"
                "  Shows message box with given title and text. Buttons texts are\n"
                "  optional. Return index of pressed button.\n\n");

    return 0;
}

static int missingArg()
{
    printf("missing argument\n");
    return -1;
}

int main(int argc, char **argv)
{
#ifdef QTOPIA
    QtopiaApplication app(argc, argv);
    Q_UNUSED(app);
#else
    QApplication app(argc, argv);
    Q_UNUSED(app);
#endif

    if(argc <= 1)
        return help();

    if(strcmp(argv[1], "showmessage") == 0) {
        if(argc < 4)
            return missingArg();
        return QMessageBox::question(NULL, argv[2], argv[3]);
    }

    return help();
}

