#ifndef NEOCONTROL_H
#define NEOCONTROL_H

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
#include <QTimer>
#include <QSettings>
#include <QCheckBox>
#include <QDateTime>
#ifdef QTOPIA
#include <Qtopia>
#include <QtopiaApplication>
#endif
#ifdef Q_WS_WIN
#include <Windows.h>
#endif
#include <alsa/asoundlib.h>
#include "mixerslider.h"

class NeoControl : public QWidget
{
    Q_OBJECT

public:
    NeoControl(QWidget *parent = 0, Qt::WFlags f = 0);
    ~NeoControl();

private:
    enum Screen
    {
        ScreenInit,
        ScreenRtc,
        ScreenMixer,
        ScreenModem,
        ScreenSysfs,
        ScreenDisplay,
    };

    Screen screen;
    bool updatingModem;
    snd_mixer_t *mixerFd;
    QVBoxLayout *layout;
    QHBoxLayout *buttonLayout;
    QLabel *label;
    QLineEdit *lineEdit;
    QPushButton *bQvga;
    QPushButton *bBack;
    QPushButton *bNext;
    QPushButton *bSave;
    QCheckBox *chkDeepSleep;
    QCheckBox *chkMux;
    QCheckBox *chkFso;
    QLabel *label4;
    QLabel *label5;
    MixerSlider *slider4;
    MixerSlider *slider5;

    void showScreen(NeoControl::Screen scr);
    int openAlsaMixer();
    void closeAlsaMixer();
    QString getQpeEnv();
    void setQpeEnv(bool);

private slots:
    void qvgaClicked();
    void backClicked();
    void nextClicked();
    void saveClicked();
    void updateRtc();
    void updateMixer();
    void updateModem();
    void updateSysfs();
    void deepSleepStateChanged(int);
    void muxStateChanged(int);
    void fsoStateChanged(int);
    void fsoChange();
};

#endif // NEOCONTROL_H
