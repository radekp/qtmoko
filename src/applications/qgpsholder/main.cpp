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

#include <QDebug>
#include <QTimer>
#include <QProcess>
#include <QWhereabouts>
#include <QWhereaboutsFactory>

#include <QCoreApplication>

/*
    This program turns GPS on and launches application given in arguments.
    Once launched app exist it turns gps off and exits too.
*/


class QGpsHolder : public QObject
{
    Q_OBJECT
public:
    QGpsHolder(QObject *parent = 0)
        : QObject(parent)
        , ps(this)
    {
        QWhereabouts *whereabouts = QWhereaboutsFactory::create();
        connect(whereabouts, SIGNAL(updated(QWhereaboutsUpdate)),
                SLOT(updated(QWhereaboutsUpdate)));

        whereabouts->startUpdates();

        prevState = QProcess::NotRunning;
        QTimer::singleShot(10, this, SLOT(tick()));
    }

private:
    QProcess ps;
    QProcess::ProcessState prevState;
    
private slots:
    void tick()
    {
        QProcess::ProcessState state = ps.state();

        if(state == QProcess::Running ||
            state == QProcess::Starting)
        {
            // app starting or running
        }
        else if(QCoreApplication::arguments().count() <= 1)
        {
            // no args given, just hold gps
        }
        else if(state == QProcess::NotRunning && prevState == QProcess::NotRunning)
        {
            QStringList args;
            for(int i = 2; i < QCoreApplication::arguments().count(); i++)
            {
                args.append(QCoreApplication::arguments().at(i));
            }
            ps.start(QCoreApplication::arguments().at(1), args);
        }
        else
        {
            QCoreApplication::exit(0);
	    return;
        }
	prevState = state;
        QTimer::singleShot(5000, this, SLOT(tick()));
    }

    void updated(const QWhereaboutsUpdate & update)
    {
	Q_UNUSED(update);
    }
};

int main(int argc, char **argv)
{
    QCoreApplication a(argc, argv);
    QGpsHolder demo;
    return a.exec();
}

#include "main.moc"
