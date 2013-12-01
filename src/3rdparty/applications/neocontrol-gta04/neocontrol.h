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

protected:
    void paintEvent(QPaintEvent *);

private:
    enum Screen
    {
        ScreenInit,
        ScreenRtc,
        ScreenMixer,
        ScreenModem,
        ScreenSysfs,
        ScreenCharge,
    };

    Screen screen;
    bool updatingScreen;
    snd_mixer_t *mixerFd;
    QVBoxLayout *layout;
    QHBoxLayout *buttonLayout;
    QLabel *label;
    QLineEdit *lineEdit;
    QPushButton *bBack;
    QPushButton *bNext;
    QPushButton *bSave;
    QCheckBox *chkFso;
    QCheckBox *chkCharge;
    QLabel *label4;
    QLabel *label5;
    MixerSlider *slider4;
    MixerSlider *slider5;
    QFont normalFont;
    QFont smallFont;
    QString chargeLog;

    void showScreen(NeoControl::Screen scr);
    int openAlsaMixer();
    void closeAlsaMixer();
    QString getQpeEnv();
    void setQpeEnv(bool);

private slots:
    void backClicked();
    void nextClicked();
    void saveClicked();
    void updateRtc();
    void updateMixer();
    void updateModem();
    void updateSysfs();
    void updateCharge();
    void fsoStateChanged(int);
    void chargeStateChanged(int);
    void fsoChange();
};

#endif // NEOCONTROL_H
