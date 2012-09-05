#include "neocontrol.h"

NeoControl::NeoControl(QWidget *parent, Qt::WFlags f)
    : QWidget(parent)
{
#ifdef QTOPIA
    this->setWindowState(Qt::WindowMaximized);
#else
    Q_UNUSED(f);
#endif
    bQvga = new QPushButton(tr("Switch to QVGA"), this);
    connect(bQvga, SIGNAL(clicked()), this, SLOT(qvgaClicked()));

    bBack = new QPushButton(this);
    connect(bBack, SIGNAL(clicked()), this, SLOT(backClicked()));

    bNext = new QPushButton(tr("Next"), this);
    connect(bNext, SIGNAL(clicked()), this, SLOT(nextClicked()));

    bSave = new QPushButton(tr("Save"), this);
    connect(bSave, SIGNAL(clicked()), this, SLOT(saveClicked()));

    chkDeepSleep = new QCheckBox(tr("Deep sleep"), this);
    connect(chkDeepSleep, SIGNAL(stateChanged(int)), this, SLOT(deepSleepStateChanged(int)));

    chkMux = new QCheckBox(tr("Multiplexing"), this);
    connect(chkMux, SIGNAL(stateChanged(int)), this, SLOT(muxStateChanged(int)));

    chkFso = new QCheckBox(tr("Use FSO (freesmartphone.org)"), this);
    connect(chkFso, SIGNAL(stateChanged(int)), this, SLOT(fsoStateChanged(int)));

    label = new QLabel(this);
    lineEdit = new QLineEdit(this);

    label4 = new QLabel(this);
    label5 = new QLabel(this);

    slider4 = new MixerSlider(this);
    slider5 = new MixerSlider(this);

    buttonLayout = new QHBoxLayout();
    buttonLayout->setAlignment(Qt::AlignBottom);
    buttonLayout->addWidget(bBack);
    buttonLayout->addWidget(bNext);

    layout = new QVBoxLayout(this);
    layout->addWidget(bQvga);
    layout->addWidget(label);
    layout->addWidget(label4);
    layout->addWidget(slider4);
    layout->addWidget(label5);
    layout->addWidget(slider5);
    layout->addWidget(bSave);
    layout->addWidget(lineEdit);
    layout->addWidget(chkDeepSleep);
    layout->addWidget(chkMux);
    layout->addWidget(chkFso);
    layout->addLayout(buttonLayout);

    showScreen(NeoControl::ScreenInit);
}

NeoControl::~NeoControl()
{

}

// Qtopia compat for Qt only PC version
#ifndef QTOPIA
namespace Qtopia
{
    QByteArray readFile(const char *path);
    QDateTime rtcNow();
}
QByteArray Qtopia::readFile(const char *path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning() << "readFile failed" << path << ":" << f.errorString();
        return QByteArray();
    }
    QByteArray content = f.readAll();
    f.close();
    return content;
}

QDateTime Qtopia::rtcNow()
{
    QByteArray secsStr = readFile("/sys/class/rtc/rtc0/since_epoch").trimmed();
    uint secs = secsStr.toUInt();
    return QDateTime::fromTime_t(secs);
}
#endif

void NeoControl::backClicked()
{
    switch(screen)
    {
    case ScreenInit:
        close();
        break;
    case ScreenRtc:
        showScreen(ScreenInit);
        break;
    case ScreenMixer:
        showScreen(ScreenRtc);
        break;
    case ScreenModem:
        showScreen(ScreenMixer);
        break;
    case ScreenSysfs:
        showScreen(ScreenModem);
        break;
    case ScreenDisplay:
        showScreen(ScreenSysfs);
        break;
    }
}

void NeoControl::nextClicked()
{
    switch(screen)
    {
    case ScreenInit:
        showScreen(ScreenRtc);
        break;
    case ScreenRtc:
        showScreen(ScreenMixer);
        break;
    case ScreenMixer:
        showScreen(ScreenModem);
        break;
    case ScreenModem:
        showScreen(ScreenSysfs);
        break;
    case ScreenSysfs:
        showScreen(ScreenDisplay);
        break;
    case ScreenDisplay:
        break;
    }
}

void NeoControl::saveClicked()
{
    if(screen == ScreenMixer)
    {
        system("alsactl -f /opt/qtmoko/etc/alsa-scenarios/gsmhandset.state store");
    }
}

void NeoControl::qvgaClicked()
{
    QFile cal("/etc/pointercal_qvga");
    if(!cal.exists())
    {
        cal.open(QFile::WriteOnly);
        cal.write("-360 16920 -2227050 -21726 -170 20270704 56321");
        cal.close();
    }

    QFile f("/tmp/restart-qtopia-qvga");
    f.open(QFile::WriteOnly);
    f.write("restart in qvga");
    f.close();
    QMessageBox::information(this, tr("QVGA setup"), tr("QVGA mode will be activated after restarting QtExtended with POWER button"));
}

void NeoControl::showScreen(NeoControl::Screen scr)
{
    if(scr == ScreenMixer)
    {
        openAlsaMixer();
    }
    if(this->screen == ScreenMixer)
    {
        closeAlsaMixer();
    }

    this->screen = scr;

    bQvga->setVisible(scr == ScreenDisplay);
    label->setVisible(scr == ScreenInit || scr == ScreenRtc || scr == ScreenMixer || scr == ScreenModem || scr == ScreenSysfs);
    bBack->setText(scr == ScreenInit ? tr("Quit") : tr("Back"));
    lineEdit->setVisible(false);
    chkDeepSleep->setVisible(scr == ScreenModem);
    chkMux->setVisible(scr == ScreenModem);
    chkFso->setVisible(scr == ScreenModem);
    label4->setVisible(scr == ScreenMixer);
    label5->setVisible(scr == ScreenMixer);
    slider4->setVisible(scr == ScreenMixer);
    slider5->setVisible(scr == ScreenMixer);
    bSave->setVisible(scr == ScreenMixer);

    switch(scr)
    {
    case ScreenInit:
        label->setText(tr("Neo hardware tool"));
        break;
    case ScreenRtc:
        updateRtc();
        break;
    case ScreenMixer:
        updateMixer();
        break;
    case ScreenModem:
        updateModem();
        break;
    case ScreenDisplay:
        break;
    case ScreenSysfs:
        updateSysfs();
        break;

    default:
        break;
    }
}

void NeoControl::updateRtc()
{
    if(screen != ScreenRtc)
        return;

    QDateTime rtcNow = Qtopia::rtcNow();
    QString rtcDate = Qtopia::readFile("/sys/class/rtc/rtc0/date").trimmed();
    QString rtcTime = Qtopia::readFile("/sys/class/rtc/rtc0/time").trimmed();

    QByteArray wakealarmStr = Qtopia::readFile("/sys/class/rtc/rtc0/wakealarm").trimmed();
    QString alarmStr;
    if(wakealarmStr.isEmpty()) {
        alarmStr = tr("not set");
    } else {
        uint wakealarmSecs = wakealarmStr.toUInt();
        QDateTime wakealarmDt = QDateTime::fromTime_t(wakealarmSecs);
        alarmStr = wakealarmDt.toString();
    }

    label->setText(QString(tr("RTC (Real time clock)\n\nDate: %1\nTime: %2\nLocal: %3\nAlarm: %4"))
                   .arg(rtcDate)
                   .arg(rtcTime)
                   .arg(rtcNow.toString())
                   .arg(alarmStr));

    QTimer::singleShot(1000, this, SLOT(updateRtc()));
}

int NeoControl::openAlsaMixer()
{
    system("alsactl -f /opt/qtmoko/etc/alsa-scenarios/gsmhandset.state restore");

    int ret = 0;
    QString text(tr("Call volume settings\n\n"));

    if ((ret = snd_mixer_open(&mixerFd, 0)) < 0) {
        text += QString("snd_mixer_open error %1").arg(ret);
        goto err;
    }
    if ((ret = snd_mixer_attach(mixerFd, "default")) < 0) {
        text += QString("snd_mixer_attach error %1").arg(ret);
        goto err;
    }
    if ((ret = snd_mixer_selem_register(mixerFd, NULL, NULL)) < 0) {
        text += QString("snd_mixer_selem_register error %1").arg(ret);
        goto err;
    }
    if ((ret = snd_mixer_load(mixerFd)) < 0) {
        text += QString("snd_mixer_load error %1").arg(ret);
        goto err;
    }

    goto ok;

    err:
    if (mixerFd)
        snd_mixer_close(mixerFd);
    mixerFd = NULL;
    ok:
    label->setText(text);

    return ret;
}

void NeoControl::closeAlsaMixer()
{
    if (mixerFd) {
        snd_mixer_detach(mixerFd, "default");
        snd_mixer_close(mixerFd);
        mixerFd = NULL;
    }

    system("alsactl -f /opt/qtmoko/etc/alsa-scenarios/stereoout.state restore");
}

void NeoControl::updateMixer()
{
    if(screen != ScreenMixer)
    {
        return;
    }
    if(slider4->sliding || slider5->sliding)
    {
        QTimer::singleShot(100, this, SLOT(updateMixer()));
        return;
    }

    snd_mixer_elem_t *elem;
    snd_mixer_elem_t *elem4 = NULL;
    snd_mixer_elem_t *elem5 = NULL;

    for (elem = snd_mixer_first_elem(mixerFd); elem;
    elem = snd_mixer_elem_next(elem)) {
        QString elemName = QString(snd_mixer_selem_get_name(elem));

        if(elemName == "Speaker")
        {
            elem4 = elem;
        }
        else if(elemName == "Mono Playback")
        {
            elem5 = elem;
        }
    }

    slider4->setMixerElem(elem4, true);
    slider5->setMixerElem(elem5, true);

    label4->setText(tr("Playback (control.4) %1").arg(slider4->volume));        // Mono Playback Volume
    label5->setText(tr("Microphone (control.5) %1").arg(slider5->volume));      // Mono Sidetone Playback Volume

    label->setText(tr("Call volume settings"));

    QTimer::singleShot(1000, this, SLOT(updateMixer()));
}


void NeoControl::deepSleepStateChanged(int state)
{
    if(updatingModem)
    {
        return;
    }
    QString val = (state == Qt::Checked ? "always" : "never");
    QSettings cfg("Trolltech", "Modem");
    cfg.setValue("DeepSleep/Active", val);
    cfg.sync();

    QMessageBox::information(this, tr("Deep sleep"), tr("Settings will be activated after restarting QtExtended with POWER button"));
}

void NeoControl::muxStateChanged(int state)
{
    if(updatingModem)
    {
        return;
    }
    QString val = (state == Qt::Checked ? "yes" : "no");
    QSettings cfg("Trolltech", "Modem");
    cfg.setValue("Multiplexing/Active", val);
    cfg.sync();

    QMessageBox::information(this, tr("Multiplexing"), tr("Settings will be activated after restarting QtExtended with POWER button"));
}

QString NeoControl::getQpeEnv()
{
    QFile f("/opt/qtmoko/qpe.env");
    if(!f.open(QFile::ReadOnly))
    {
        QMessageBox::critical(this, tr("FSO"), tr("Failed to read") + " " + f.fileName());
        return "";
    }
    QString content = f.readAll();
    f.close();
    return content;
}

void NeoControl::setQpeEnv(bool fso)
{
    QString content = getQpeEnv();
    QString fsoStr = "export QTOPIA_PHONE=Fso";
    QString atStr = "export QTOPIA_PHONE=AT";
    if(fso)
    {
        content = content.replace(atStr, fsoStr);
    }
    else
    {
        content = content.replace(fsoStr, atStr);
    }
    QFile f("/opt/qtmoko/qpe.env");
    if(!f.open(QFile::WriteOnly))
    {
        QMessageBox::critical(this, tr("FSO"), tr("Failed to write to") + " " + f.fileName());
        return;
    }
    f.write(content.toLatin1());
    f.close();
    QMessageBox::information(this, tr("FSO"), tr("You have to restart your phone for changes to take place"));
}

void NeoControl::fsoStateChanged(int)
{
    if(updatingModem)
    {
        return;
    }
    QTimer::singleShot(0, this, SLOT(fsoChange()));
}

void NeoControl::fsoChange()
{
    bool checked = chkFso->isChecked();
    if(!checked)
    {
        QProcess::execute("qterminal", QStringList() << "-c" << "update-rc.d" << "-f" << "fso-deviced" << "remove");
        setQpeEnv(false);     // disable FSO
        return;
    }
    if(!QFile::exists("/usr/sbin/fsogsmd"))
    {
        QMessageBox::information(this, tr("FSO"), tr("FSO packages have to be downloaded and installed. Please make sure you have internet connection now."));
        QProcess::execute("raptor", QStringList() << "-u" << "-i" << "fso-gsmd-openmoko" << "fso-usaged-openmoko");
        QMessageBox::information(this, tr("FSO"), tr("QtMoko needs very recent FSO, it will be downloaded from http://activationrecord.net/radekp/pub/libfsogsm.so.0.0.0"));
        QProcess::execute("qterminal", QStringList() << "-c" << "wget" << "http://activationrecord.net/radekp/pub/libfsogsm.so.0.0.0");
        QProcess::execute("qterminal", QStringList() << "-c" << "mv" << "libfsogsm.so.0.0.0" << "/usr/lib/cornucopia/libs/fsogsm/libfsogsm.so.0.0.0");
    }
    QProcess::execute("qterminal", QStringList() << "-c" << "update-rc.d" << "fso-deviced" << "defaults");
    setQpeEnv(true);
}

void NeoControl::updateModem()
{
    if(screen != ScreenModem)
    {
        return;
    }
    updatingModem = true;

    QString text(tr("Modem settings\n\n"));
    QSettings cfg("Trolltech", "Modem");

    QString deepSleep = cfg.value("DeepSleep/Active").toString();
    text += tr("Deep sleep") + ": " + deepSleep + "\n";
    chkDeepSleep->setChecked(deepSleep != "never");

    QString multiplexing = cfg.value("Multiplexing/Active", "yes").toString();
    text += tr("Multiplexing") + ": " + multiplexing;
    chkMux->setChecked(multiplexing != "no");

    label->setText(text);

    QString qpeEnv = getQpeEnv();
    QString fsoStr = "export QTOPIA_PHONE=Fso";
    chkFso->setChecked(qpeEnv.indexOf(fsoStr) >= 0);

    updatingModem = false;
    QTimer::singleShot(1000, this, SLOT(updateModem()));
}

static void appendValue(QString desc, QString file, QString *text)
{
    text->append(desc);
    text->append(": ");

    QFile f(file);
    if(!f.open(QFile::ReadOnly))
    {
        text->append("failed to open " + file + " " + f.errorString() + "\n");
    }
    else
    {
        text->append(f.readAll());
        f.close();
    }
}

void NeoControl::updateSysfs()
{
    if(screen != ScreenSysfs)
    {
        return;
    }

    QString text;
    appendValue(tr("Battery status"), "/sys/class/power_supply/battery/status", &text);
    appendValue(tr("Current"), "/sys/class/power_supply/battery/current_now", &text);
    appendValue(tr("Modem power"), "/sys/devices/platform/s3c2440-i2c/i2c-0/0-0073/pcf50633-gpio.0/reg-fixed-voltage.1/gta02-pm-gsm.0/power_on", &text);
    appendValue(tr("GPS power"), "/sys/devices/platform/gta02-pm-gps.0/power_on", &text);
    appendValue(tr("Bluetooth power"), "/sys/devices/platform/gta02-pm-bt.0/power_on", &text);
    label->setText(text);

    QTimer::singleShot(1000, this, SLOT(updateSysfs()));
}
