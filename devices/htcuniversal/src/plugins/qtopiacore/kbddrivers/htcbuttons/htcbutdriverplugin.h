/****************************************************************************
**
** Copyright (C) 2000-2006 TROLLTECH ASA. All rights reserved.
**
** This file is part of the Phone Edition of the Qtopia Toolkit.
**
** This software is licensed under the terms of the GNU General Public
** License (GPL) version 2.
**
** See http://www.trolltech.com/gpl/ for GPL licensing information.
**
** Contact info@trolltech.com if any conditions of this licensing are
** not clear to you.
**
**
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
****************************************************************************/

#ifndef HTCBUTDRIVERPLUGIN_H
#define HTCBUTDRIVERPLUGIN_H

#include <QtGui/QWSKeyboardHandlerFactoryInterface>

class htcButDriverPlugin : public QKbdDriverPlugin {
    Q_OBJECT
public:
    htcButDriverPlugin( QObject *parent  = 0 );
    ~htcButDriverPlugin();

    QWSKeyboardHandler* create(const QString& driver, const QString& device);
    QWSKeyboardHandler* create(const QString& driver);
    QStringList keys()const;
};

#endif // PXA27XKBDDRIVERPLUGIN_H
