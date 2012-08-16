#ifndef QMPLAYER_H
#define QMPLAYER_H

#include <QWidget>
#include <QListWidget>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QProcess>
#include <QDir>
#include <QProgressDialog>
#include <QErrorMessage>
#include <QProgressBar>
#include <QApplication>
#include <QMouseEvent>
#include <QMessageBox>
#include <QHttp>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTextCodec>
#include <QLineEdit>
#include <QStringList>
#include <QTimer>
#include <QMenuBar>
#include <QMainWindow>
#include <QPainter>
#include <QDesktopWidget>
#ifdef QTOPIA
#include <QSoftMenuBar>
#include <QtopiaApplication>
#include <QtopiaServiceRequest>
#else
#include <QCoreApplication>
#endif
#ifdef Q_WS_WIN
#include <Windows.h>
#endif

#define NUM_MENU_ITEMS 3

// This is simple fullscreen widget used for refreshing softmenu bar and top
// bar. It's also base for the playing full screen widget
class QMplayerFullscreen : public QWidget
{
    Q_OBJECT
public:
    QMplayerFullscreen();
    bool clicked;
    QPixmap pixmap;

     signals:void deactivated();

public slots:
    void showScreen();

protected:
     bool event(QEvent *);
    void enterFullScreen();
};

// This is fullscreen dialog displayed when mplayer application is running. We need
// to avoid any Qtopia drawing when mplayer is running so that it's screen is not
// damaged by Qtopia redraws.
class QMplayerFullscreenPlay : public QMplayerFullscreen
{
    Q_OBJECT
public:
    QMplayerFullscreenPlay();

    signals: void pause();
    void volumeUp();
    void volumeDown();

protected:
    void paintEvent(QPaintEvent *);
    void mousePressEvent(QMouseEvent *);
    void mouseReleaseEvent(QMouseEvent *);
    void mouseMoveEvent(QMouseEvent *);

private slots:
    void adjustVolume();

private:
    int radius;
    int downX;
    int downY;
    int lastX;
    int lastY;
    int dim;                    // 1 backlight off, 0 backlight on, -2 backlight on (just undimmed by sliding), 2 screen off (just dimmed by sliding)
    bool adjustingVolume;
};

class QMplayer : public QWidget
{
    Q_OBJECT
public:
    QMplayer(QWidget * parent = 0, Qt::WFlags f = 0);
    ~QMplayer();

    enum Screen
    {
        ScreenInit,
        ScreenScan,
        ScreenFullscreen,
        ScreenPaused,
        ScreenDownload,
        ScreenConnect,
        ScreenTube,
        ScreenCmd,
        ScreenEncoding,
        ScreenEncodingInProgress,
    };

private:
     Screen screen;
    int maxScanLevel;
    int delTmpFiles;
    bool abort;
    int useBluetooth;
    QMplayerFullscreenPlay fsPlay;  // fulscreen used for playing
    QMplayerFullscreen fsRefresh;   // fullscreen used to refresh upper bar and softmenubar
    QVBoxLayout *layout;
    QHBoxLayout *buttonLayout;
    QListWidget *lw;
    QListWidgetItem *scanItem;
    QListWidgetItem *sharingItem;
    QListWidgetItem *encodingItem;
    QLabel *label;
    QLineEdit *lineEdit;
    QPushButton *bOk;
    QPushButton *bBack;
    QPushButton *bUp;
    QPushButton *bDown;
    QProcess *process;
    QProgressBar *progress;
    QTcpServer *tcpServer;
    QStringList mpargs;
    QStringList mplist;
#ifdef QTOPIA
    QMenu *softm;
    QAction *rmMpAction, *rmDlAction, *rmFlvAction;
#endif
    bool mpgui;
    bool tube;
    QString ubasedir;
    QString ufname;
    double uload;
    QProcess *upr;
    bool ufinished;

    void showScreen(QMplayer::Screen scr);
    void scan();
    int scanDir(QString const &path, int level, int maxLevel, int min, int max,
                bool followSymLinks);
    void sharing();
    void play(QStringList & args);
    void encode(QString filename);
    void setRes(int xy);
    bool installMplayer();
    bool installYoutubeDl();
    bool runServer();
    bool runClient();
    bool download(QString url, QString destPath, QString filename,
                  bool justCheck);
    bool startMencoder(QString srcFile, QString dstFile);
    void console(QString s);
    bool youtubeDl();
    bool runCmd(QString cmd, int maxp = 0);
    void setDlText();
    QString getEncFilename(QString srcFile, QString dstIdentifier);
    bool isPlaylist(QString fileName);

protected:
    void paintEvent(QPaintEvent *);
    void closeEvent(QCloseEvent * event);

private slots:
    void okClicked();
    void backClicked();
    void upClicked();
    void downClicked();
    void newConnection();
    void processFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void removeMplayer();
    void removeYoutubeDl();
    void removeFlv();
    void sTimerEvent();
    void uReadyRead();
    void mencoderReadyRead();
    void uFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void handleFsDeactivate();
    void stopMplayer();
    void finishPause();
    void volumeUp();
    void volumeDown();
};

class QMplayerMainWindow : public QMainWindow
{
public:
    QMplayerMainWindow(QWidget * parent = 0, Qt::WFlags f = 0);
    ~QMplayerMainWindow();
};

#endif                          // QMPLAYER_H
