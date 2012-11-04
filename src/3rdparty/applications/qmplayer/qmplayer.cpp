#include "qmplayer.h"
#include <QDebug>

#include <sys/time.h>
#include <sys/resource.h>

QMplayerMainWindow *mainWin;

QMplayer::QMplayer(QWidget * parent, Qt::WFlags f)
:  QWidget(parent)
    , screen(ScreenInit)
    , fsPlay()
    , fsRefresh()
{
#ifdef QTOPIA
    this->setWindowState(Qt::WindowMaximized);
    softm = QSoftMenuBar::menuFor(mainWin);
    rmMpAction =
        softm->addAction(tr("Remove mplayer"), this, SLOT(removeMplayer()));
    rmDlAction =
        softm->addAction(tr("Remove youtube-dl"), this,
                         SLOT(removeYoutubeDl()));
    rmFlvAction =
        softm->addAction(tr("Remove FLV videos"), this, SLOT(removeFlv()));
#else
    Q_UNUSED(f);
#endif
    lw = new QListWidget(this);
    int h = lw->fontMetrics().height();
    lw->setIconSize(QSize(h, h));

    scanItem = new QListWidgetItem(tr("Scan"), lw);
    scanItem->setSelected(true);

    sharingItem = new QListWidgetItem(tr("Sharing"), lw);

    encodingItem = new QListWidgetItem(lw);

    bOk = new QPushButton(this);
    connect(bOk, SIGNAL(clicked()), this, SLOT(okClicked()));

    bBack = new QPushButton(this);
    connect(bBack, SIGNAL(clicked()), this, SLOT(backClicked()));

    bUp = new QPushButton(this);
    connect(bUp, SIGNAL(clicked()), this, SLOT(upClicked()));

    bDown = new QPushButton(this);
    connect(bDown, SIGNAL(clicked()), this, SLOT(downClicked()));

    label = new QLabel(this);
    lineEdit = new QLineEdit(this);
    progress = new QProgressBar(this);

    buttonLayout = new QHBoxLayout();
    buttonLayout->setAlignment(Qt::AlignBottom);
    buttonLayout->addWidget(bOk);
    buttonLayout->addWidget(bBack);
    buttonLayout->addWidget(bUp);
    buttonLayout->addWidget(bDown);

    layout = new QVBoxLayout(this);
    layout->addWidget(lw);
    layout->addWidget(label);
    layout->addWidget(lineEdit);
    layout->addWidget(progress);
    layout->addLayout(buttonLayout);

    connect(&fsPlay, SIGNAL(deactivated()), this, SLOT(handleFsDeactivate()));
    connect(&fsPlay, SIGNAL(volumeUp()), this, SLOT(volumeUp()));
    connect(&fsPlay, SIGNAL(volumeDown()), this, SLOT(volumeDown()));

    maxScanLevel = 0;
    delTmpFiles = -1;
    useBluetooth = -1;
    process = NULL;
    tcpServer = NULL;

    ubasedir = QDir::homePath();
    QString basedir1 = QDir::homePath() + "/Documents";
    QString basedir2 = "/media/card/Documents";
    QDir dirch;
    if (dirch.exists(basedir1))
        ubasedir = basedir1;
    if (dirch.exists(basedir2))
        ubasedir = basedir2;

    tube = false;
    QStringList cl_args = QCoreApplication::arguments();
    int ac = cl_args.count();
    if (ac > 1) {
        int tki = 1;
        while (true) {
            QString a = cl_args[tki];
            if (a == "--mpargs") {
                if (ac > tki + 1) {
                    mpargs << QString(cl_args[tki + 1]).split(" ");
                } else {
                    console("Not enough parameters for '--mpargs' argument");
                }
                tki += 2;
            } else if (a == "--basedir") {
                if (ac > tki + 1) {
                    ubasedir = cl_args[tki + 1];
                } else {
                    console("Not enough parameters for '--basedir' argument");
                }
                tki += 2;
            } else if (a == "--youtube-dl") {
                tube = true;
                tki += 1;
            } else {
                mplist << a;
                tki += 1;
            }

            if (tki >= ac)
                break;
        }
    }

    mpgui = (mplist.count() == 0);
    if (mpgui)
        showScreen(QMplayer::ScreenInit);
    else {
        if (tube) {
            while (mplist.count() > 1)
                mplist.removeLast();
            QTimer::singleShot(0, this, SLOT(sTimerEvent()));
        } else {
            this->screen = QMplayer::ScreenInit;
            okClicked();
        }
    }
}

QMplayer::~QMplayer()
{

}

void QMplayer::sTimerEvent()
{
    showScreen(QMplayer::ScreenTube);
    bool uok = youtubeDl();
    if (!uok) {
        if (QMessageBox::question(this, tr("qmplayer"),
                                  tr
                                  ("Program youtube-dl is necessary for downloading videos from Youtube. Do you want to install it now?"),
                                  QMessageBox::Yes,
                                  QMessageBox::No) == QMessageBox::Yes) {
            if (installYoutubeDl()) {
                showScreen(QMplayer::ScreenTube);
                uok = youtubeDl();
            } else
                QMessageBox::critical(this, tr("qmplayer"),
                                      tr
                                      ("Installation of youtube-dl has failed"));
        }
    }
    if (!uok)
        mainWin->close();
}

void QMplayer::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    if (fsPlay.pixmap.width() > 0) {
        p.drawPixmap(0, 0, fsPlay.pixmap);
    }
}

void QMplayer::closeEvent(QCloseEvent * event)
{
    QWidget::closeEvent(event);
}

void QMplayer::console(QString s)
{
    //qLog
    qDebug("%s", (s + "\n").toAscii().data());
}

static bool isDirectory(QString path)
{
    return path != NULL && (
#ifdef Q_WS_WIN
                               (path.length() >= 3) && (path.at(1) == ':')
#else
                               path.startsWith('/')
#endif
                               || path.startsWith("http://"));
}

// Return directory or filename suitable for placing as url.
// Result always starts and never ends with slash.
static QString pathToUrl(QString path)
{
    QString res(path);
    for (int i = 0; i < res.length(); i++) {
        QChar ch = res.at(i);
        if (ch == ':' || ch == ' ') {
            res[i] = '_';
        } else if (ch == '\\') {
            res[i] = '/';
        } else if (ch > 127) {
            res.remove(i, 1);
        }
    }
    while (res.endsWith('/') && res.length() > 0) {
        res.remove(res.length() - 1, 1);
    }
    if (res.at(0) != '/') {
        res.prepend('/');
    }
    return res;
}

// Returns if process is running. If process is starting then waits for start
// and returns start result.
static bool processRunning(QProcess * p)
{
    return p != NULL &&
        (p->state() == QProcess::Running ||
         (p->state() == QProcess::Starting && p->waitForStarted(1000)));
}

// Removes items from listview except top menu items.
static void delItems(QListWidget * lw)
{
    while (lw->count() > NUM_MENU_ITEMS) {
        delete(lw->takeItem(NUM_MENU_ITEMS));
    }
}

// Add item with directory name in list view.
static QListWidgetItem *getDirItem(QListView * lw, QString dir)
{
    QListWidgetItem *res = new QListWidgetItem(dir);
    res->setIcon(QApplication::style()->
                 standardIcon(QStyle::SP_DirIcon, 0, lw));
    return res;
}

void QMplayer::handleFsDeactivate()
{
    if (screen != ScreenFullscreen) {
        return;
    }
#ifdef QTOPIA
    QtopiaServiceRequest e1("QtopiaPowerManager", "setBacklight(int)");
    e1 << -1; // read brightness setting from config
    e1.send();
#endif
    if (processRunning(process)) {

        // On GTA04A3 (old model) we have to stop mplayer so that gsm voice
        // routing program can open sound card.
        bool gta04a3 = !QFile::exists("/sys/class/gpio/gpio186/value");
        if(gta04a3) {
            if (!fsPlay.clicked) {  // user did not press pause, so e.g. incomming call triggered this
                fsPlay.hide();
                stopMplayer();      // quit mplayer so that it does not block sound card e.g. when someone
                return;             // this might be not needed when we have HW sound routing or mixing on GTA04
            }
        }

        process->write(" ");
        process->waitForBytesWritten(1000);
    }
    if (fsPlay.clicked) {
        QTimer::singleShot(200, &fsRefresh, SLOT(showScreen()));    // these two lines refresh taskbar and softmenu bar
        QTimer::singleShot(250, &fsRefresh, SLOT(hide()));
        QTimer::singleShot(300, this, SLOT(finishPause()));
    } else {
        showScreen(ScreenPaused);
    }
}

void QMplayer::finishPause()
{
    update();
    showScreen(ScreenPaused);
}

void QMplayer::okClicked()
{
    if (screen == QMplayer::ScreenInit || screen == QMplayer::ScreenEncoding) {
        QString dir = "";
        bool hit = false;
        QStringList list;
        QStringList downList;
        QStringList nameList;
        QMessageBox::StandardButton answer = QMessageBox::NoButton;
        QMessageBox::StandardButton moreAnswer = QMessageBox::NoButton;

        list << mpargs;
        list << mplist;

        if (mpgui) {
            QListWidgetItem *sel = lw->currentItem();
            if (sel == NULL) {
                return;
            }
            if (sel == scanItem) {
                scan();
                return;
            }
            if (sel == sharingItem) {
                sharing();
                return;
            }
            if (sel == encodingItem) {
                if (screen != ScreenEncoding) {
                    showScreen(ScreenEncoding);
                    if (lw->count() > NUM_MENU_ITEMS) {
                        QListWidgetItem *item = lw->item(NUM_MENU_ITEMS);
                        item->setSelected(true);
                    }
                } else {
                    showScreen(ScreenInit);
                }
                return;
            }

            for (int i = NUM_MENU_ITEMS; i < lw->count(); i++) {
                QListWidgetItem *item = lw->item(i);
                QString path = item->text();
                bool isDir = isDirectory(path);
                if (isDir) {
                    if (hit) {
                        break;
                    }
                    dir = path;
                }
                hit |= (item == sel);
                if (!hit || isDir) {
                    continue;
                }
                if (!dir.startsWith("http://")) {
                    list.append(dir + "/" + path);  // add local files to playlist
                    continue;
                }
                if (moreAnswer == QMessageBox::NoButton) {
                    answer =
                        QMessageBox::question(this, "qmplayer", path,
                                              QMessageBox::Save | QMessageBox::
                                              Open | QMessageBox::Cancel);
                }
                if (answer == QMessageBox::Cancel) {
                    break;
                }
                if (answer == QMessageBox::Open) {
                    list.append(dir + pathToUrl(path)); // append to playlist if we just play (no download)
                }
                downList.append(dir + pathToUrl(path));
                nameList.append(path);
                if (moreAnswer != QMessageBox::YesToAll) {
                    moreAnswer =
                        QMessageBox::question(this, "qmplayer",
                                              tr("Next file?"),
                                              QMessageBox::Yes | QMessageBox::
                                              No | QMessageBox::YesToAll);

                    if (moreAnswer == QMessageBox::No) {
                        break;
                    }
                }
            }
            for (int i = 0; i < downList.count(); i++) {
                QString url = downList[i];
                int slashIndex = url.lastIndexOf('/');
                if (slashIndex < 0) {
                    continue;
                }
                QString filename = nameList[i];
                QString destPath = QDir::homePath() + "/" + filename;
                if (QDir("/media/card").exists()) {
                    destPath = "/media/card/" + filename;
                }
                bool justCheck = list.contains(url);
                if (!download(url, destPath, filename, justCheck)) {
                    return;
                }
                if (justCheck) {
                    continue;
                }
                // Add downloaded file to list
                list.append(destPath);
            }
        }

        if (list.count() > mpargs.count()) {
            if (screen == ScreenEncoding) {
                encode(list.at(0));
            } else {
                if (isPlaylist(list[mpargs.count()]))
                    list.insert(mpargs.count(), "-playlist");
                play(list);
            }
        }
    } else if (screen == QMplayer::ScreenPaused) {
        if (processRunning(process)) {
            process->write(" ");
#ifdef QT_QWS_NEO
            // Workaround unpause not working for alsa out in mplayer glamo.
            // We send left key to make mplayer start playing.
            process->write("\x1b" "[D");
#endif
        }
        showScreen(QMplayer::ScreenFullscreen);
    } else if (screen == QMplayer::ScreenConnect) {
        runClient();
    } else if (screen == QMplayer::ScreenTube) {
        this->screen = QMplayer::ScreenInit;
        okClicked();
    }
}

bool QMplayer::download(QString url, QString destPath, QString filename,
                        bool justCheck)
{
    QString host = url;
    QString reqPath;
    int port = 80;

    if (url.startsWith("http://")) {
        host.remove(0, 7);
    }

    int colonIndex = host.indexOf(':');
    int slashIndex = host.indexOf('/');
    if (slashIndex < 0) {
        return false;
    }
    reqPath = host.right(host.length() - slashIndex).replace(" ", "%20");
    host = host.left(slashIndex);
    if (colonIndex > 0) {
        QString portStr = host.right(host.length() - colonIndex - 1);
        host = host.left(colonIndex);
        port = portStr.toInt(0, 10);
    }

connect:
    QTcpSocket sock(this);
    sock.setReadBufferSize(65535);
    sock.connectToHost(host, port);
    if (!sock.waitForConnected(5000)) {
        QMessageBox::critical(this, tr("qmplayer"), sock.errorString());
        return false;
    }

    QByteArray req("GET ");
    req.append(reqPath);
    req.append(" HTTP/1.1\r\nHost: ");
    req.append(host);
    req.append(':');
    req.append(QByteArray::number(port));
    req.append("\r\n\r\n");

    sock.write(req);
    sock.flush();
    sock.waitForBytesWritten();

    int contentLen = 0;
    bool html = false;
    QByteArray line;
    for (;;) {
        line = sock.readLine();
        if (line.isEmpty()) {
            if (sock.waitForReadyRead(5000)) {
                continue;
            }
            break;
        }
        if (line.trimmed().isEmpty()) {
            break;
        }
        html = html | (line.indexOf("Content-Type: text/html") == 0);
        if (line.indexOf("Content-Length: ") == 0) {
            contentLen = line.remove(0, 16).trimmed().toInt(0, 10);
        }
    }

    if (html) {
        QByteArray text = sock.readAll();
        sock.close();
        if (text.length() == 0) {
            QMessageBox::critical(this, tr("qmplayer"),
                                  tr("No response from ") + host);
            return false;
        }
        text.replace("</br>", "\n");
        if (QMessageBox::information(this, "qmplayer", text,
                                     QMessageBox::Ok | QMessageBox::Retry) ==
            QMessageBox::Retry) {
            goto connect;
        }

        return false;
    } else if (justCheck) {
        sock.close();
        return true;
    }

    QFile f(destPath);
    if (!f.open(QFile::WriteOnly)) {
        QMessageBox::critical(this, tr("qmplayer"),
                              tr("Unable to save file:\r\n\r\n") +
                              f.errorString());
        sock.close();
        return false;
    }

    showScreen(QMplayer::ScreenDownload);
    label->setText(tr("Downloading") + " " + filename);
#ifdef QTOPIA
    QtopiaApplication::setPowerConstraint(QtopiaApplication::DisableSuspend);
#endif

    if (contentLen <= 0) {
        contentLen = 1024 * 1024;
    }
    progress->setMaximum(contentLen);
    progress->setValue(0);
    int remains = contentLen;

    char buf[65535];
    int count;
    abort = false;
    for (;;) {
        QApplication::processEvents();
        if (abort) {
            break;
        }
        if (sock.bytesAvailable() < 65535
            && sock.state() == QAbstractSocket::ConnectedState) {
            sock.waitForReadyRead(1000);
            continue;
        }
        count = sock.read(buf, 65535);
        if (count <= 0) {
            break;
        }
        f.write(buf, count);
        f.flush();
        remains -= count;
        if (remains <= 0) {
            remains = contentLen;
        }
        progress->setValue(contentLen - remains);
    }
    f.close();
    sock.close();

#ifdef QTOPIA
    QtopiaApplication::setPowerConstraint(QtopiaApplication::Enable);
#endif

    return true;
}

void QMplayer::stopMplayer()
{
    if (process == NULL) {
        return;
    }
    process->write("q");
    process->waitForFinished(4000);
    delete(process);
    process = NULL;
}

void QMplayer::backClicked()
{
    if (screen == QMplayer::ScreenInit) {
        mainWin->close();
    } else if (screen == QMplayer::ScreenPaused) {
        stopMplayer();
    } else if (screen == QMplayer::ScreenConnect) {
        showScreen(QMplayer::ScreenInit);
    } else if (screen == QMplayer::ScreenTube) {
        mainWin->close();
    } else if (screen == ScreenEncodingInProgress) {
        process->terminate();
    } else {
        abort = true;
    }
}

void QMplayer::volumeDown()
{
//    QProcess::execute("amixer", QStringList() << "set" << "Master" << "5%-");
    if (processRunning(process)) {
        process->write("9");
    }
}

void QMplayer::volumeUp()
{
//    QProcess::execute("amixer", QStringList() << "set" << "Master" << "5%+");
    if (processRunning(process)) {
        process->write("0");
    }
}

void QMplayer::upClicked()
{
    if (processRunning(process)) {
        if (screen == QMplayer::ScreenPaused) {
            process->write("\x1b" "[A ");
        } else {
            volumeUp();
        }
    }
}

void QMplayer::downClicked()
{
    if (processRunning(process)) {
        if (screen == QMplayer::ScreenPaused) {
            process->write("\x1b" "[B ");
        } else {
            volumeDown();
        }
    }
}

static void enableDisableSuspend(QMplayer::Screen scr, QMplayer::Screen newScr,
                                 QMplayer::Screen oldScr)
{
    if (scr == newScr) {
#ifdef QTOPIA
        QtopiaApplication::setPowerConstraint(QtopiaApplication::Disable);
#endif
    } else if (scr == oldScr) {
#ifdef QTOPIA
        QtopiaApplication::setPowerConstraint(QtopiaApplication::Enable);
#endif
    }
}

void QMplayer::showScreen(QMplayer::Screen scr)
{
    // Full screen -> normal
    if (screen == QMplayer::ScreenFullscreen) {
        setRes(640480);
        fsPlay.hide();
    }
    // Disable suspend if enter these screens and enable if leave
    enableDisableSuspend(ScreenEncodingInProgress, scr, screen);
    enableDisableSuspend(ScreenFullscreen, scr, screen);

    this->screen = scr;

    lw->setVisible(scr == QMplayer::ScreenInit
                   || scr == QMplayer::ScreenEncoding);
    bOk->setVisible(scr == QMplayer::ScreenInit || scr == QMplayer::ScreenPaused
                    || scr == QMplayer::ScreenConnect
                    || scr == QMplayer::ScreenTube
                    || scr == QMplayer::ScreenEncoding);
    bBack->setVisible(scr == QMplayer::ScreenInit
                      || scr == QMplayer::ScreenPaused
                      || scr == QMplayer::ScreenScan
                      || scr == QMplayer::ScreenConnect
                      || scr == QMplayer::ScreenTube
                      || scr == QMplayer::ScreenCmd
                      || scr == ScreenEncodingInProgress);
    bUp->setVisible(scr == QMplayer::ScreenPaused);
    bDown->setVisible(scr == QMplayer::ScreenPaused);
    label->setVisible(scr == QMplayer::ScreenScan
                      || scr == QMplayer::ScreenDownload
                      || scr == QMplayer::ScreenConnect
                      || scr == QMplayer::ScreenTube
                      || scr == QMplayer::ScreenCmd
                      || scr == ScreenEncodingInProgress);
    lineEdit->setVisible(scr == QMplayer::ScreenConnect);
    progress->setVisible(scr == QMplayer::ScreenScan
                         || scr == QMplayer::ScreenDownload
                         || scr == QMplayer::ScreenTube
                         || scr == QMplayer::ScreenCmd
                         || scr == ScreenEncodingInProgress);

    bOk->setEnabled(true);

#ifdef QTOPIA
    rmMpAction->setEnabled(scr == QMplayer::ScreenInit);
    rmDlAction->setEnabled(scr == QMplayer::ScreenInit);
    rmFlvAction->setEnabled(scr == QMplayer::ScreenInit);
#endif

    switch (scr) {
    case QMplayer::ScreenInit:
        bOk->setText(">");
        bBack->setText(tr("Quit"));
        encodingItem->setText(tr("Encode"));
        break;
    case QMplayer::ScreenFullscreen:
        fsPlay.showScreen();
#ifdef QTOPIA
        setRes(320240);
#endif
        break;
    case QMplayer::ScreenPaused:
        bOk->setText("Play");
        bBack->setText(tr("Back"));
        bUp->setText(tr(">>"));
        bDown->setText(tr("<<"));
        break;
    case QMplayer::ScreenConnect:
        label->setText(tr("Enter host and port to connect to"));
        bOk->setText(tr("Ok"));
        bBack->setText(tr("Cancel"));
        break;
    case QMplayer::ScreenScan:
    case QMplayer::ScreenDownload:
        bBack->setText(tr("Cancel"));
        break;
    case QMplayer::ScreenTube:
        setDlText();
        progress->setMaximum(100);
        progress->setValue((int)uload);
        bOk->setText("Watch Now!");
        bOk->setEnabled(!ufname.isEmpty());
        bBack->setText(tr("Cancel"));
        break;
    case QMplayer::ScreenCmd:
        bBack->setText(tr("Cancel"));
        break;
    case QMplayer::ScreenEncoding:
        encodingItem->setText(tr("Select file to encode"));
        break;
    case QMplayer::ScreenEncodingInProgress:
        bBack->setText(tr("Cancel"));
        progress->setMaximum(100);
        progress->setValue(0);
        break;
    }
}

void QMplayer::scan()
{
    showScreen(QMplayer::ScreenScan);
    delItems(lw);
    abort = false;

    progress->setMinimum(0);
    progress->setMaximum(0x7fffffff);

scan_files:

#ifdef Q_WS_WIN
    scanDir("c:\\", 0, maxScanLevel, 0, 0x1fffffff, false);
#else
    // For the first time scan /home/root/Documents and dont scan other dirs if
    // something found there.
    if (maxScanLevel > 0 ||
        !scanDir(QDir::homePath() + "/" + "Documents", 0, 2, 0, 0x1fffffff,
                 true)
        || !scanDir("/media/card/Documents", 0, 2, 0, 0x2fffffff, true)) {
        scanDir("/", 0, 0, 0, 0x1fffffff, true);
        scanDir("/mnt", 0, maxScanLevel + 1, 0, 0x2fffffff, false);
        scanDir("/media", 0, maxScanLevel + 1, 0, 0x3fffffff, false);
        scanDir(QDir::homePath(), 0, maxScanLevel, 0, 0x4fffffff, true);
        scanDir(QDir::homePath() + "/" + "Documents", 0, 2, 0, 0x5fffffff,
                true);
        scanDir("/media/card/Documents", 0, 2, 0, 0x5fffffff, true);
        scanDir("/root", 0, maxScanLevel, 0, 0x6fffffff, true);
    }
#endif

    maxScanLevel++;
    scanItem->setText(tr("Scan more"));

    if (lw->count() <= NUM_MENU_ITEMS && !abort &&
        QMessageBox::question(this, "qmplayer",
                              tr("No media files found, scan more?"),
                              QMessageBox::Yes | QMessageBox::No) ==
        QMessageBox::Yes) {
        goto scan_files;
    }

    showScreen(QMplayer::ScreenInit);
}

bool QMplayer::isPlaylist(QString fileName)
{
    return (fileName.endsWith(".m3u", Qt::CaseInsensitive)
            || fileName.endsWith(".pls", Qt::CaseInsensitive));
}

int QMplayer::scanDir(QString const &path, int level, int maxLevel, int min,
                      int max, bool followSymLinks)
{
    if (abort) {
        return 0;
    }

    QDir dir(path);
    QFileInfoList list = dir.entryInfoList(QDir::AllEntries, QDir::Name);

    int found = 0;
    int index = lw->count();
    for (int i = 0; i < list.count(); i++) {
        QFileInfo fi = list.at(i);
        if (!fi.isFile()) {
            continue;
        }
        QString fileName = fi.fileName();
        if (fileName.endsWith(".mp3", Qt::CaseInsensitive)
            || fileName.endsWith(".ogg", Qt::CaseInsensitive)
            || fileName.endsWith(".ogv", Qt::CaseInsensitive)
	    || fileName.endsWith(".mkv", Qt::CaseInsensitive)
	    || fileName.endsWith(".webm", Qt::CaseInsensitive)
            || fileName.endsWith(".avi", Qt::CaseInsensitive)
            || fileName.endsWith(".mp4", Qt::CaseInsensitive)
            || fileName.endsWith(".wav", Qt::CaseInsensitive)
            || fileName.endsWith(".flv", Qt::CaseInsensitive)
            || fileName.endsWith(".flac", Qt::CaseInsensitive)
            || isPlaylist(fileName)) {
            if (fileName.contains(".qmplayer.")) {
                if (delTmpFiles < 0) {
                    delTmpFiles =
                        (QMessageBox::question(this, "qmplayer",
                                               tr
                                               ("Delete qmplayer temporary files?"),
                                               QMessageBox::
                                               Yes | QMessageBox::No) ==
                         QMessageBox::Yes);
                }
                if (delTmpFiles) {
                    QFile::remove(fi.absoluteFilePath());
                }
                continue;
            }
            lw->addItem(fileName);
            found++;
        }
    }

    if (found) {
        lw->insertItem(index, getDirItem(lw, path));
    }

    if (level >= maxLevel) {
        return found;
    }

    for (int i = 0; i < list.count(); i++) {
        QFileInfo fi = list.at(i);
        if (fi.isFile() || fi.fileName() == "." || fi.fileName() == ".." ||
            (!followSymLinks && fi.isSymLink())) {
            continue;
        }

        int unit = (max - min) / list.count();
        int value = min + i * unit;
        if (progress->value() == value) {
            value++;
        }
        progress->setValue(value);
        label->setText(fi.absolutePath());
        QApplication::processEvents();

        found +=
            scanDir(fi.filePath(), level + 1, maxLevel, value, value + unit,
                    true);
    }
    return found;
}

QString QMplayer::getEncFilename(QString srcFile, QString dstIdentifier)
{
    QString res(srcFile);
    if (srcFile.endsWith(".avi") ||
        srcFile.endsWith(".mp4") || srcFile.endsWith(".flv")) {
        res.insert(srcFile.lastIndexOf('.'), dstIdentifier);
    }
    return res;
}

bool QMplayer::startMencoder(QString srcFile, QString dstFile)
{
    QStringList args;
    args.append(srcFile);
    args.append("-ovc");
    args.append("lavc");
    args.append("-lavcopts");
    args.append("vcodec=mpeg4:vhq:vbitrate=300:acodec=ac3");
    args.append("-vf");
#ifdef QT_QWS_NEO
    args.append("scale=320:240,eq2=1.2:0.5:-0.25,rotate=2");
#else
    args.append("scale=640:480,eq2=1.2:0.5:-0.25,rotate=2");
#endif
    args.append("-oac");
    args.append("lavc");
#ifdef QT_QWS_NEO
    args.append("-ofps");
    args.append("15");
#endif
    args.append("-o");
    args.append(dstFile);

    process = new QProcess(this);
    connect(process, SIGNAL(readyRead()), this, SLOT(mencoderReadyRead()));
    connect(process, SIGNAL(finished(int, QProcess::ExitStatus)), this,
            SLOT(processFinished(int, QProcess::ExitStatus)));

    process->setProcessChannelMode(QProcess::MergedChannels);
    process->start("mencoder", args);

#ifdef Q_WS_WIN
    PROCESS_INFORMATION *pi = (PROCESS_INFORMATION *) process->pid();
    SetPriorityClass(pi->hProcess, IDLE_PRIORITY_CLASS);
#endif

    if (process->waitForStarted(10000)) {
        return true;
    }
    delete(process);
    process = NULL;

    if (QMessageBox::question(this, "qmplayer", tr("Install mencoder?"),
                              QMessageBox::Yes | QMessageBox::No) ==
        QMessageBox::No) {
        return false;
    }
    QProcess::execute("raptor", QStringList() << "-u" << "-i" << "mencoder");

    return startMencoder(srcFile, dstFile);
}

void QMplayer::mencoderReadyRead()
{
    QString txt = process->readAll().trimmed();
    label->setText(txt);
    qDebug() << txt;

    // Search for percents - e.g. (34%)
    int index = txt.indexOf("%)");
    if (index < 5) {
        return;
    }
    int startIndex = txt.indexOf('(', index - 5) + 1;
    if (startIndex <= 0) {
        return;
    }
    QString numStr = txt.mid(startIndex, index - startIndex);
    progress->setValue(numStr.toInt());
}

bool QMplayer::runServer()
{
    if (tcpServer != NULL) {
        return false;
    }
    if (QMessageBox::question(this, "qmplayer", tr("Start server?"),
                              QMessageBox::Yes | QMessageBox::No) ==
        QMessageBox::No) {
        return false;
    }

    tcpServer = new QTcpServer(this);

    if (!tcpServer->listen(QHostAddress::Any, 7654)) {
        QMessageBox::critical(this, tr("qmplayer"),
                              tr("Unable to start the server on port 7654: ") +
                              tcpServer->errorString());
        delete(tcpServer);
        tcpServer = NULL;
        return true;
    }

    connect(tcpServer, SIGNAL(newConnection()), this, SLOT(newConnection()));

    QMessageBox::information(this, tr("qmplayer"),
                             tr("Web server is running on port 7654"));

    sharingItem->setText(tr("Web server running on port 7654"));
    return true;
}

bool QMplayer::runClient()
{
    if (tcpServer != NULL) {
        QMessageBox::critical(this, tr("qmplayer"),
                              tr
                              ("Point your web browser to http://localhost:7654 (replace localhost with this computer IP address)"));
        return true;
    }

    if (screen != QMplayer::ScreenConnect) {
        lineEdit->setText("192.168.0.200:7654");
        showScreen(QMplayer::ScreenConnect);
        return true;
    }
    showScreen(QMplayer::ScreenInit);

    QString host = lineEdit->text();
    int port = 80;
    int colonIndex = host.indexOf(':');
    if (colonIndex > 0) {
        port = host.right(host.length() - colonIndex - 1).toInt(0, 10);
        host = host.remove(colonIndex, host.length() - colonIndex);
    }

    QTcpSocket sock(this);
    sock.connectToHost(host, port);
    if (!sock.waitForConnected(5000)) {
        QMessageBox::critical(this, tr("qmplayer"), sock.errorString());
        return true;
    }

    QByteArray req("GET / HTTP/1.1\r\nHost: ");
    req.append(host);
    req.append(':');
    req.append(QByteArray::number(port));
    req.append("\r\n");

    sock.write(req);
    sock.flush();
    sock.waitForBytesWritten();

    QByteArray res;
    while (sock.waitForReadyRead(5000)) {
        res += sock.readAll();
    }
    sock.close();

    if (res.length() == 0) {
        QMessageBox::critical(this, tr("qmplayer"),
                              tr("No response from ") + host);
        return true;
    }

    delItems(lw);

    QTextStream buf(&res);
    buf.setCodec(QTextCodec::codecForName("utf8"));
    QString line;
    QString dir;

    for (;;) {
        line = buf.readLine();
        if (line.isNull()) {
            break;
        }
        if (line.indexOf("<a href=\"http://") == 0) {
            int urlEnd = line.indexOf('"', 17);
            if (urlEnd < 0) {
                continue;
            }
            QString url = line.mid(9, urlEnd - 9);
            int start = url.lastIndexOf('/');
            if (start < 0) {
                continue;
            }
            for (int i = start - 1; i >= 0; i--) {
                if (dir.length() <= i || dir.at(i) != url.at(i)) {
                    dir = url.left(start);
                    lw->addItem(getDirItem(lw, dir));
                    break;
                }
            }
            int fileStart = line.indexOf('>', 17) + 1;
            int fileEnd = line.indexOf('<', 17);
            if (fileStart <= 17 || fileEnd <= 17 || fileEnd < fileStart) {
                continue;
            }
            QString file = line.mid(fileStart, fileEnd - fileStart);
            lw->addItem(file);
        }
    }

    return true;
}

void QMplayer::sharing()
{
#ifdef QTOPIA
    if (!runClient()) {
        runServer();
    }
#else
    if (!runServer()) {
        runClient();
    }
#endif
}

void QMplayer::newConnection()
{
    QTcpSocket *con = tcpServer->nextPendingConnection();
    connect(con, SIGNAL(disconnected()), con, SLOT(deleteLater()));

    QByteArray req;
    while (con->waitForReadyRead(100)) {
        req += con->readAll();
    }

    QString res;
    QTextStream buf(&req);
    buf.setCodec(QTextCodec::codecForName("utf8"));
    QString line;
    QString reqPath = "";       // path from http request
    QString filename = "";      // file on the disk
    QString encPath = "";       // transcoded file
    QString host = "";
    QFile file;
    bool sendFile = false;

    for (;;) {
        line = buf.readLine();
        if (line.isNull()) {
            break;
        }
        if (line.startsWith("GET ")) {
            int index = req.indexOf(' ', 4);
            if (index > 0) {
                reqPath = req.mid(4, index - 4);
                for (int i = 0; i < reqPath.count(); i++) {
                    if (reqPath.at(i) == '%') {
                        i += 2;
                        if (i >= reqPath.count()) {
                            break;
                        }
                        QChar ch = (QChar) (reqPath.mid(i - 1, 2).toInt(0, 16));
                        reqPath.replace(i - 2, 3, ch);
                    }
                }
            }
        } else if (line.startsWith("Host: ")) {
            host = line.right(line.length() - 6).trimmed();
        }
    }

    res.append("HTTP/1.0 200 OK\r\n");
    res.append("Content-Type: text/html; charset=utf-8\r\n\r\n");
    res.append("<html>");
    res.append("<body>");

    if (reqPath.length() == 0) {
        res.append("Valid GET request not found");
    } else if (host.length() == 0) {
        res.append("Host not found in html header");
    } else if (reqPath == "/") {
        QString dir = "";
        for (int i = NUM_MENU_ITEMS; i < lw->count(); i++) {
            QListWidgetItem *item = lw->item(i);
            QString path = item->text();
            bool isDir = isDirectory(path);
            if (isDir) {
                res.append(path);
                res.append("</br>");
                dir = pathToUrl(path);
            } else {
                res.append("\r\n<a href=\"http://");
                res.append(host);
                res.append(dir);
                res.append(pathToUrl(path));
                res.append("\">");
                res.append(path);
                res.append("</a></br>\r\n");
            }
        }
    } else {
        res.append(reqPath);
        res.append("</br>");
        QString dir = "";
        QString testDir;
        for (int i = NUM_MENU_ITEMS; i < lw->count(); i++) {
            QListWidgetItem *item = lw->item(i);
            QString path = item->text();
            QString testPath = pathToUrl(path);
            bool isDir = isDirectory(path);
            if (isDir) {
                dir = path;
                testDir = testPath;
            } else if (reqPath == (testDir + testPath)) {
                filename = dir;
                if (!filename.endsWith('/') && !filename.endsWith('\\')) {
                    filename.append('/');
                }
                filename.append(path);
                break;
            }
        }
        if (filename.length() == 0) {
            res.append("file not found");
        } else {
            encPath = getEncFilename(filename, ".qmplayer");
            QFileInfo fi(encPath);
            if (fi.exists()) {
                QString size;
                size.setNum(fi.size());
                res.append("file size: ");
                res.append(size);

                if (processRunning(process)) {
                    res.append
                        ("</br>encoding in progress</br>Reload page to check status or download");
                } else {
                    QString mime = "text/plain";
                    if (reqPath.endsWith(".avi")) {
                        mime = "video/avi";
                    } else if (reqPath.endsWith(".mp4")) {
                        mime = "video/mp4";
                    } else if (reqPath.endsWith(".ogv")) {
                        mime = "video/theora";
                    } else if (reqPath.endsWith(".mkv")) {
                        mime = "video/x-matroska";
                    } else if (reqPath.endsWith(".webm")) {
                        mime = "video/webm";
                    } else if (reqPath.endsWith(".ogg")) {
                        mime = "audio/vorbis";
                    } else if (reqPath.endsWith(".mp3")) {
                        mime = "audio/mpeg";
                    } else if (reqPath.endsWith(".flac")) {
                        mime = "audio/flac";
                    }

                    res.clear();
                    res.append("HTTP/1.0 200 OK\r\n");
                    res.append("Content-Type: ");
                    res.append(mime);
                    res.append("\r\nContent-Length: ");
                    res.append(size);
                    res.append("\r\n\r\n");

                    sendFile = true;
                }
            } else if (processRunning(process)) {
                res.append("Another encoding is in progress");
            } else {
                if (startMencoder(filename, encPath)) {
                    // FIXME: whatever we send to back, browser wont show anything
                    res.append
                        ("Encoding started</br>Reload page to check status or download");
                } else {
                    res.append("Failed to start mencoder:</br>");
                    res.append(process->errorString());
                }
            }
        }
    }

    if (!sendFile) {
        res.append("</body>");
        res.append("</html>");
    }
    con->write(res.toUtf8());

    if (sendFile) {
        QFile f(encPath);
        char buf[4096];
        f.open(QFile::ReadOnly);
        int count;
        while ((count = f.read(buf, 4096)) > 0) {
            con->write(buf, count);
            con->flush();
            con->waitForBytesWritten(-1);
        }
    }

    con->close();
    con->disconnectFromHost();
}

void QMplayer::play(QStringList & args)
{
    if (useBluetooth < 0) {
        QFile f("/home/root/.asoundrc");
        if (f.exists() && f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QByteArray text = f.readAll();
            f.close();
            useBluetooth = ((text.indexOf("pcm.bluetooth") >= 0) &&
                            QMessageBox::question(this, tr("qmplayer"),
                                                  tr("Use bluetooth headset?"),
                                                  QMessageBox::Yes,
                                                  QMessageBox::No) ==
                            QMessageBox::Yes);
        }
    }
    if (useBluetooth > 0) {
        args.insert(0, "alsa:device=bluetooth");
        args.insert(0, "-ao");
    }

PLAY:

    showScreen(QMplayer::ScreenFullscreen);

    process = new QProcess(this);
    connect(process, SIGNAL(finished(int, QProcess::ExitStatus)), this,
            SLOT(processFinished(int, QProcess::ExitStatus)));
    //process->setProcessChannelMode(QProcess::ForwardedChannels);
    process->start("mplayer", args, QIODevice::ReadWrite);

    if (process->waitForStarted(5000)) {
        setpriority(PRIO_PROCESS, process->pid(), -5);
        return;
    }

    delete(process);
    process = NULL;
    showScreen(QMplayer::ScreenInit);

    if (QMessageBox::
        question(this, tr("qmplayer"),
                 tr
                 ("Program MPlayer must be downloaded. Please make sure you have internet connection and press yes to confirm download"),
                 QMessageBox::Yes, QMessageBox::No) == QMessageBox::No) {
        return;
    }
    showScreen(QMplayer::ScreenDownload);
    if (installMplayer()) {
        QMessageBox::information(this, tr("qmplayer"),
                                 tr("MPlayer installed sucessfully"));
        goto PLAY;
    }
    QMessageBox::warning(this, tr("qmplayer"), tr("Failed to install MPlayer"));
    showScreen(QMplayer::ScreenInit);
}

void QMplayer::encode(QString filename)
{
    QString dstFile = getEncFilename(filename, ".encoded.mp4");
    if (startMencoder(filename, dstFile)) {
#ifdef QTOPIA
        QtopiaApplication::setPowerConstraint(QtopiaApplication::
                                              DisableSuspend);
#endif
        showScreen(QMplayer::ScreenEncodingInProgress);
        QListWidgetItem *item = new QListWidgetItem(dstFile, lw);
        item->setSelected(true);
    } else {
        QMessageBox::warning(this, tr("qmplayer"),
                             tr("Failed to start mencoder"));
    }
}

void QMplayer::processFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitCode);
    Q_UNUSED(exitStatus);

    if (screen == ScreenEncodingInProgress) {
        delete(process);
        process = NULL;
        showScreen(ScreenInit);
        return;
    }

    QTimer::singleShot(100, &fsRefresh, SLOT(showScreen()));    // refresh taskbar and softmenu bar
    QTimer::singleShot(200, &fsRefresh, SLOT(hide()));

    if (mpgui) {
        showScreen(QMplayer::ScreenInit);
    } else if (tube && !ufinished)
        showScreen(QMplayer::ScreenTube);
    else
        mainWin->close();
}

void QMplayer::setRes(int xy)
{
#ifdef QT_QWS_NEO
    if (xy == 320240 || xy == 640480) {
        QFile f("/sys/class/lcd/jbt6k74-lcd/device/resolution");
        f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate);
        if (xy == 320240) {
            QProcess p(this);
            p.start("fbset", QStringList("qvga"));
            p.waitForFinished(5000);
            f.write("qvga");
        } else if (xy == 640480) {
            QProcess p(this);
            p.start("fbset", QStringList("vga"));
            p.waitForFinished(5000);
            f.write("vga");
        }
        f.close();
    }
#else
    Q_UNUSED(xy);
#endif
}

bool QMplayer::installMplayer()
{
#ifdef QT_QWS_NEO
    if (QMessageBox::
        question(this, tr("qmplayer"),
                 tr
                 ("Install glamo mplayer (YES) or distribution mplayer (NO)?"),
                 QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes) {
        QDir("/home/root").mkdir(".mplayer");
        QFile f("/home/root/.mplayer/config");
        f.open(QFile::WriteOnly);
        f.write("vo=glamo\n\n[default]\nafm=ffmpeg\nvfm=ffmpeg\n");
        f.close();

        return download("http://72.249.85.183/radekp/qmplayer/download/mplayer",
                        "/usr/bin/mplayer", "mplayer", false) &&
            QFile::setPermissions("/usr/bin/mplayer", QFile::ReadOwner |
                                  QFile::WriteOwner | QFile::ExeOwner |
                                  QFile::ReadUser | QFile::ExeUser |
                                  QFile::ReadGroup | QFile::ExeGroup |
                                  QFile::ReadOther | QFile::ExeOther);
    } else {
        QProcess::execute("raptor", QStringList() << "-u" << "-i" << "mplayer");

        QDir("/home/root").mkdir(".mplayer");
        QFile f("/home/root/.mplayer/config");
        f.open(QFile::WriteOnly);
        f.write("vo=fbdev\n\n[default]\nafm=ffmpeg\nvfm=ffmpeg\n");
        f.close();
    }
#elif QT_QWS_GTA04
    QProcess::execute("raptor", QStringList() << "-u" << "-i" << "mplayer");
    QDir("/home/root").mkdir(".mplayer");
    QFile f("/home/root/.mplayer/config");
    f.open(QFile::WriteOnly);
    f.write
        ("vo=fbdev2\nao=alsa\n[default]\nafm=ffmpeg\nvfm=ffmpeg\nvf=expand=640:480,rotate=1\nsws=0\nframedrop=1");
    f.close();
#else
    QMessageBox::critical(this, tr("qmplayer"), tr("You must install mplayer"));
    return false;
#endif
}

bool QMplayer::installYoutubeDl()
{
#ifdef QTOPIA
    return runCmd("apt-get update", 300) &&
        runCmd("apt-get -y install python", 200) &&
        runCmd
        ("wget -O /usr/bin/youtube-dl http://dl.linuxphone.ru/openmoko/qtmoko/packages/youtube-dl",
         100)
        && QFile::setPermissions("/usr/bin/youtube-dl",
                                 QFile::ReadOwner | QFile::WriteOwner | QFile::
                                 ExeOwner | QFile::ReadUser | QFile::
                                 ExeUser | QFile::ReadGroup | QFile::
                                 ExeGroup | QFile::ReadOther | QFile::ExeOther);
#else
    QMessageBox::critical(this, tr("qmplayer"),
                          tr("You must install youtube-dl"));
    return false;
#endif
}

void QMplayer::removeMplayer()
{
#ifdef QTOPIA
    if (QMessageBox::
        question(this, tr("qmplayer"),
                 tr("You are about to remove mplayer. Are you sure?"),
                 QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes) {
        bool res = runCmd("rm /usr/bin/mplayer", 10)
            && runCmd("rm /home/root/.mplayer/config", 10);
        runCmd("apt-get remove mplayer");
        showScreen(QMplayer::ScreenInit);
        if (res)
            QMessageBox::information(this, tr("qmplayer"),
                                     tr("mplayer has been removed"));
        else
            QMessageBox::critical(this, tr("qmplayer"),
                                  tr("Can't remove mplayer"));
    }
#endif
}

void QMplayer::removeYoutubeDl()
{
#ifdef QTOPIA
    if (QMessageBox::question(this, tr("qmplayer"),
                              tr
                              ("You are about to remove youtube-dl. Are you sure?"),
                              QMessageBox::Yes,
                              QMessageBox::No) == QMessageBox::Yes) {
        bool res = runCmd("rm /usr/bin/youtube-dl", 10);
        showScreen(QMplayer::ScreenInit);
        if (res)
            QMessageBox::information(this, tr("qmplayer"),
                                     tr("youtube-dl has been removed"));
        else
            QMessageBox::critical(this, tr("qmplayer"),
                                  tr("Can't remove youtube-dl"));
    }
#endif
}

void QMplayer::removeFlv()
{
#ifdef QTOPIA
    if (QMessageBox::question(this, tr("qmplayer"),
                              tr
                              ("You are about to remove all FLV videos from directory %1. Are you sure?").
                              arg(ubasedir), QMessageBox::Yes,
                              QMessageBox::No) == QMessageBox::Yes) {
        abort = false;
        showScreen(QMplayer::ScreenCmd);
        QString rtitle = tr("Removing FLV files from %1").arg(ubasedir);
        label->setText(rtitle);
        progress->setMaximum(100);
        progress->setValue(0);

        QDir dir(ubasedir);
        QFileInfoList list = dir.entryInfoList(QDir::AllEntries, QDir::Name);
        progress->setMaximum(list.count());
        for (int i = 0; i < list.count(); i++) {
            progress->setValue(i + 1);
            if (abort)
                break;

            QFileInfo fi = list.at(i);
            if (!fi.isFile()) {
                continue;
            }
            QString fileName = fi.fileName();
            if (fileName.endsWith(".flv", Qt::CaseInsensitive)) {
                QString fullfname = ubasedir + "/" + fileName;
                label->setText(rtitle + "\n" + fileName);
                QFile::remove(fullfname);
            }
        }
        showScreen(QMplayer::ScreenInit);

        if (!abort)
            QMessageBox::information(this, tr("qmplayer"),
                                     tr("FLV videos has been removed"));
        else
            QMessageBox::critical(this, tr("qmplayer"),
                                  tr("Operation aborted"));
    }
#endif
}

bool QMplayer::runCmd(QString cmd, int maxp)
{
    abort = false;
    bool inter = (maxp > 0);
    if (inter) {
        showScreen(QMplayer::ScreenCmd);
        label->setText(tr("Executing") + " '" + cmd + "'");
        progress->setMaximum(maxp);
        progress->setValue(0);
    }

    QProcess p;
    p.start(cmd);
    if (!p.waitForStarted()) {
        QMessageBox::critical(this, tr("qmplayer"),
                              tr("Unable to start") + " '" + cmd + "'");
        return false;
    }

    while ((p.state() == QProcess::Running) && (!abort)) {
        if (inter)
            progress->setValue((progress->value() + 1) % maxp);
        QApplication::processEvents();
        p.waitForFinished(100);
    }

    if (abort) {
        p.kill();
        return false;
    }

    return true;
}

bool QMplayer::youtubeDl()
{
    ufname = "";
    uload = 0;
    ufinished = false;

    QString cmd = "youtube-dl -t -c " + mplist[0];
    upr = new QProcess(this);
    connect(upr, SIGNAL(readyRead()), this, SLOT(uReadyRead()));
    connect(upr, SIGNAL(finished(int, QProcess::ExitStatus)), this,
            SLOT(uFinished(int, QProcess::ExitStatus)));
    upr->setWorkingDirectory(ubasedir);
    upr->start(cmd);
    if (!upr->waitForStarted()) {
        //QMessageBox::critical(this, tr("qmplayer"), tr("Unable to start") + " youtube-dl");
        return false;
    }
    return true;
}

void QMplayer::uReadyRead()
{
    QString txt = upr->readAll().trimmed();
    if (txt != "") {
        if (ufname == "") {
            int ext_i;
            QString ext_str;
            QString ext1_str = ".flv";
            QString ext2_str = ".mp4";
            ext_str = ext1_str;
            ext_i = txt.indexOf(ext_str);
            if (ext_i < 0) {
                ext_str = ext2_str;
                ext_i = txt.indexOf(ext_str);
            }
            if (ext_i >= 0) {
                int from_i;
                QString beg_str;
                QString beg1_str = "[download] Destination: ";
                QString beg2_str = " ";
                beg_str = beg1_str;
                from_i = txt.indexOf(beg_str);
                if (from_i < 0) {
                    beg_str = beg2_str;
                    from_i = txt.indexOf(beg_str);
                }
                if (from_i >= 0) {
                    from_i += beg_str.length();
                    int to_i = ext_i + ext_str.length();
                    ufname = txt.mid(from_i, to_i - from_i);
                    //if (txt.indexOf("has already been downloaded") >= 0) already=true;
                    mplist[0] = ubasedir + "/" + ufname;
                    if (screen == QMplayer::ScreenTube) {
                        setDlText();
                        bOk->setEnabled(true);
                    }
                }
            }
        } else {
            QString per_str = "% of ";
            int to_i = txt.indexOf(per_str);
            if (to_i >= 0) {
                int from_i = txt.indexOf(" ");
                if (from_i < to_i) {
                    QString load_str = txt.mid(from_i, to_i - from_i);
                    uload = load_str.toDouble();
                    if (screen == QMplayer::ScreenTube)
                        progress->setValue((int)uload);
                }
            }
        }                       //if
    }                           //if (txt != "")
}

void QMplayer::uFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitCode);
    Q_UNUSED(exitStatus);

    ufinished = true;
    delete upr;
    if (ufname == "") {
        QMessageBox::critical(this, tr("qmplayer"),
                              tr
                              ("youtube-dl has not returned name of downloaded file"));
        mainWin->close();
    } else {
        if (screen == QMplayer::ScreenTube) {
            screen = QMplayer::ScreenInit;
            okClicked();
        }
    }
}

void QMplayer::setDlText()
{
    label->setText(tr("Downloading video from Youtube") + "\n" + ufname);
}

QMplayerFullscreen::QMplayerFullscreen():QWidget()
    , clicked(false)
{
}

void QMplayerFullscreen::showScreen()
{
    clicked = false;
    showMaximized();
    enterFullScreen();
}

bool QMplayerFullscreen::event(QEvent * event)
{
#ifdef QTOPIA
    if (event->type() == QEvent::WindowDeactivate) {
        if (clicked) {
            pixmap = QPixmap::grabWindow(QApplication::desktop()->winId());
        }
        lower();
        emit deactivated();
    } else if (event->type() == QEvent::WindowActivate) {
        QString title = windowTitle();
        setWindowTitle(QLatin1String("_allow_on_top_"));
        raise();
        setWindowTitle(title);
    }
#endif
    return QWidget::event(event);
}

void QMplayerFullscreen::enterFullScreen()
{
#ifdef QTOPIA
    // Show editor view in full screen
    setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
    setWindowState(Qt::WindowFullScreen);
    raise();
#endif
}

QMplayerFullscreenPlay::QMplayerFullscreenPlay():QMplayerFullscreen()
  , downX(-1)
  , dim(0)
{
    radius = width() > height() ? height() / 4 : width() / 4;
}

void QMplayerFullscreenPlay::paintEvent(QPaintEvent *)
{
    if(downX < 0)
        return;

    QPainter p(this);

#ifdef QT_QWS_NEO
    p.scale(0.5, 0.5);
#endif

    // Circle for unlocking area
    QPoint center = this->rect().center();
    p.drawEllipse(center, radius, radius);

    p.drawText(this->rect(), Qt::AlignCenter,
                tr("pause\n\n\n\n\n\nvol down                vol up\n\n\n\n\n\ndim"));


    // Mouse pointer
    QPoint mP(center.x() + lastX - downX, center.y() + lastY - downY);
    p.setBrush(Qt::white);
    p.setPen( Qt::white);
    p.drawEllipse(mP, radius / 4, radius / 4);
}

void QMplayerFullscreenPlay::adjustVolume()
{
    int deltaX = lastX - downX;
    if(deltaX > radius)
        emit volumeUp();
    else if(-deltaX > radius)
        emit volumeDown();
    else {
        adjustingVolume = false;
        return;
    }
    adjustingVolume = true;
    QTimer::singleShot(200, this, SLOT(adjustVolume()));
}

void QMplayerFullscreenPlay::mousePressEvent(QMouseEvent * e)
{
    downX = lastX = e->x();
    downY = e->y();
    adjustingVolume = false;
    dim = (dim > 0);
    update();
}

void QMplayerFullscreenPlay::mouseReleaseEvent(QMouseEvent *)
{
    downX = lastX = -1;
    update();
}

void QMplayerFullscreenPlay::mouseMoveEvent(QMouseEvent * e)
{
    int deltaX = abs(e->x() - downX);
    int deltaY = e->y() - downY;

    lastX = e->x();
    lastY = e->y();

    if(deltaY > radius || deltaY < -radius)
    {
        if(dim == 0 || dim == 1)
        {
            if(deltaY < -radius) {       // slide up
                dim = 1;                 // undim
                hide();                  // leave fullscreen
            }

#ifdef QTOPIA
            QtopiaServiceRequest e1("QtopiaPowerManager", "setBacklight(int)");
            e1 << (dim ? -1 : 0);
            e1.send();
#endif
            dim = (dim > 0 ? -2 : 2);
        }
    }
    else if(deltaX > radius && !adjustingVolume)
        adjustVolume();

    if(!adjustingVolume)
        update();
}

QMplayerMainWindow::QMplayerMainWindow(QWidget * parent, Qt::WFlags f)
:  QMainWindow(parent, f)
{
#ifdef QTOPIA
    this->setWindowState(Qt::WindowMaximized);
#else
    resize(640, 480);
#endif
    Q_UNUSED(f);

    mainWin = this;
    setCentralWidget(new QMplayer(this));
}

QMplayerMainWindow::~QMplayerMainWindow()
{

}
