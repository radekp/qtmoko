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

#include "dialing.h"

#include <qtopiaapplication.h>

#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QCheckBox>
#include <QGroupBox>

DialingPage::DialingPage( const QtopiaNetworkProperties prop, QWidget* parent, Qt::WFlags flags)
    : QWidget(parent, flags)
{
    init();
    readConfig( prop );
}

DialingPage::~DialingPage()
{
}

void DialingPage::init()
{
}

void DialingPage::readConfig( const QtopiaNetworkProperties& prop )
{
}

QtopiaNetworkProperties DialingPage::properties()
{
    QtopiaNetworkProperties props;
    return props;
}
