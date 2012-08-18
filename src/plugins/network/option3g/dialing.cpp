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
#include <qsoftmenubar.h>
#include <ipvalidator.h>


DialingPage::DialingPage( const QtopiaNetworkProperties prop, QWidget* parent, Qt::WFlags flags)
    : QWidget(parent, flags)
{
    ui.setupUi( this );
    init();
    readConfig( prop );

    QSoftMenuBar::menuFor( this );
    QSoftMenuBar::setHelpEnabled( this , true );
    setObjectName("dialing");
}

DialingPage::~DialingPage()
{
}

void DialingPage::init()
{
    IPValidator * val = new IPValidator( this );
    ui.dns1->setValidator( val );
    ui.dns2->setValidator( val );

    QtopiaApplication::setInputMethodHint(ui.dns1, "netmask");
    QtopiaApplication::setInputMethodHint(ui.dns2, "netmask");

    connect( ui.usepeerdns, SIGNAL(stateChanged(int)),
           this, SLOT(manualDNS(int)));
}

void DialingPage::readConfig( const QtopiaNetworkProperties& prop )
{
    QVariant v = prop.value("Serial/Timeout");
    if (!v.isValid()) {
        ui.timeout->setValue(120);
    } else if (v.toString() == "none") {
        ui.timeout->setValue( 0 );
    } else if (v.canConvert(QVariant::Int)) {
        ui.timeout->setValue( v.toInt() );
    }

    v = prop.value("Serial/UsePeerDNS");
    if ( v.toString() != "n" )
        ui.usepeerdns->setCheckState(Qt::Checked);
    else
        ui.usepeerdns->setCheckState(Qt::Unchecked);
    manualDNS( ui.usepeerdns->checkState() );

    ui.dns1->setText( prop.value("Properties/DNS_1").toString() );
    ui.dns2->setText( prop.value("Properties/DNS_2").toString() );

}

QtopiaNetworkProperties DialingPage::properties()
{
    QtopiaNetworkProperties props;
    if (ui.timeout->value() == 0)
        props.insert("Serial/Timeout", QString("none"));
    else
        props.insert("Serial/Timeout", ui.timeout->value());


    props.insert("Serial/UsePeerDNS",
        ui.usepeerdns->checkState()==Qt::Checked ? QString("y"): QString("n"));
    props.insert("Properties/DNS_1", ui.dns1->text());
    props.insert("Properties/DNS_2", ui.dns2->text());

    return props;
}

void DialingPage::manualDNS(int state) {
    if ( state == Qt::Checked )
        ui.dns_box->setEnabled( false );
    else {
        ui.dns_box->setEnabled( true );
    }
}
