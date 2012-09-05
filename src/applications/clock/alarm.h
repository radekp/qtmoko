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
#ifndef ALARM_H
#define ALARM_H

#include "ui_alarmbase.h"
#include <qdatetime.h>
#include <QHash>

class QLabel;
class QDialog;
class QEvent;
class AlarmDialog;
namespace clockalarmringcontrol {
class RingControl;
};
using namespace clockalarmringcontrol;

class Alarm : public QWidget, Ui::AlarmBase
{
    Q_OBJECT
public:
    Alarm( QWidget *parent=0, Qt::WFlags fl=0 );
    ~Alarm();

    void triggerAlarm(const QDateTime &when, int type);
    bool eventFilter(QObject *o, QEvent *e);
public slots:
    void setDailyEnabled(bool);

private slots:
    void changeClock( bool );
    void applyDailyAlarm();
    void changeAlarmDays();

    void applySnooze(int);
    void setSnooze();
    void selectTone();

protected:
    QDateTime nextAlarm( int h, int m );
    QString getAlarmDaysText() const;
    void resetAlarmDaysText();

private:
    RingControl *alarmt;
    bool ampm;
    bool weekStartsMonday;
    bool initEnabled;
    AlarmDialog* alarmDlg;
    QLabel* alarmDlgLabel;
    bool init;
    int snooze;
    void setAlarm(QDateTime when, bool add);
    QHash<int, bool> daysSettings;
};

#endif

