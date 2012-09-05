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
#include "alarm.h"
#include "ringcontrol.h"

#include <qtopiaapplication.h>
#include <qtopiaipcenvelope.h>
#include <qsettings.h>
#include <qtimestring.h>
#include <qtopianamespace.h>
#include <qlabel.h>
#include <qcheckbox.h>
#include <qlayout.h>
#include <qtimer.h>
#include <QDateTimeEdit>
#include <QSoftMenuBar>
#include <QKeyEvent>
#include <QDocumentSelectorDialog>

#ifdef Q_WS_X11
#include <qcopchannel_x11.h>
#else
#include <qcopchannel_qws.h>
#endif

static const int magic_daily = 2292922;     //type for daily alarm

// =====================================================================

class AlarmDialog : public QDialog
{
    Q_OBJECT
public:
    AlarmDialog(int snooze, QWidget *parent)
        : QDialog(parent)
    {
        setWindowTitle(tr("Clock"));
        QSoftMenuBar::setLabel( this, Qt::Key_Back, "back",  tr("Dismiss"));
        setSnooze(snooze);
    }

    ~AlarmDialog() {}

    void setSnooze(int snooze)
    {
        canSnooze = (snooze > 0);
        if (canSnooze)
            QSoftMenuBar::setLabel(this, Qt::Key_Select, "select",  tr("Snooze"));
        else
            QSoftMenuBar::setLabel(this, Qt::Key_Select, QSoftMenuBar::NoLabel);
    }

    QWidget *snoozer;

signals:
    void snooze();

private:
    void keyPressEvent(QKeyEvent *e)
    {
        if (canSnooze && e->key() == Qt::Key_Select) {
            e->accept();
            emit snooze();
            accept();
            return;
        }
        e->ignore();
    }

    void mousePressEvent(QMouseEvent *e)
    {
        if (canSnooze) {
            // Clicking on the bell also snoozes
            if (snoozer->rect().contains(snoozer->mapFromGlobal(e->globalPos()))) {
                e->accept();
                emit snooze();
                accept();
            }
        }
    }

    bool canSnooze;
};

// =====================================================================

Alarm::Alarm( QWidget * parent, Qt::WFlags f )
    : QWidget( parent, f ), init(false) // No tr
{
    setupUi(this);
    alarmDlg = 0;
    alarmDlgLabel = 0;

    ampm = QTimeString::currentAMPM();
    weekStartsMonday = Qtopia::weekStartsOnMonday();

    alarmt = new RingControl( this );

    connect( qApp, SIGNAL(timeChanged()), SLOT(applyDailyAlarm()) );
    connect( qApp, SIGNAL(clockChanged(bool)), this, SLOT(changeClock(bool)) );

    connect( alarmEnabled, SIGNAL(toggled(bool)), this, SLOT(setDailyEnabled(bool)) );
    connect( alarmDaysEdit, SIGNAL(clicked()), this, SLOT(changeAlarmDays()) );
    QSoftMenuBar::setLabel(alarmDaysEdit, Qt::Key_Select, "select", tr("Change"));

    QSettings cConfig("Trolltech","Clock"); // No tr
    cConfig.beginGroup( "Daily Alarm" );

    QStringList exclDays = cConfig.value( "ExcludeDays").toStringList();
    for (int i=Qt::Monday; i<=Qt::Sunday; i++) {
        daysSettings.insert(i, !exclDays.contains(QString::number(i)));
    }
    resetAlarmDaysText();

    initEnabled = cConfig.value("Enabled", false).toBool();
    alarmEnabled->setChecked( initEnabled );
    int m = cConfig.value( "Minute", 0 ).toInt();
    int h = cConfig.value( "Hour", 7 ).toInt();
    snooze = cConfig.value( "Snooze", 0 ).toInt();
    
    cConfig.endGroup();
    cConfig.beginGroup( "Ringtone" );
    toneButton->setText(cConfig.value("Name", "alarm.wav").toString());
    QVariant fileName = cConfig.value("FileName");
    if(fileName.isValid())
        alarmt->setSound(fileName.toString());

    if (ampm)
        alarmTimeEdit->setDisplayFormat("h:mm ap");
    else
        alarmTimeEdit->setDisplayFormat("hh:mm");
    alarmTimeEdit->setTime( QTime( h, m ) );

    snoozeTimeSpinner->setValue(snooze);

    connect( alarmTimeEdit, SIGNAL(editingFinished()), this, SLOT(applyDailyAlarm())) ;
    connect(snoozeTimeSpinner, SIGNAL(valueChanged(int)), this, SLOT(applySnooze(int)));
    connect(toneButton, SIGNAL(clicked()), this, SLOT(selectTone()));

    alarmDaysEdit->installEventFilter(this);

    init = true;
}

Alarm::~Alarm()
{
}

void Alarm::changeClock( bool a )
{
    //change display format (whether or not we want am/pm)
    if ( ampm != a ) {
        ampm = a;
        if (ampm)
            alarmTimeEdit->setDisplayFormat("h:mm ap");
        else
            alarmTimeEdit->setDisplayFormat("hh:mm");
    }
}

void Alarm::triggerAlarm(const QDateTime &when, int type)
{
    QTime theTime( when.time() );
    if ( type == magic_daily ) {
        // Make sure the screen comes on
        QtopiaApplication::setPowerConstraint(QtopiaApplication::DisableLightOff);
        QString ts = QTimeString::localHM(theTime);
        QString msg = ts + "\n" + tr( "(Daily Alarm)" );
        alarmt->setVibrateStartDelay(60);
        alarmt->setRepeat(120);
        QTimer::singleShot(5000, alarmt, SLOT(start()));
        if ( !alarmDlg ) {
            alarmDlg = new AlarmDialog(snooze, this);
            connect(alarmDlg, SIGNAL(snooze()), this, SLOT(setSnooze()));
            QVBoxLayout *vb = new QVBoxLayout(alarmDlg);
            vb->setMargin(6);
            vb->addStretch(1);
            QWidget *w = new QWidget;
            {
                QHBoxLayout *hb = new QHBoxLayout(w);
                QFrame *frame = new QFrame;
                hb->addStretch(1);
                hb->addWidget(frame);
                hb->addStretch(1);
                alarmDlg->snoozer = frame;
                frame->setFrameStyle(QFrame::Box);
                QVBoxLayout *vb = new QVBoxLayout( frame );
                QLabel *l = new QLabel( alarmDlg );
                QIcon icon(":icon/alarmbell");
                int height = QFontMetrics(QFont()).height();
                QPixmap pm = icon.pixmap(icon.actualSize(QSize(height * 5, height * 5)));
                l->setPixmap(pm);
                l->setAlignment( Qt::AlignCenter );
                vb->addWidget(l);
                l = new QLabel;
                l->setText( tr("Snooze") );
                l->setAlignment( Qt::AlignCenter );
                vb->addWidget(l);
            }
            vb->addWidget(w);
            alarmDlgLabel = new QLabel( msg, alarmDlg );
            alarmDlgLabel->setAlignment( Qt::AlignCenter );
            vb->addWidget(alarmDlgLabel);
            vb->addStretch(1);
        } else {
            alarmDlg->setSnooze(snooze);
            alarmDlgLabel->setText(msg);
        }
        // Set for tomorrow, so user wakes up every day, even if they
        // don't confirm the dialog.
        applyDailyAlarm();
        if ( !alarmDlg->isVisible() ) {
            alarmDlg->showMaximized();
            alarmDlg->exec();
            alarmt->stop();
            QtopiaApplication::setPowerConstraint(QtopiaApplication::Enable);
        }
    }
}

void Alarm::setDailyEnabled(bool enableDaily)
{
    alarmEnabled->setChecked( enableDaily );
    applyDailyAlarm();
}

QDateTime Alarm::nextAlarm( int h, int m )
{
    QDateTime now = QDateTime::currentDateTime();
    QTime at( h, m );
    QDateTime when( now.date(), at );
    int count = 0;
    int dow = when.date().dayOfWeek();

    while ( when < now || daysSettings[dow] == false ) {
        when = when.addDays( 1 );
        dow = when.date().dayOfWeek();
        if ( ++count > 7 )
            return QDateTime();
    }

    return when;
}

void Alarm::applyDailyAlarm()
{
    if ( !init )
        return;

    int minute = alarmTimeEdit->time().minute();
    int hour = alarmTimeEdit->time().hour();

    QSettings config("Trolltech","Clock");
    config.beginGroup( "Daily Alarm" );
    config.setValue( "Hour", hour );
    config.setValue( "Minute", minute );

    bool enableDaily = alarmEnabled->isChecked();
    config.setValue( "Enabled", enableDaily );

    QStringList exclDays;
    for (int i=1; i<=7; i++) {
        if ( !daysSettings.value(i, false) ) {
            exclDays << QString::number( i );
        }
    }
    config.setValue( "ExcludeDays", exclDays );
    config.sync();

    if (enableDaily != initEnabled) {
        QtopiaIpcEnvelope e("QPE/AlarmServer", "dailyAlarmEnabled(bool)");
        e << enableDaily;
        initEnabled = enableDaily;
    }

    bool addAlarm = false;
    if ( alarmEnabled->isChecked() && exclDays.size() < 7 ) {
        addAlarm = true;
    }
    setAlarm(nextAlarm(hour, minute), addAlarm);
}

bool Alarm::eventFilter(QObject *o, QEvent *e)
{
    if (o == alarmDaysEdit) {
        if (e->type() == QEvent::MouseButtonRelease) {
            changeAlarmDays();
            return true;
        } else if (e->type() == QEvent::Resize) {
            resetAlarmDaysText();
        }
    }
    return false;
}

QString Alarm::getAlarmDaysText() const
{
    int day;

    QList<int> alarmDays;
    for (day=Qt::Monday; day<=Qt::Sunday; day++) {
        if (daysSettings[day])
            alarmDays << day;
    }

    int alarmDaysCount = alarmDays.size();
    if (alarmDaysCount == 7) {
        return tr("Every day");

    } else if (alarmDaysCount == 5 &&
                alarmDays[0] == Qt::Monday &&
                alarmDays.last() == Qt::Friday) {

        return tr("Weekdays");

    } else if (alarmDaysCount == 2 &&
                alarmDays[0] == Qt::Saturday &&
                alarmDays[1] == Qt::Sunday) {

        return tr("Weekends");

    } else {
        if (alarmDaysCount == 0)
            return QLatin1String("");

        QStringList dayStrings;
        if (alarmDaysCount == 1) {
            return QTimeString::nameOfWeekDay(alarmDays[0],
                        QTimeString::Long);
        } else {
            for (day=0; day<alarmDaysCount; day++) {
                dayStrings << QTimeString::nameOfWeekDay(alarmDays[day],
                        QTimeString::Medium);
            }
            // move Sunday to front if necessary
            if (!weekStartsMonday && alarmDays.last() == Qt::Sunday)
                dayStrings.insert(0, dayStrings.takeLast());
            return dayStrings.join(", ");
        }
    }
}

void Alarm::resetAlarmDaysText()
{
    QFontMetrics fm(alarmDaysEdit->font());
    alarmDaysEdit->setText(fm.elidedText(getAlarmDaysText(), Qt::ElideRight, alarmDaysEdit->width()));
    alarmDaysEdit->home(false);
}

void Alarm::changeAlarmDays()
{
    int day;
    QDialog dlg;
    QVBoxLayout layout;

    QHash<int, QCheckBox *> checkboxes;
    QCheckBox *c;
    for (day=Qt::Monday; day<=Qt::Sunday; day++) {
        c = new QCheckBox(QTimeString::nameOfWeekDay(day, QTimeString::Long));
        c->setChecked(daysSettings[day]);
        checkboxes.insert(day, c);

        if (day == Qt::Sunday && !weekStartsMonday)
            layout.insertWidget(0, c);
        else
            layout.addWidget(c);
    }

    layout.setMargin(6);
    dlg.setLayout(&layout);
    dlg.setWindowTitle(tr("Set alarm days"));

    if (QtopiaApplication::execDialog(&dlg) == QDialog::Accepted) {
        bool foundChecked = false;
        for (day=Qt::Monday; day<=Qt::Sunday; day++) {
            if (checkboxes[day]->isChecked()) {
                foundChecked = true;
                break;
            }
        }

        // don't change previous alarm daysSettings if nothing has been checked
        // and the alarm is going to be disabled
        if (foundChecked) {
            for (day=Qt::Monday; day<=Qt::Sunday; day++)
                daysSettings[day] = checkboxes[day]->isChecked();
        }

        // if no daysSettings have been set, disable the alarm
        setDailyEnabled(foundChecked);
        resetAlarmDaysText();
    }
}

void Alarm::setSnooze()
{
    if (snooze > 0) {
        //setAlarm(QDateTime::currentDateTime().addSecs(snooze*60), true);
	Qtopia::addAlarm(QDateTime::currentDateTime().addSecs(snooze*60),
                         "QPE/Application/clock",
                         "alarm(QDateTime,int)", magic_daily);
    }
}

void Alarm::applySnooze(int time)
{
    snooze = time;
    QSettings config("Trolltech","Clock");
    config.beginGroup( "Daily Alarm" );
    config.setValue( "Snooze", snooze );
}

void Alarm::setAlarm(QDateTime when, bool addAlarm)
{
    Qtopia::deleteAlarm(QDateTime(), "QPE/Application/clock",
                        "alarm(QDateTime,int)", magic_daily);
    if (addAlarm) {
        Qtopia::addAlarm(when, "QPE/Application/clock",
                         "alarm(QDateTime,int)", magic_daily);
    }
}

void Alarm::selectTone()
{
    QString dlgcaption = tr("Select Ringtone");

    QDocumentSelectorDialog *dlg = new QDocumentSelectorDialog(this);
    dlg->setModal(true);
    dlg->setWindowTitle(dlgcaption);
    dlg->setWindowState( dlg->windowState() | Qt::WindowMaximized );
    dlg->setSelectPermission( QDrmRights::Play );
    dlg->setMandatoryPermissions( QDrmRights::Play | QDrmRights::Automated );

    QContentFilter audiofilter(QContent::Document);
    audiofilter &= QContentFilter( QContentFilter::MimeType, QLatin1String( "audio/x-wav" ) );
    audiofilter |= QContentFilter( QContentFilter::MimeType, QLatin1String( "audio/ogg" ) );

    dlg->setFilter( audiofilter );
    dlg->disableOptions( QDocumentSelector::ContextMenu );

    if (!QtopiaApplication::execDialog(dlg))
        return;

    QContent content = dlg->selectedDocument();
    toneButton->setText(content.name());
    alarmt->setSound(content.fileName());
    
    QSettings config("Trolltech","Clock");
    config.beginGroup( "Ringtone" );
    config.setValue( "Name", content.name() );
    config.setValue( "FileName", content.fileName() );
    config.sync();
}

#include "alarm.moc"
