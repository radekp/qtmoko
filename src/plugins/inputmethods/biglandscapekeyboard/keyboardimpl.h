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
#ifndef KEYBOARDIMPL_H
#define KEYBOARDIMPL_H

#include <inputmethodinterface.h>
#include <keyboard.h>

class KeyboardFrame;

class KeyboardInputMethod : public QtopiaInputMethod
{
    Q_OBJECT
public:
    KeyboardInputMethod(QObject * =0);
    ~KeyboardInputMethod();

    QString name() const;
    QString identifier() const;
    QString version() const;

    State state() const;
    int properties() const { return RequireMouse | InputModifier | MenuItem; }
    QIcon icon() const;

    void setHint(const QString &, bool );

    // state managment.
    void reset();

    QWidget *inputWidget( QWidget *parent );
    QWSInputMethod *inputModifier( );

    void menuActionActivated(int v);
    QList<QIMActionDescription*> menuDescription();
private slots:
    void sendKeyPress( ushort unicode, ushort keycode, ushort modifiers, bool press, bool repeat );
private:
    Keyboard *input;
    void queryResponse ( int property, const QVariant & result );
};

#endif
