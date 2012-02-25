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

#include "firstuse.h"


#include "../../../../settings/systemtime/settime.h"
#include "../../../../settings/language/languagesettings.h"

#include <Qtopia>

#include <QLabel>
#include <QPushButton>
#include <QPainter>
#include <QTimer>
#include <QDesktopWidget>
#include <QEvent>
#include <QKeyEvent>
#include <QTranslator>
#include <QImage>
#include <QTimeZone>
#include <QSoftMenuBar>
#include <QWaitWidget>
#include <QSettings>

#include "qtopiainputevents.h"

#ifdef Q_WS_QWS
#include <QWSServer>
#endif

#include "uifactory.h"

#include <custom.h>

static FirstUse *firstUse = 0;

class FirstUse_KeyboardFilter : public QObject, public QtopiaKeyboardFilter
{
    Q_OBJECT

public:
    FirstUse_KeyboardFilter(FirstUse *p) {parent = p;};
    ~FirstUse_KeyboardFilter() {};

    bool filter(int unicode, int keycode, int modifiers, bool isPress, bool autoRepeat)
    {
        return parent->filter(unicode, keycode, modifiers, isPress, autoRepeat);
    }

private:
    FirstUse *parent;
};

static QDialog *createLanguage(QWidget *parent) {
    LanguageSettings *dlg = new LanguageSettings(parent);
    dlg->setConfirm(false);
    dlg->setNotify(false);
    return dlg;
}

static void acceptLanguage(QDialog *)
{
    if (firstUse)
        firstUse->reloadLanguages();
}

static QDialog *createDateTime(QWidget *parent) {
    SetDateTime *dlg = new SetDateTime(parent);
    dlg->setTimezoneEditable(true);
    dlg->setExternalTimeSourceWarnings(false);
    return dlg;
}

struct Settings {
    bool enabled;
    QDialog *(*createFunc)(QWidget *parent);
    void (*acceptFunc)(QDialog *dlg);
    const char *trans;
    const char *desc;
    bool needIM;
};

Settings settingsTable[] =
{
    { true, createLanguage, acceptLanguage, "language.qm", QT_TRANSLATE_NOOP("FirstUse", "Language"), false },
    { true, createDateTime, 0, "systemtime.qm", QT_TRANSLATE_NOOP("FirstUse", "Date/Time"), true },
    { false, 0, 0, "", "", false }
};


/*!
    \class FirstUse
    \inpublicgroup QtEssentialsModule
    \brief The FirstUse class provides a setup wizard that is shown the first time the device is turned on.
    \ingroup QtopiaServer::GeneralUI

    FirstUse provides a sequence of dialogs that are used to:
    \list
        \o Select the Language.
        \o Set the date and time.
    \endlist

    The following boolean settings in the \c Startup group of the \c qpe.conf configuration
    file can be used to specify which dialogs are shown.

    \table
        \header \o Setting \o Description
        \row \o FirstUse \o Enable or disable the first use dialog.
        \row \o NeedTimeZone \o Enable or disable presentation of the date/time dialog.
    \endtable

    This class is part of the Qt Extended server and cannot be used by other applications. Any
    server component that uses this dialog should create an instance via
    UIFactory::createDialog().

    \sa UIFactory
*/


static bool enableLanguageSelection()
{
    int languageCount = 0;

    foreach (QString tfn, Qtopia::installPaths()) {
        tfn +="i18n/";

        foreach (const QString &id, QDir(tfn).entryList(QStringList(), QDir::AllDirs | QDir::NoDotAndDotDot)) {
            QFileInfo desktopFile(tfn + "/" + id + "/.directory");
            if (desktopFile.exists()) {
                languageCount++;
                if (languageCount > 1)
                    return true;
            }
        }
    }

    return false;
}

/*!
    Constructs a FirstUse instance with the given \a parent and flags \a f.
*/
FirstUse::FirstUse(QWidget *parent, Qt::WFlags f)
: QDialog(parent, f), currDlgIdx(-1), currDlg(0),
  avoidEarlyExit(true)
{
    setObjectName("firstuse");
    setWindowFlags(Qt::FramelessWindowHint);
    // reading the settings to know what is needed
    QSettings config("Trolltech","qpe");
    config.beginGroup( "Startup" );

    // depending on the number of available languages, language selection is enabled or not
    settingsTable[0].enabled = enableLanguageSelection();

    // depending on the config file, time zone selection is enabled or not
    settingsTable[1].enabled = config.value( "NeedTimeZone", true ).toBool();

    // we force our height beyound the maximum (which we set anyway)
    QDesktopWidget *desktop = QApplication::desktop();
    QRect desk = desktop->screenGeometry(desktop->primaryScreen());
    setGeometry(desk);

#ifdef Q_WS_QWS
    // more hackery
    // It will be run as either the main server or as part of the main server
    QWSServer::setScreenSaverIntervals(0);
    qpe_setBrightness(qpe_sysBrightnessSteps());
#endif

    setFocusPolicy(Qt::NoFocus);

    taskBar = new QWidget(0, Qt::Tool | Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);
    taskBar->setAttribute(Qt::WA_GroupLeader, true);

    back = new QPushButton(tr("<< Back"), taskBar);
    back->setFocusPolicy(Qt::NoFocus);

    next = new QPushButton(tr("Next >>"), taskBar);
    next->setFocusPolicy(Qt::NoFocus);

    // need to set the geom to lower corner
    int x = 0;
    controlHeight = next->sizeHint().height();
    QSize sz(0,0);
    int buttonWidth = (width() - sz.width()) / 2;
    if (QApplication::layoutDirection() == Qt::LeftToRight)
        back->setGeometry(x, 0, buttonWidth, controlHeight);
    else
        next->setGeometry(x, 0, buttonWidth, controlHeight);
    x += buttonWidth;
    if (QApplication::layoutDirection() == Qt::LeftToRight)
        next->setGeometry(x, 0, buttonWidth, controlHeight);
    else
        back->setGeometry(x, 0, buttonWidth, controlHeight);

    taskBar->setGeometry( 0, height() - controlHeight, desk.width(), controlHeight);
    taskBar->hide();

    QWidget *w = new QWidget(0);
    w->showMaximized();
    int titleHeight = w->geometry().y() - w->frameGeometry().y();
    delete w;

    titleBar = new QLabel(0, Qt::Tool | Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);
    titleBar->setAttribute(Qt::WA_GroupLeader, true);
    QPalette pal = titleBar->palette();
    pal.setBrush(QPalette::Background, pal.brush(QPalette::Normal, QPalette::Highlight));
    pal.setColor(QPalette::Text, pal.color(QPalette::Normal, QPalette::HighlightedText));
    titleBar->setPalette(pal);
    titleBar->setAlignment(Qt::AlignCenter);
    titleBar->setGeometry(0, 0, desk.width(), titleHeight);
    titleBar->hide();

    calcMaxWindowRect();

    waitDialog = new QWaitWidget;

    reloadLanguages();
    firstUse = this;

#ifdef Q_WS_QWS
    FirstUse_KeyboardFilter *keyFilter = new FirstUse_KeyboardFilter(this);
    QWSServer::addKeyboardFilter(keyFilter);
#endif

    QSoftMenuBar::setLabel(this, Qt::Key_Context1, QSoftMenuBar::NoLabel);
}

/*!
    Destroys the FirstUse instance.
*/
FirstUse::~FirstUse()
{
#ifdef Q_WS_QWS
    QWSServer::removeKeyboardFilter();
#endif
    delete taskBar;
    delete titleBar;
    firstUse = 0;
}

/*!
    \internal
*/
void FirstUse::calcMaxWindowRect()
{
#ifdef Q_WS_QWS
    QRect wr;
    QDesktopWidget *desktop = QApplication::desktop();
    QRect displayRect = desktop->screenGeometry(desktop->primaryScreen());
    QRect ir;
    if ( ir.isValid() ) {
        wr.setCoords(displayRect.x(), displayRect.y(),
                    displayRect.width()-1, ir.top()-1 );
    } else {
        wr.setCoords(displayRect.x(), displayRect.y(),
                    displayRect.width()-1,
                    displayRect.height() - controlHeight-1);
    }

    QWSServer::setMaxWindowRect( wr );
#endif
}

/*!
    \internal

    Sets the index of the next dialog to show to the next dialog in sequence.
*/
void FirstUse::nextDialog()
{
    currDlgIdx = findNextDialog(true);
}

/*!
    \internal

    Sets the index of the next dialog to show to the previous dialog in sequence.
*/
void FirstUse::previousDialog()
{
    if (currDlgIdx != 0)
        currDlgIdx = findNextDialog(false);
}

/*!
    \internal

    Switch to the next dialog.
*/
void FirstUse::switchDialog()
{
    if (currDlgIdx == -1) {
        QSettings config("Trolltech","qpe");
        config.beginGroup( "Startup" );
        config.setValue( "FirstUse", false );
        config.sync();

        QSettings cfg("Trolltech", "WorldTime");
        cfg.beginGroup( "TimeZones" );

        // translate the existing list of QTimeZone names
        // This is usually enforced during the startup of qpe
        // (main.cpp::refreshTimeZoneConfig). However when we use
        // First use we have to do it here again to ensure a
        // translation
        int zoneIndex = 0;
        QTimeZone curZone;
        QString zoneID;

        while (cfg.contains( "Zone"+ QString::number( zoneIndex ))){
            zoneID = cfg.value( "Zone" + QString::number( zoneIndex )).toString();
            curZone = QTimeZone( zoneID.toAscii().constData());
            if ( !curZone.isValid() ){
	      qWarning( "initEnvironment() Invalid QTimeZone %s", zoneID.toLatin1().constData() );
                break;
            }
            zoneIndex++;
        }

        waitDialog->show();
        qApp->processEvents();
        QTimer::singleShot(0, waitDialog, SLOT(deleteLater()));

        updateButtons();

        avoidEarlyExit = false;
        accept();
    } else {
        int shownDialogIndex = currDlgIdx;
        currDlg = settingsTable[currDlgIdx].createFunc(this);
        connect(currDlg, SIGNAL(accepted()), this, SLOT(dialogAccepted()));
        currDlg->showMaximized();
        currDlg->installEventFilter(this);
        updateButtons();
        currDlg->exec();
        if (shownDialogIndex == currDlgIdx)
            nextDialog();
        delete currDlg;
        currDlg = 0;
        QTimer::singleShot(0, this, SLOT(switchDialog()));
    }
}

/*!
    \internal

    Accepts the current dialog.
*/
void FirstUse::acceptCurrentDialog()
{
    if (currDlg)
        currDlg->accept();
}

/*!
    \internal

    Calls the accept function for the accepted dialog.
*/
void FirstUse::dialogAccepted()
{
    if (settingsTable[currDlgIdx].acceptFunc)
        settingsTable[currDlgIdx].acceptFunc(currDlg);
}

/*!
    \reimp
*/
bool FirstUse::eventFilter(QObject *o, QEvent* e)
{
    if (o == currDlg) {
        switch (e->type()) {
        case QEvent::WindowActivate:
            waitDialog->hide();
            updateButtons();
            break;
        case QEvent::WindowDeactivate:
            updateButtons();
            if (qApp->activeModalWidget() == this)
                waitDialog->show();
            break;
        default:
            break;
        }
    }

    return false;
}

/*!
    \internal

    Reload the translations after a language change.
*/
void FirstUse::reloadLanguages()
{
    // read language from config file.  Waiting on QCop takes too long.
    QSettings config("Trolltech","locale");
    config.beginGroup("Language");
    QString l = config.value("Language", "en_US").toString();
    QString cl = getenv("LANG");
    qWarning("%s", QString("language message - %1").arg(l).toLatin1().constData());
    // setting anyway...
    if (l.isNull())
        unsetenv("LANG");
    else
        setenv("LANG", l.toLatin1(), 1);

#ifndef QT_NO_TRANSLATION
    static QList<QTranslator *> translatorList;

    while(!translatorList.isEmpty())
    {
        QTranslator *t = translatorList.takeFirst();
        qApp->removeTranslator(t);
        delete t;
    }
    // clear old translators

    const char *qmFiles[] = { "qt.qm", "qpe.qm", "libqtopia.qm" , 0 };

    // qpe/library translation files.
    int i = 0;
    QTranslator *trans;
    while (qmFiles[i]) {
        trans = new QTranslator(qApp);
        QString atf = qmFiles[i];
        QString tfn = Qtopia::qtopiaDir() + "i18n/"+l+"/"+atf;
        qWarning("%s", QString("loading %1").arg(tfn).toLatin1().constData());
        if ( trans->load(tfn) ) {
            qWarning(" installing translator");
            qApp->installTranslator( trans );
            translatorList.append(trans);
        } else {
            delete trans;
        }
        i++;
    }

    // first use dialog translation files.
    i = 0;
    while (settingsTable[i].createFunc) {
        if (settingsTable[i].enabled && settingsTable[i].trans) {
            trans = new QTranslator(qApp);
            QString atf = settingsTable[i].trans;
            QString tfn = Qtopia::qtopiaDir() + "i18n/"+l+"/"+atf;
            qWarning("%s", QString("loading %1").arg(tfn).toLatin1().constData());
            if ( trans->load(tfn) ) {
                qWarning(" installing translator");
                qApp->installTranslator( trans );
                translatorList.append(trans);
            } else  {
                delete trans;
            }
        }
        i++;
    }

    QStringList qpepaths = Qtopia::installPaths();
    for (QStringList::Iterator qit=qpepaths.begin();
            qit != qpepaths.end(); ++qit ) {
        QTranslator t(0);
        QString tfn = *qit+"i18n/";
        if (t.load(tfn+l+"/QtopiaDefaults.qm")) {
            QSettings fmcfg(QSettings::SystemScope, "Trolltech","qpe");
            fmcfg.beginGroup("Font");
            QString f = config.value( QLatin1String("FontFamily[]"), "dejavu_sans_condensed").toString();
            f = t.translate( "QPE", f.toAscii().constData() );
        }
    }
#endif
    updateButtons();
}

/*!
    \internal
*/
void FirstUse::paintEvent(QPaintEvent *)
{
    if (currDlgIdx >= 0)
        return;

    QPainter p(this);

    QImage img(":image/FirstUseBackground");
    p.drawImage(rect(), img);

    QFont f = p.font();
    f.setPointSize(8);
    f.setItalic(false);
    f.setBold(false);

    p.setFont(f);
    p.setPen(QPen(Qt::black));

    QString msg;
    if (!Qtopia::mousePreferred())
        msg = tr("Press Select to continue.");
    else
        msg = tr("Tap anywhere on the screen to continue.");

    // draw the text between 0.2 * width and 0.8 * width
    p.drawText(int(0.2 * width()), int(height() / 2), int(0.6 * width()), height(), Qt::TextWordWrap, msg);
}

/*!
    \internal
*/
int FirstUse::findNextDialog(bool forwards)
{
    int i;
    if (forwards) {
        i = currDlgIdx+1;
        while ( settingsTable[i].createFunc && !settingsTable[i].enabled )
            i++;
        if ( !settingsTable[i].createFunc )
            i = -1;
    } else {
        i = currDlgIdx-1;
        while ( i >= 0 && !settingsTable[i].enabled )
            i--;
    }
    return i;
}


/*!
    \reimp
*/
void FirstUse::done(int r)
{
    //avoid early exit of Dialog by pressing Back button
    //catching close event not possible -> see CloseNoEvent flag in QDialog::done(.)
    if (avoidEarlyExit)
        return;
    QDialog::done(r);
}

/*!
    \internal
*/
void FirstUse::updateButtons()
{
    QWidget *w = QApplication::activeModalWidget();
    if ((w == 0 || w == this || w == waitDialog) && (currDlgIdx == -1)) {
        /* don't show buttons when this dialog is on top */
        taskBar->hide();
        titleBar->hide();
        back->setEnabled(false);
        next->setEnabled(false);
    } else if (w == currDlg) {
        if ( currDlgIdx >= 0 ) {
            taskBar->show();
            titleBar->setText("<b>"+tr(settingsTable[currDlgIdx].desc)+"</b>");
            titleBar->show();
        }

        back->setText(tr("<< Back"));
        back->disconnect();
        connect(back, SIGNAL(clicked()), this, SLOT(acceptCurrentDialog()));
        connect(back, SIGNAL(clicked()), this, SLOT(previousDialog()));
        back->setEnabled(findNextDialog(false) >= 0);

        if (findNextDialog(true) < 0)
            next->setText(tr("Finish"));
        else
            next->setText(tr("Next >>"));
        next->disconnect();
        connect(next, SIGNAL(clicked()), this, SLOT(acceptCurrentDialog()));
        connect(next, SIGNAL(clicked()), this, SLOT(nextDialog()));
        next->setEnabled(true);
    } else if (w != this && w != waitDialog) {
        /* other sub-dialog is on top */
        back->setText(QString());
        back->setEnabled(false);
        next->setText(tr("Accept"));
        next->disconnect();
        connect(next, SIGNAL(clicked()), w, SLOT(accept()));
        next->setEnabled(true);
    }
}

/*!
    \internal
*/
void FirstUse::keyPressEvent( QKeyEvent *e )
{
    if (e->key() == Qt::Key_Select && currDlgIdx < 0)
        showFirstDialog();

    QDialog::keyPressEvent(e);
}

/*!
    \internal
*/
void FirstUse::mouseReleaseEvent( QMouseEvent *event )
{
    if (currDlgIdx < 0)
        showFirstDialog();

    QDialog::mouseReleaseEvent( event );
}

/*!
    \internal

    Show the first dialog.
*/
void FirstUse::showFirstDialog()
{
    nextDialog();

    currDlg = 0;

    // Remove welcome screen
    update();

    waitDialog->show();
    qApp->processEvents();

    QTimer::singleShot(0, this, SLOT(switchDialog()));
}

/*!
    \internal

    Returns the index of the current dialog displayed by FirstUse. If the current dialog
    is not currently active or there is no current dialog -1 is returned.
*/
int FirstUse::currentDialogIndex() const
{
    QWidget *w = QApplication::activeModalWidget();
    if (w != this && w != currDlg)
        return -1;

    return currDlgIdx;
}

/*!
    \internal
*/
bool FirstUse::filter(int unicode, int keycode, int modifiers, bool isPress, bool autoRepeat)
{
    Q_UNUSED(unicode);
    Q_UNUSED(modifiers);
    Q_UNUSED(autoRepeat);

    QWidget *w = QApplication::activeModalWidget();
    if (w != 0 && w != this && w != waitDialog && w != currDlg)
        return false;

    // filter key press events for this, waitDialog and currDlg
    if (isPress) {
        switch (keycode) {
        case Qt::Key_Back:
            if (currentDialogIndex() >= 0) {
                acceptCurrentDialog();
                nextDialog();
            }
            return true;
        case Qt::Key_Context1:
            if (currentDialogIndex() > 0) {
                acceptCurrentDialog();
                previousDialog();
            }
            return true;
        }
    }

    return false;
};


UIFACTORY_REGISTER_WIDGET( FirstUse );
#include "firstuse.moc"
