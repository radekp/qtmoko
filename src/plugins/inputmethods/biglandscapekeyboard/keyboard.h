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
#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <qwindowsystem_qws.h>
#include "keyboardframe.h"
#include <qtopiaipcenvelope.h>

/*
    Keyboard is an input method for Qtopia.  Keyboard displays a popup widget depiciting keys onscreen (KeyboardFrame), and converts them into key events.

    Currently, KeyboardFrame handles both the mouseEvents and creating the keyEvents, but it is anticipated that output will be re-routed through the Keyboard class in future, most likely in the form of IMEvents instead.

    \enum Keyboard::MenuActivationCode

    This enum describes the value for the different SoftMenu items that the Keyboard input method provides.

    \value RootItem The top "Keyboard" menu item
    \value SwapPosition The command to swap keyboard positions.
                        No longer supported
    \value ShowKeyboard Shows the keyboard when it is hidden
    \value HideKeyboard Hides the keyboard when it is visible
    \value DockKeyboard Docks the keyboard to the bottom of the screen,
            reducing the available area for other applications.
            Not yet supported.
    \value UnDockKeyboard Un-docks the keyboard to the bottom of the screen,
            increasing the available area for other applications.
            Not yet supported.

*/
class QAction;

class Keyboard : public QWSInputMethod
{
    Q_OBJECT
public:
    enum MenuActivationCode {
        Unknown = 0,
        RootItem = 1,
        SwapPosition = 2,
        ShowKeyboard,
        HideKeyboard,
        DockKeyboard,
        UnDockKeyboard
    };

    Keyboard( QWidget* parent=0, Qt::WFlags f=0 );
    virtual ~Keyboard();
    void queryResponse ( int property, const QVariant & result );

    QWidget* frame();
    void resetState();
    void menuActionActivated(int v);

signals:
    void stateChanged();

public slots:
    void checkMicroFocus();
private slots:
    void swapPosition(bool);
protected:
    virtual void updateHandler(int type);
    KeyboardFrame *keyboardFrame;
    int microX;
    int microY;
};

#endif
