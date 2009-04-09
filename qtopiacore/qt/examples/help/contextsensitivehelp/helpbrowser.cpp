/****************************************************************************
**
** Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies).
** Contact: Qt Software Information (qt-info@nokia.com)
**
** This file is part of the example classes of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial Usage
** Licensees holding valid Qt Commercial licenses may use this file in
** accordance with the Qt Commercial License Agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Nokia.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain
** additional rights. These rights are described in the Nokia Qt LGPL
** Exception version 1.0, included in the file LGPL_EXCEPTION.txt in this
** package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
** If you are unsure which license is appropriate for your use, please
** contact the sales department at qt-sales@nokia.com.
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtCore/QLibraryInfo>
#include <QtGui/QApplication>
#include <QtHelp/QHelpEngineCore>

#include "helpbrowser.h"

HelpBrowser::HelpBrowser(QWidget *parent)
    : QTextBrowser(parent)
{
    QString collectionFile = QLibraryInfo::location(QLibraryInfo::ExamplesPath)
        + QLatin1String("/help/contextsensitivehelp/doc/wateringmachine.qhc");

    m_helpEngine = new QHelpEngineCore(collectionFile, this);
    if (!m_helpEngine->setupData()) {
        delete m_helpEngine;
        m_helpEngine = 0;
    }
}

void HelpBrowser::showHelpForKeyword(const QString &id)
{
    if (m_helpEngine) {
        QMap<QString, QUrl> links = m_helpEngine->linksForIdentifier(id);
        if (links.count())
            setSource(links.constBegin().value());
    }
}

QVariant HelpBrowser::loadResource(int type, const QUrl &name)
{
    QByteArray ba;
    if (type < 4 && m_helpEngine) {
        QUrl url(name);
        if (name.isRelative())            
            url = source().resolved(url);
        ba = m_helpEngine->fileData(url);        
    }
    return ba;
}

