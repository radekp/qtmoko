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

#include <QVariant>
#include <qwindowsystem_qws.h>

#include <qtopialog.h>
#include "dockedkeyboard.h"

DockedKeyboard::DockedKeyboard(QWidget* parent, Qt::WFlags f) : Keyboard(parent, f)
{
    // The keyboard frame is meaningless after the DockedKeyboard IM is destroyed,
    // so keep control of it by never parenting DockedKeyboardFrame;
    // This should also help keep the keyboard on top of other widgets.
    Q_UNUSED(parent);
    Q_ASSERT(keyboardFrame);
    qLog(Input) << "DockedKeyboard Instantiated.";
};

void DockedKeyboard::queryResponse ( int , const QVariant & )
{
};


DockedKeyboard::~DockedKeyboard()
{
    qLog(Input) << "DockedKeyboard Destructing";
    if(keyboardFrame){
    }
};

QWidget* DockedKeyboard::frame(){
    return keyboardFrame;
};

void DockedKeyboard::resetState()
{
    qLog(Input) << "DockedKeyboard::resetState()";
    Keyboard::resetState();
};
