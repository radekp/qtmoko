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

#include <QWSServer>
#include <qapplication.h>
#include <qpixmap.h>
#include "keyboard.h"
#include "keyboardimpl.h"
#include <qtopialog.h>
#include <QVariant>

/* XPM */
static const char * kb_xpm[] = {
"28 13 4 1",
"       c None",
".      c #4C4C4C",
"+      c #FFF7DD",
"@      c #D6CFBA",
" .......................... ",
" .+++.+++.+++.+++.+++.++++. ",
" .+@@.+@@.+@@.+@@.+@@.+@@@. ",
" .......................... ",
" .+++++.+++.+++.+++.++++++. ",
" .+@@@@.+@@.+@@.+@@.+@@@@@. ",
" .......................... ",
" .++++++.+++.+++.+++.+++++. ",
" .+@@@@@.+@@.+@@.+@@.+@@@@. ",
" .......................... ",
" .++++.++++++++++++++.++++. ",
" .+@@@.+@@@@@@@@@@@@@.+@@@. ",
" .......................... "};

KeyboardInputMethod::KeyboardInputMethod(QObject *parent)
    : QtopiaInputMethod(parent), input(0)
{
}

KeyboardInputMethod::~KeyboardInputMethod()
{
    delete input;
}

QWidget *KeyboardInputMethod::inputWidget( QWidget *parent )
{
    if ( !input ) {
        input = new Keyboard( parent );
        connect(input->frame(), SIGNAL(showing()), this, SIGNAL(stateChanged()));
        connect(input->frame(), SIGNAL(hiding()), this, SIGNAL(stateChanged()));
    }
    return input->frame();
}

void KeyboardInputMethod::reset()
{
    if ( input )
        input->resetState();
}

QIcon KeyboardInputMethod::icon() const
{
    QIcon i;
    i.addPixmap(QPixmap((const char **)kb_xpm));
    return i;
}

QString KeyboardInputMethod::name() const
{
    return qApp->translate( "InputMethods", "LandscapeKeyboard" );
//    return qApp->translate( "InputMethods", "Opti" );
}

QString KeyboardInputMethod::identifier() const
{
    return "LandscapeKeyboard";
}

QString KeyboardInputMethod::version() const
{
    return "4.0.0";
}

QtopiaInputMethod::State KeyboardInputMethod::state() const
{
    if (input && input->frame()->isVisible())
        return Ready;
  else
    return Sleeping;
}
void KeyboardInputMethod::sendKeyPress(ushort k, ushort u, ushort, bool p, bool a)
{
    QWSServer::sendKeyEvent(k, u, 0, p, a);
}

QWSInputMethod *KeyboardInputMethod::inputModifier( )
{
    if ( !input ) {
        qLog(Input) << "KeyboardInPutMethod::inputModifier";
        input = new Keyboard();
        connect (input, SIGNAL(stateChanged()), this, SIGNAL(stateChanged()));
    }
    return input;
}

void KeyboardInputMethod::menuActionActivated(int v)
{
    input->menuActionActivated(v);
};

void KeyboardInputMethod::setHint(const QString &hint, bool)
{
    if(input && hint.isEmpty() && input->frame())
    {
        input->resetState();
    }
    if (input)
        input->checkMicroFocus();
}

QList<QIMActionDescription*> KeyboardInputMethod::menuDescription()
{
    QList<QIMActionDescription*> descriptionList;
    QIMActionDescription* keyboardActionDescription = new QIMActionDescription;
    // First item of more than one is the menu item, and will never be called
    keyboardActionDescription->setId(Keyboard::RootItem);
    keyboardActionDescription->setLabel(tr("Keyboard"));
    keyboardActionDescription->setIconFileName(QString(":icon/keyboard"));
    descriptionList.append(keyboardActionDescription);

    if(input->frame()->isVisible()){
        keyboardActionDescription = new QIMActionDescription;        
        keyboardActionDescription->setId(Keyboard::HideKeyboard);
        keyboardActionDescription->setLabel(("Hide Keyboard"));
        keyboardActionDescription->setIconFileName(QString(":icon/stop"));
        descriptionList.append(keyboardActionDescription);
    } else {
        keyboardActionDescription = new QIMActionDescription(Keyboard::ShowKeyboard, tr("Show Keyboard"), QString(":icon/keyboard"));
        descriptionList.append(keyboardActionDescription);
    };

    return descriptionList;
};


QTOPIA_EXPORT_PLUGIN(KeyboardInputMethod);
