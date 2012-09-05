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

#include "versioninfo.h"

#include <qtopianamespace.h>
#include <qthumbnail.h>
#include <QLabel>
#include <QFile>
#include <QTextStream>
#include <QLayout>
#include <QApplication>
#include <QDesktopWidget>

// For releases, this defines get replaced with the appropriate data by the packaging scripts
#define QT_EXTENDED_COPYRIGHT_YEAR	2012
#define QT_EXTENDED_COPYRIGHT_COMPANY "Trolltech ASA & QtMoko authors"

VersionInfo::VersionInfo( QWidget *parent, Qt::WFlags f )
    : QWidget( parent, f )
{
    init();
}

VersionInfo::~VersionInfo()
{
}

void VersionInfo::init()
{
    setMinimumSize(QSize(16,400));
    QFont boldFont = this->font();
    boldFont.setBold(true);

    QDesktopWidget *desktop = QApplication::desktop();
    double imageScale = ((double)desktop->availableGeometry(desktop->screenNumber(this)).width())/400.0;
    if (imageScale > 1.0)
        imageScale = 1.0;
    int imageSize = (int)(55 * imageScale);
    int finalSize = (int)(60 * imageScale);

    QVBoxLayout *vBoxLayout = new QVBoxLayout(this);
    vBoxLayout->setMargin( 0 );
    vBoxLayout->setSpacing( 0 );

    QGridLayout *gridLayout1 = new QGridLayout;
    gridLayout1->setSpacing( 4 );
    gridLayout1->setMargin( 0 );
    gridLayout1->setColumnMinimumWidth(0, 20 );

    QLabel *qtopiaLogo = new QLabel(this);
    QThumbnail thumbnail(":image/qpe-logo");
    qtopiaLogo->setPixmap(thumbnail.pixmap(QSize(imageSize, imageSize)));
    qtopiaLogo->setFixedSize( finalSize, finalSize );
    gridLayout1->addWidget(qtopiaLogo, 0, 0, 1, 1);

    QVBoxLayout *vBoxLayout1 = new QVBoxLayout;
    vBoxLayout1->setMargin( 0 );
    vBoxLayout1->setSpacing( 3 );
    QLabel *qtopiaName = new QLabel(this);
    qtopiaName->setFont(boldFont);
    qtopiaName->setText(tr("QtMoko"));
    vBoxLayout1->addWidget(qtopiaName);

    QLabel *qtopiaVersion = new QLabel(this);
    qtopiaVersion->setWordWrap(true);
    qtopiaVersion->setText(tr("Version:") + ' ' + Qtopia::version() + '\n' + tr("Qt version:") + ' ' + QT_VERSION_STR);
    vBoxLayout1->addWidget(qtopiaVersion);
    vBoxLayout1->addSpacing( 10 );

    QLabel *qtopiaCopyright = new QLabel(this);
    qtopiaCopyright->setText(tr( "Copyright \251 %1", "%1 = 'year'" ).arg(QT_EXTENDED_COPYRIGHT_YEAR));
    vBoxLayout1->addWidget(qtopiaCopyright);
    QLabel* qtopiaCopyright1 = new QLabel( this );
    qtopiaCopyright1->setWordWrap( true );
    qtopiaCopyright1->setText( QT_EXTENDED_COPYRIGHT_COMPANY );
    vBoxLayout1->addWidget(qtopiaCopyright1);

    vBoxLayout1->addSpacing( 10 );

    QLabel *qtopiaBuild = new QLabel(this);
    qtopiaBuild->setWordWrap( true );
    QString builder( BUILDER );
    int atIndex = builder.indexOf( QChar('@') );
    int dotIndex = -1;
    if ( atIndex >= 0 )
        dotIndex = builder.indexOf( QChar('.'), atIndex );
    if ( dotIndex >= 0 )
        builder = builder.left( dotIndex );
    qtopiaBuild->setText(tr("Built by\n%1", "%1 = name").arg(builder));
    vBoxLayout1->addWidget(qtopiaBuild);

    qtopiaBuild = new QLabel(this);
    qtopiaBuild->setText(tr("Built on %1","1=date").arg(__DATE__));
    vBoxLayout1->addWidget(qtopiaBuild);

    QSpacerItem *spacerItem = new QSpacerItem(20, 10, QSizePolicy::Minimum, QSizePolicy::MinimumExpanding);
    vBoxLayout1->addItem(spacerItem);
    gridLayout1->addLayout(vBoxLayout1, 0, 1, 2, 1);
    vBoxLayout->addLayout(gridLayout1);

    QGridLayout *gridLayout2 = new QGridLayout;
    gridLayout2->setColumnMinimumWidth(0, 20 );
    gridLayout2->setSpacing( 4 );
    gridLayout2->setMargin( 0 );
    QLabel *linuxLogo = new QLabel(this);
    QThumbnail thumbnail2(":image/tux-logo");
    linuxLogo->setPixmap(thumbnail2.pixmap(QSize(imageSize, imageSize)));
    linuxLogo->setFixedSize( finalSize, finalSize );
    gridLayout2->addWidget(linuxLogo, 0, 0, 1, 1);

    QVBoxLayout *vBoxLayout2 = new QVBoxLayout;
    vBoxLayout2->setMargin( 0 );
    vBoxLayout2->setSpacing( 3 );
    QLabel *linuxName = new QLabel(this);
    linuxName->setFont(boldFont);
    linuxName->setText(tr("Linux Kernel"));
    vBoxLayout2->addWidget(linuxName);

    QString kernelVersionString;
    QString compiledByString;
    QFile file("/proc/version");
    if(file.open(QFile::ReadOnly))
    {
        QTextStream t( &file );
        QString v;
        t >> v; t >> v; t >> v;
        kernelVersionString = v.left( 22 );
        t >> v;
        compiledByString = v;
        file.close();
    }
    QLabel *linuxVersion = new QLabel(this);
    linuxVersion->setWordWrap(true);
    linuxVersion->setText(kernelVersionString);
    vBoxLayout2->addWidget(linuxVersion);

    QLabel *linuxCompiledBy = new QLabel(this);
    linuxCompiledBy->setWordWrap(true);
    linuxCompiledBy->setText(tr("Compiled by:") + ' ' + compiledByString);
    vBoxLayout2->addWidget(linuxCompiledBy);

    gridLayout2->addLayout(vBoxLayout2, 0, 1, 2, 1);
    vBoxLayout->addLayout(gridLayout2);
}
