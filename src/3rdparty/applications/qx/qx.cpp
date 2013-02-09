#include "qx.h"
#include "AppSettings.h"
#include "AppInfo.h"

QxMainWindow *mainWin;

void QX::listClicked()
{
    if (!favouritesAction->isChecked())
    {
        favourites.clear();
        for (int i=0; i<lw->count(); i++)
            if (lw->item(i)->checkState() == Qt::Checked)
                favourites << scanner->entries[i].file;
        favConf->beginGroup("Apps");
        favConf->setValue("list", favourites);
        favConf->endGroup();
    }
}

int QX::GetClickedId()
{
    int sel = -1;
    for (int i=0; i<lw->count(); i++)
        if (lw->item(i)->isSelected())
        {
            sel = lw->item(i)->data(Qt::UserRole).toInt();
            break;
        }
    return sel;
}

void QX::settings_clicked()
{
    int sel = GetClickedId();
    if (sel<0) return;

    QString appname = scanner->entries[sel].file;
    SettingsDialog *win= new SettingsDialog(appname, this);
    win->show();
}

void QX::info_clicked()
{
    int sel = GetClickedId();
    if (sel<0) return;

    InfoDialog *info_dlg = new InfoDialog(scanner->entries[sel], this);
    info_dlg->show();
}

void QX::favourites_clicked()
{
    FillApps(favouritesAction->isChecked());
}

void QX::quitClicked()
{
    mainWin->close();
}

void QX::BuildMenu()
{
#ifdef QTOPIA
    menu = QSoftMenuBar::menuFor(this);
#else
    menu = mainWin->menuBar()->addMenu("&File");
#endif
    menu->addAction(tr("Launch"),this,SLOT(launch_clicked()));
    menu->addAction(tr("Settings"),this,SLOT(settings_clicked()));
    menu->addAction(tr("Info"),this,SLOT(info_clicked()));
    menu->addSeparator();
    favouritesAction = menu->addAction(tr("Favourites"),this,SLOT(favourites_clicked()));
    favouritesAction->setCheckable(true);
    menu->addSeparator();
    menu->addAction(tr("Quit"),this,SLOT(quitClicked()));
}

void QX::LoadFavourites()
{
#ifdef QTOPIA
    QString dir = Qtopia::qtopiaDir();
#else
    QString dir;
#endif
    favConf = new QSettings(dir + "/etc/qx/favourites.conf", QSettings::IniFormat);
    favConf->beginGroup("Apps");
    favourites = favConf->value("list").toStringList();
    favConf->endGroup();
}

void QX::FillApps(bool filter)
{
    lw->clear();
    for (int i=0; i<scanner->entries.count(); i++)
    {
        DesktopEntry entry = scanner->entries[i];
        bool cont = favourites.contains(entry.file);

        if ((!filter) || (cont))
        {
            QPixmap pm(entry.icon);
            if (pm.isNull()) pm.load(":image/qx/qx");
            pm = pm.scaled(64,64);
            QListWidgetItem *element = new QListWidgetItem(pm, entry.name);
            element->setData(Qt::UserRole, i);
            lw->addItem(element);
            if (!filter)
            {
                Qt::CheckState state = (cont) ? Qt::Checked : Qt::Unchecked;
                element->setCheckState(state);
            }
        }
    }
}


QX::QX(QWidget *parent, Qt::WFlags f)
        : QWidget(parent)
{
    Q_UNUSED(f);

    BuildMenu();
    favouritesAction->setChecked(true);

    lineEdit = new QLineEdit("xterm", this);

    bOk = new QPushButton(this);
    bOk->setMinimumWidth(100);
    bOk->setText("Run");
    connect(bOk, SIGNAL(clicked()), this, SLOT(okClicked()));

    lw = new QListWidget(this);
    connect(lw, SIGNAL(clicked(QModelIndex)), this, SLOT(listClicked()));

    lAppname = new QLabel(this);
    bResume = new QPushButton(this);
    bTerminate = new QPushButton(this);
    lAppname->setVisible(false);
    lAppname->setAlignment(Qt::AlignCenter);
    bResume->setVisible(false);
    bTerminate->setVisible(false);
    connect(bResume, SIGNAL(clicked()), this, SLOT(resumeClicked()));
    connect(bTerminate, SIGNAL(clicked()), this, SLOT(terminateClicked()));

    //------------------------------------------

    grid=new QGridLayout(this);
    grid->addWidget(lineEdit,0,0);
    grid->addWidget(bOk,0,1);
    grid->addWidget(lw,1,0,1,2);
    grid->addWidget(lAppname,2,0,1,2);
    grid->addWidget(bResume,3,0,1,2);
    grid->addWidget(bTerminate,4,0,1,2);

    LoadFavourites();
    scanner = new DesktopScanner();
    FillApps(favouritesAction->isChecked());

    //==========================================

    appRunScr = new AppRunningScreen();
    connect(appRunScr, SIGNAL(deactivated()), this, SLOT(pauseApp()));
    connect(appRunScr, SIGNAL(keyPress(QKeyEvent *)), this, SLOT(keyPress(QKeyEvent *)));
    connect(appRunScr, SIGNAL(keyRelease(QKeyEvent *)), this, SLOT(keyRelease(QKeyEvent *)));

    process = NULL;
    xprocess = NULL;
    rotHelper = new RotateHelper(this, 0);
    wmTimer = new QTimer(this);
    connect(wmTimer, SIGNAL(timeout()), this, SLOT(processWmEvents()));
    screen = QX::ScreenMain;

    if(getenv("DISPLAY") == NULL)
        setenv("DISPLAY", ":0.0", true);

#if QTOPIA
    powerConstraint = QtopiaApplication::Disable;

    // Start the "QX" service that handles application switching.
    new QxService(this);
#endif

    showScreen(QX::ScreenMain);
}

QX::~QX()
{

}

QxMainWindow::QxMainWindow(QWidget *parent, Qt::WFlags f)
        : QMainWindow(parent, f)
{
#ifdef QTOPIA
    this->setWindowState(Qt::WindowMaximized);
#else
    resize(640, 480);
#endif
    Q_UNUSED(f);

    mainWin = this;
    setCentralWidget(new QX(this));
}

QxMainWindow::~QxMainWindow()
{

}

#ifdef QTOPIA
static void gpsPower(const char *powerStr)
{
    QFile f("/sys/devices/platform/gta02-pm-gps.0/power_on");
    f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate);
    f.write(powerStr);
    f.close();
}

#endif

void QX::showScreen(QX::Screen scr)
{
    if(scr < QX::ScreenFullscreen && this->screen >= QX::ScreenFullscreen)
    {
        appRunScr->hide();
        if(rotate)
        {
            rotHelper->stop();
        }
#ifdef QTOPIA
        if(powerConstraint != QtopiaApplication::Enable)
        {
            QtopiaApplication::setPowerConstraint(QtopiaApplication::Enable);
        }
#endif
    }
    if(scr >= QX::ScreenFullscreen && this->screen < QX::ScreenFullscreen)
    {
        appRunScr->showScreen(fullscreen, kbd);
        if(rotate)
        {
            //system("xrandr -o 1");
            rotHelper->start(2000);
        }
#ifdef QTOPIA
        if(powerConstraint != QtopiaApplication::Enable)
        {
            QtopiaApplication::setPowerConstraint(powerConstraint);
        }
#endif
    }

    this->screen = scr;

    bOk->setVisible(scr == QX::ScreenMain);
    //bQuit->setVisible(scr == QX::ScreenMain || scr == QX::ScreenPaused);
    //bTango->setVisible(scr == QX::ScreenMain);
    //bScummvm->setVisible(scr == QX::ScreenMain);
    lineEdit->setVisible(scr == QX::ScreenMain);
    lw->setVisible(scr == QX::ScreenMain);

    lAppname->setVisible(scr == QX::ScreenPaused);
    bResume->setVisible(scr == QX::ScreenPaused);
    bTerminate->setVisible(scr == QX::ScreenPaused);

    for (int i=0; i<menu->actions().count(); i++)
    {
        menu->actions()[i]->setEnabled(scr == QX::ScreenMain);
    }

    switch(scr)
    {
    case QX::ScreenMain:
        update();
        break;
    case QX::ScreenPaused:
        lAppname->setText(appName);
        bResume->setText(tr("Resume"));
        bTerminate->setText(terminating ? tr("Kill") : tr("Stop"));
        break;
    default:
        break;
    }
}

void QX::stopX()
{
    if(dpy != NULL)
    {
        XCloseDisplay(dpy);
        dpy = NULL;
    }
    if(wm)
    {
        wmTimer->stop();
        wm_stop();
    }
    if(xprocess == NULL)
    {
        return;
    }
    if(xprocess->state() != QProcess::NotRunning)
    {
        xprocess->terminate();
        if(!xprocess->waitForFinished(3000))
        {
            xprocess->kill();
        }
        xprocess->waitForFinished();
    }
    delete(xprocess);
    xprocess = NULL;
}

static bool saveConf(QX * parent, const char * srcFilename, const char * dstDir, const char * dstFile)
{
    QFile src(srcFilename);
    QFile dst(dstDir + QString("/") + dstFile);

    if(!QDir::root().mkpath(dstDir))
    {
        QMessageBox::critical(parent, QObject::tr("QX"), QObject::tr("Unable to create directory") + " " + dstDir);
        return false;
    }
    if(!src.open(QIODevice::ReadOnly))
    {
        QMessageBox::critical(parent, QObject::tr("QX"), QObject::tr("Unable to open") + " " + srcFilename + ": " + src.errorString());
        return false;
    }
    if(!dst.open(QIODevice::WriteOnly))
    {
        QMessageBox::critical(parent, QObject::tr("QX"), QObject::tr("Unable to save ") + " " + dstFile + ": " + dst.errorString());
        src.close();
        return false;
    }
    QByteArray content = src.readAll();
    src.close();
    dst.write(content);
    dst.close();
    return true;
}

bool QX::checkX()
{
    if(QFile::exists("/usr/bin/Xorg") ||
       QFile::exists("/usr/bin/Xglamo") ||
       QFile::exists("/usr/bin/Xfbdev")
    )
    {
        return true;
    }
    int val = QMessageBox::question(this, tr("X server not found"),
                                    tr("You don't have X server. Choose X server to install:"),
                                    tr("Xorg"),
#ifdef QT_QWS_NEO
                                    tr("Xglamo"),
#else
                                    tr("Xfbdev"),
#endif                                    
                                    tr("Cancel"), 0, 2);

    if(val == 2)
    {
        return false;
    }
    if(val == 0)
    {
        QStringList args;
        args << "-u";
        args << "-i";
#ifdef QT_QWS_NEO
        args << "xserver-xorg-video-glamo";
#else
        args << "xserver-xorg-video-fbdev";  
#endif        
        args << "xserver-xorg-input-tslib";
        args << "xinput";
        args << "xterm";
        QProcess::execute("raptor", args);

#ifdef QT_QWS_NEO
        return saveConf(this, ":/xorg-glamo.conf", "/etc/X11", "xorg.conf") &&
#else
        return saveConf(this, ":/xorg.conf", "/etc/X11", "xorg.conf") &&
#endif
                saveConf(this, ":/xterm.desktop", "/usr/share/applications", "xterm.desktop");
    }

#ifdef QT_QWS_NEO    
    QProcess::execute("raptor", QStringList() << "-u" << "-i" << "xfonts-base" << "xterm" << "x11-xserver-utils");
    QProcess::execute("raptor", QStringList() << "-i" << "http://qtmoko.sourceforge.net/download/Xglamo.deb");
#else
    QProcess::execute("raptor", QStringList() << "-u" << "-i" << "xfonts-base" << "xterm" << "x11-xserver-utils" << "xserver-xfbdev");
#endif

#ifdef QT_QWS_NEO
    return saveConf(this, ":/xglamo.conf", "/etc/X11", "xorg.conf") &&
#else
    return saveConf(this, ":/xfbdev.conf", "/etc/X11", "xorg.conf") &&
#endif
            saveConf(this, ":/xterm.desktop", "/usr/share/applications", "xterm.desktop");
}

void QX::fixTs()
{
    system("DISPLAY=:0 xinput set-int-prop \"Touchscreen\" \"Evdev Axis Calibration\" 32 107 918 911 98");
    system("DISPLAY=:0 xinput set-int-prop \"Touchscreen\" \"Evdev Axes Swap\" 8 1");
}

void QX::runApp(QString filename, QString applabel, bool rotate)
{
    //this->appName = filename;
    this->appName = applabel;
    this->rotate = rotate;
    terminating = false;
    bool xorg = QFile::exists("/usr/bin/Xorg");

    showScreen(QX::ScreenStarting);

    // Ask to delete X temporary files - hack for recovering from crashed X
    if(QFile::exists("/tmp/.X0-lock"))
    {
        if(QMessageBox::question(this, tr("QX"), tr("The X lock file exists (X is running), delete it and start new X server?"),
                                                 QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
        {
            system("rm -rf /tmp/.X*");
        }
    }
    if(!QFile::exists("/tmp/.X0-lock"))
    {
        xprocess = new QProcess(this);
        xprocess->setProcessChannelMode(QProcess::ForwardedChannels);
        QStringList args;
        if(xorg)
        {
            xprocess->start("/usr/bin/Xorg", args);
        }
        else
        {
#ifdef QT_QWS_NEO
            args.append("-hide-cursor");
            args.append("-dpi");
            args.append("128");
            xprocess->start("/usr/bin/Xglamo", args);
#else
            args.append("-nocursor");
            args.append("-dpi");
            args.append("128");
            xprocess->start("/usr/bin/Xfbdev", args);
#endif
        }
        if(!xprocess->waitForStarted())
        {
            showScreen(QX::ScreenMain);
            QMessageBox::critical(this, tr("QX"), tr("Unable to start X server"));
            return;
        }
    }

    for(int i = 0; i < 3; i++)
    {
        dpy = XOpenDisplay(NULL);
        if(dpy != NULL)
        {
            break;
        }
        Sleeper::msleep(1000);
    }
    if(dpy == NULL)
    {
        stopX();
        showScreen(QX::ScreenMain);
        QMessageBox::critical(this, tr("QX"), tr("Unable to connect to X server"));
        return;
    }
    fakeKey = fakekey_init(dpy);

    showScreen(QX::ScreenRunning);
    QApplication::processEvents();

    if(wm)
    {
        // Hack - if not in fullscreen we want apps to be started below qtopia
        // status bar
        int top = fullscreen ? 0 : 80;
        int width = appRunScr->width();
        int height = 640 - top;

        if(kbd)
        {
            height -= 137;      // hack: keyboard height - how to get correct size?
        }

        //qDebug() << " top=" << top << " appRunScr size: " << width << "x" << height;

        wm_start(dpy, 0, top, width, height);
        wmTimer->start(10);
    }

    process = new QProcess(this);
    connect(process, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(processFinished(int, QProcess::ExitStatus)));
    process->setProcessChannelMode(QProcess::ForwardedChannels);

    // Remove QtMoko LD_LIBRARY_PATH so that X11-QT programs work ok
    unsetenv("LD_LIBRARY_PATH");

    process->start(filename, NULL);

    if(!process->waitForStarted())
    {
        delete(process);
        process = NULL;
        stopX();
        showScreen(QX::ScreenMain);
        QMessageBox::critical(this, tr("QX"), tr("Unable to start") + " " + filename);
        return;
    }

#ifdef QT_QWS_NEO    
    if(xorg)
    {
        QTimer::singleShot(1000, this, SLOT(fixTs()));
    }
#endif    
}

void QX::processWmEvents()
{
    wm_process_events();
}

void QX::pauseApp()
{
    if(process == NULL)
    {
        return;
    }
    if(rotate)
    {
        system("xrandr -o 0");
    }
    if(wm)
    {
        wmTimer->stop();
    }

    system(QString("kill -STOP %1").arg(process->pid()).toAscii());
    if(xprocess)
    {
        system(QString("kill -STOP %1").arg(xprocess->pid()).toAscii());
    }
    showScreen(QX::ScreenPaused);
}

void QX::resumeApp()
{
    if(xprocess)
    {
        system(QString("kill -CONT %1").arg(xprocess->pid()).toAscii());
    }
    system(QString("kill -CONT %1").arg(process->pid()).toAscii());
    if(wm)
    {
        wmTimer->start(10);
    }
    showScreen(QX::ScreenRunning);
}

void QX::okClicked()
{
    if(!checkX()) return;
    //if (screen == QX::ScreenMain)
    runApp(lineEdit->text(), lineEdit->text(), false);
}

void QX::resumeClicked()
{
    //if (screen == QX::ScreenPaused)
    resumeApp();
}

void QX::keyPress(QKeyEvent *e)
{
    QString text = e->text();
    if(text.length() > 0)
    {
        QByteArray buf = text.toUtf8();
        fakekey_press(fakeKey, (unsigned char *)(buf.constData()), buf.length(), 0);
        return;
    }
    // See keysymdef.h for KeySym values
    KeySym sym = -1;
    switch(e->key())
    {
    case Qt::Key_Left:
        sym = 0xff51;
        break;
    case Qt::Key_Right:
        sym = 0xff53;
        break;
    case Qt::Key_Up:
        sym = 0xff52;
        break;
    case Qt::Key_Down:
        sym = 0xff54;
        break;
    }
    fakekey_press_keysym(fakeKey, sym, 0);
}

void QX::keyRelease(QKeyEvent *)
{
    fakekey_release(fakeKey);
}


/*
void QX::tangoClicked()
{
#ifdef QTOPIA
    gpsPower("1");
    powerConstraint = QtopiaApplication::DisableSuspend;
#endif
    system("gpsd /dev/ttySAC1");
    runApp("tangogps", false);
}

void QX::scummvmClicked()
{
#ifdef QTOPIA
    powerConstraint = QtopiaApplication::Disable;
#endif
    runApp("/usr/games/scummvm", true);
}
*/

void QX::terminateClicked()
{
    if(process)
    {
        // because SIGTERM does not work on stopped process and we also give
        // program chance to terminate correctly (save data etc...)
        resumeApp();
        process->terminate();
        if(terminating && !process->waitForFinished(3000))
        {
            process->kill();
        }
        terminating = true;
    }
    else
    {
        close();
    }
}

void QX::processFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitCode);
    Q_UNUSED(exitStatus);
    delete(process);
    process = NULL;
    stopX();
    appRunScr->pixmap = QPixmap();
#ifdef QTOPIA
    powerConstraint = QtopiaApplication::Enable;
#endif
    showScreen(QX::ScreenMain);
}

//-----------------------------------------------

void QX::launch_clicked()
{
    int sel = GetClickedId();
    if (sel<0) return;

    if(!checkX()) return;

    DesktopEntry entry = scanner->entries[sel];
    QString appname = entry.file;

    ProfileManager *mngr = new ProfileManager();
    AppProfile prof = mngr->GetAppProfile(appname);
    delete mngr;

#ifdef QTOPIA
    if (prof.antiDim)
    {
        powerConstraint = QtopiaApplication::Disable;
    }
    else if (prof.antiSuspend)
    {
        powerConstraint = QtopiaApplication::DisableSuspend;
    }

    if (prof.gps)
        gpsPower("1");
#endif

    if (prof.init != "")
        system(prof.init.toUtf8().data());

    //if (prof.qvga) { }

    QString cmd = entry.exec;
    this->wm = prof.wm;
    this->kbd = prof.kbd;
    this->fullscreen = prof.fullscreen;

    if (prof.matchbox)
    {
        if(!QFile::exists("/usr/bin/matchbox-window-manager") &&
           QMessageBox::question(this, "qx", tr("Install matchbox?"),
                                 QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
        {
            QProcess::execute("raptor", QStringList() << "-u" << "-i" << "matchbox");
        }

        if(prof.wm || prof.kbd)
        {
            QString script = "/tmp/.QX_app_launcher.sh";
            QFile f(script);
            if (f.open(QIODevice::WriteOnly))
            {
                f.write("matchbox-window-manager &\n");
                f.write("sleep 5\n");
                if (prof.kbd)
                    f.write("matchbox-keyboard &\n");
                f.write(entry.exec.toUtf8());
                f.close();
            }
            cmd = "sh " + script;
            this->wm = this->kbd = false;
        }
    }

    QString applabel = "<b>"+entry.name+"</b><br/>"+entry.exec;
    runApp(cmd, applabel, prof.rotate);
}
