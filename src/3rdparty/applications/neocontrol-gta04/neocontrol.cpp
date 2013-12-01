#include "neocontrol.h"
#include <QPainter>

NeoControl::NeoControl(QWidget * parent, Qt::WFlags f)
:  QWidget(parent)
{
#ifdef QTOPIA
    this->setWindowState(Qt::WindowMaximized);
#else
    Q_UNUSED(f);
#endif
    bBack = new QPushButton(this);
    connect(bBack, SIGNAL(clicked()), this, SLOT(backClicked()));

    bNext = new QPushButton(tr("Next"), this);
    connect(bNext, SIGNAL(clicked()), this, SLOT(nextClicked()));

    bSave = new QPushButton(tr("Save"), this);
    connect(bSave, SIGNAL(clicked()), this, SLOT(saveClicked()));

    chkFso = new QCheckBox(tr("Use FSO (freesmartphone.org)"), this);
    connect(chkFso, SIGNAL(stateChanged(int)), this,
            SLOT(fsoStateChanged(int)));

    chkCharge = new QCheckBox(tr("Log charging"), this);
    connect(chkCharge, SIGNAL(stateChanged(int)), this,
            SLOT(chargeStateChanged(int)));

    label = new QLabel(this);
    normalFont = label->font();
    smallFont = QFont(normalFont.family(), (3 * normalFont.pointSize()) / 5);
    lineEdit = new QLineEdit(this);

    label4 = new QLabel(this);
    label5 = new QLabel(this);

    slider4 = new MixerSlider(this);
    slider5 = new MixerSlider(this);

    buttonLayout = new QHBoxLayout();
    buttonLayout->setAlignment(Qt::AlignBottom);
    buttonLayout->addWidget(bBack);
    buttonLayout->addWidget(bNext);
    buttonLayout->addWidget(chkCharge);

    layout = new QVBoxLayout(this);
    layout->addWidget(label);
    layout->addWidget(label4);
    layout->addWidget(slider4);
    layout->addWidget(label5);
    layout->addWidget(slider5);
    layout->addWidget(lineEdit);
    layout->addWidget(bSave);
    layout->addWidget(chkFso);
    layout->addLayout(buttonLayout);

    showScreen(NeoControl::ScreenCharge);
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
    switch (screen) {
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
    case ScreenCharge:
        showScreen(ScreenSysfs);
        break;
    }
}

void NeoControl::nextClicked()
{
    switch (screen) {
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
        showScreen(ScreenCharge);
        break;
    case ScreenCharge:
        break;
    }
}

void NeoControl::saveClicked()
{
    if (screen == ScreenMixer) {
        system
            ("alsactl -f /opt/qtmoko/etc/alsa/gta04_initial_alsa.state store");
    }
    if (screen == ScreenModem) {
        QSettings cfg("Trolltech", "Modem");
        cfg.setValue("OPSYS/Value", lineEdit->text());
        cfg.sync();
        QMessageBox::information(this, tr("Modem settings"),
                                 tr
                                 ("Settings will be activated after restarting QtMoko with POWER button"));
    }
    if (screen == ScreenCharge) {
        QSettings cfg("Trolltech", "qpe");
        cfg.setValue("Charging/LogInterval", lineEdit->text().toInt());
        cfg.sync();
        QMessageBox::information(this, tr("Log settings"),
                                 tr
                                 ("Settings will be activated after restarting QtMoko with POWER button"));
    }
}

void NeoControl::showScreen(NeoControl::Screen scr)
{
    if (scr == ScreenMixer) {
        openAlsaMixer();
    }
    if (this->screen == ScreenMixer) {
        closeAlsaMixer();
    }
    if (this->screen == ScreenSysfs) {
        label->setFont(normalFont);
    }
    if (scr == ScreenSysfs) {
        label->setFont(smallFont);
    }
    if (this->screen == ScreenCharge) {
        setFont(normalFont);
    }
    if (scr == ScreenCharge) {
        setFont(smallFont);
    }

    this->screen = scr;

    label->setVisible(scr == ScreenInit || scr == ScreenRtc
                      || scr == ScreenMixer || scr == ScreenModem
                      || scr == ScreenSysfs);
    bBack->setText(scr == ScreenInit ? tr("Quit") : tr("Back"));
    lineEdit->setVisible(scr == ScreenModem);
    chkFso->setVisible(scr == ScreenModem);
    chkCharge->setVisible(scr == ScreenCharge);
    label4->setVisible(scr == ScreenMixer);
    label5->setVisible(scr == ScreenMixer);
    slider4->setVisible(scr == ScreenMixer);
    slider5->setVisible(scr == ScreenMixer);
    bSave->setVisible(scr == ScreenMixer || scr == ScreenModem);

    switch (scr) {
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
    case ScreenCharge:
        updateCharge();
        break;
    case ScreenSysfs:
        updateSysfs();
        break;

    default:
        break;
    }
}

static int computeCurrent(int secs, int chargeBefore, int chargeAfter)
{
    if (secs == 0)
        return 0;
    return ((chargeBefore - chargeAfter) * 36) / (10 * secs);
}

void NeoControl::paintEvent(QPaintEvent *)
{
    if (screen != ScreenCharge) {
        return;
    }

    QList < QString > lines = chargeLog.split('\n');
    if (lines.count() < 2)
        return;

    QList < QDateTime > dates;
    QList < int >charges;

    QDateTime dtMin(QDate(2999, 1, 1));
    QDateTime dtMax(QDate(1899, 1, 1));
    int chargeMax = 0;

    for (int i = 0; i < lines.count(); i++) {
        QList < QString > values = lines.at(i).split('\t');
        if (values.count() < 2) {
            continue;
        }
        QDateTime dt =
            QDateTime::fromString(values.at(0), "yyyy-MM-dd hh:mm:ss");
        int charge = values.at(1).toInt();
        dates.append(dt);
        charges.append(charge);

        dtMin = (dtMin < dt ? dtMin : dt);
        dtMax = (dtMax > dt ? dtMax : dt);
        chargeMax = (chargeMax > charge ? chargeMax : charge);
    }

    int w = (9 * this->width()) / 10;
    int h = bNext->y() - bNext->height();

    int totalSecs = dtMin.secsTo(dtMax);

    if (chargeMax == 0 || totalSecs == 0)
        return;

    QPainter p(this);
    p.fillRect(0, 0, width(), height(), Qt::black);
    p.setBackground(Qt::black);
    p.setBrush(Qt::white);
    p.setPen(Qt::white);

    int fontW = p.fontMetrics().width('w');
    int fontH = p.fontMetrics().height();

    p.translate(fontW, fontH);

    p.drawLine(0, h, w, h);
    p.drawLine(0, 0, 0, h);

    QPen pen = p.pen();

    for (int round = 0; round <= 1; round++) {
        int chargeX = 0x7fffffff;
        int currentX = 0x7fffffff;
        int hourTextX = 0x7fffffff;

        int x1 = -1;
        int y1 = -1;
        int prevSecs = -1;
        int prevCharge = -1;

        for (int i = 0; i < dates.count(); i++) {
            QDateTime dt = dates.at(i);
            int charge = charges.at(i);
            int secs = dtMin.secsTo(dt);

            int x2 = (w * secs) / totalSecs;
            int y2 = (h * charge) / chargeMax;

            y2 = h - (5 * y2 / 6);      // flip y and add 1/6 for charge value text

            // Draw time on x axis
            if (abs(x2 - hourTextX) > 5 * fontW) {
                if (round == 0) {
                    p.drawText(x2, h + fontH, dt.toString("hh:mm"));
                    p.setPen(Qt::darkGreen);
                    p.drawLine(x2, y2, x2, h);
                    p.setPen(pen);
                }
                hourTextX = x2;
            }
            // Draw charge point and charge value
            p.drawEllipse(x2 - 2, y2 - 2, 4, 4);
            int shiftY = -h / 6;
            int y = y2 + shiftY;
            if (abs(x2 - chargeX) > 2 * fontW) {
                QString text = QString::number(charge);

                p.setPen(Qt::darkGreen);
                p.drawLine(x2, y, x2, y2);
                p.setPen(pen);

                p.save();
                p.translate(x2, y);
                p.rotate(90);
                p.drawText(0, 0, text);
                p.restore();

                chargeX = x2;
            }
            // Draw charge line and in the middle write current
            if (x1 >= 0) {
                p.drawLine(x1, y1, x2, y2);
                y = (y1 + y2) / 2;
                int x = (x1 + x2) / 2;
                int shiftY = h / 4;
                if (abs(x - currentX) > 2 * fontW) {
                    int current =
                        computeCurrent(secs - prevSecs, prevCharge, charge);
                    if (round == 0) {
                        p.setPen(Qt::red);
                        p.drawLine(x, y, x, y + shiftY);
                        p.setPen(pen);
                    }
                    QString text = QString::number(current) + "mA";
                    int textW = p.fontMetrics().width(text);
                    p.save();
                    p.translate(x, y + shiftY);
                    p.rotate(90);
                    p.fillRect(0, -fontH, textW, fontH, Qt::red);
                    p.drawText(0, 0, text);
                    p.restore();
                    currentX = x;
                }
            }

            x1 = x2;
            y1 = y2;
            prevSecs = secs;
            prevCharge = charge;
        }
    }
}

void NeoControl::updateRtc()
{
    if (screen != ScreenRtc)
        return;

    QDateTime rtcNow = Qtopia::rtcNow();
    QString rtcDate = Qtopia::readFile("/sys/class/rtc/rtc0/date").trimmed();
    QString rtcTime = Qtopia::readFile("/sys/class/rtc/rtc0/time").trimmed();

    QByteArray wakealarmStr =
        Qtopia::readFile("/sys/class/rtc/rtc0/wakealarm").trimmed();
    QString alarmStr;
    if (wakealarmStr.isEmpty()) {
        alarmStr = tr("not set");
    } else {
        uint wakealarmSecs = wakealarmStr.toUInt();
        QDateTime wakealarmDt = QDateTime::fromTime_t(wakealarmSecs);
        alarmStr = wakealarmDt.toString();
    }

    label->setText(QString
                   (tr
                    ("RTC (Real time clock)\n\nDate: %1\nTime: %2\nLocal: %3\nAlarm: %4"))
                   .arg(rtcDate)
                   .arg(rtcTime)
                   .arg(rtcNow.toString())
                   .arg(alarmStr));

    QTimer::singleShot(1000, this, SLOT(updateRtc()));
}

int NeoControl::openAlsaMixer()
{
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

    system("alsactl -f /opt/qtmoko/etc/alsa/gta04_initial_alsa.state restore");
}

void NeoControl::updateMixer()
{
    if (screen != ScreenMixer) {
        return;
    }
    if (slider4->sliding || slider5->sliding) {
        QTimer::singleShot(100, this, SLOT(updateMixer()));
        return;
    }

    snd_mixer_elem_t *elem;
    snd_mixer_elem_t *elem4 = NULL;
    snd_mixer_elem_t *elem5 = NULL;

    for (elem = snd_mixer_first_elem(mixerFd); elem;
         elem = snd_mixer_elem_next(elem)) {
        QString elemName = QString(snd_mixer_selem_get_name(elem));

        if (elemName == "DAC2 Digital Fine") {
            elem4 = elem;
        } else if (elemName == "Analog") {
            elem5 = elem;
        }
    }

    slider4->setMixerElem(elem4, true);
    slider5->setMixerElem(elem5, false);

    label4->setText(tr("Playback %1").arg(slider4->volume));    // Mono Playback Volume
    label5->setText(tr("Microphone %1").arg(slider5->volume));  // Mono Sidetone Playback Volume

    label->setText(tr("Call volume settings"));

    QTimer::singleShot(1000, this, SLOT(updateMixer()));
}

QString NeoControl::getQpeEnv()
{
    QFile f("/opt/qtmoko/qpe.env");
    if (!f.open(QFile::ReadOnly)) {
        QMessageBox::critical(this, tr("FSO"),
                              tr("Failed to read") + " " + f.fileName());
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
    if (fso) {
        content = content.replace(atStr, fsoStr);
    } else {
        content = content.replace(fsoStr, atStr);
    }
    QFile f("/opt/qtmoko/qpe.env");
    if (!f.open(QFile::WriteOnly)) {
        QMessageBox::critical(this, tr("FSO"),
                              tr("Failed to write to") + " " + f.fileName());
        return;
    }
    f.write(content.toLatin1());
    f.close();
    QMessageBox::information(this, tr("FSO"),
                             tr
                             ("You have to restart your phone for changes to take place"));
}

void NeoControl::fsoStateChanged(int)
{
    if (updatingScreen) {
        return;
    }
    QTimer::singleShot(0, this, SLOT(fsoChange()));
}

void NeoControl::fsoChange()
{
    bool checked = chkFso->isChecked();
    if (!checked) {
        QProcess::execute("qterminal",
                          QStringList() << "-c" << "update-rc.d" << "-f" <<
                          "fso-deviced" << "remove");
        setQpeEnv(false);       // disable FSO
        return;
    }
    if (!QFile::exists("/usr/sbin/fsogsmd")) {
        QMessageBox::information(this, tr("FSO"),
                                 tr
                                 ("FSO packages have to be downloaded and installed. Please make sure you have internet connection now."));
        QProcess::execute("raptor",
                          QStringList() << "-u" << "-i" << "fso-gsmd-openmoko"
                          << "fso-usaged-openmoko");
        QMessageBox::information(this, tr("FSO"),
                                 tr
                                 ("QtMoko needs very recent FSO, it will be downloaded from http://activationrecord.net/radekp/pub/libfsogsm.so.0.0.0"));
        QProcess::execute("qterminal",
                          QStringList() << "-c" << "wget" <<
                          "http://activationrecord.net/radekp/pub/libfsogsm.so.0.0.0");
        QProcess::execute("qterminal",
                          QStringList() << "-c" << "mv" << "libfsogsm.so.0.0.0"
                          <<
                          "/usr/lib/cornucopia/libs/fsogsm/libfsogsm.so.0.0.0");
    }
    QProcess::execute("qterminal",
                      QStringList() << "-c" << "update-rc.d" << "fso-deviced" <<
                      "defaults");
    setQpeEnv(true);
}

void NeoControl::updateModem()
{
    if (screen != ScreenModem) {
        return;
    }
    updatingScreen = true;

    if (!lineEdit->hasFocus()) {
        QSettings cfg("Trolltech", "Modem");
        lineEdit->setText(cfg.value("OPSYS/Value", "AT_OPSYS=0,2").toString());
        label->
            setText
            ("AT_OPSYS=0,2 is 2G only\nAT_OPSYS=3,2 is 3G\n3G=modem troubles");
    }

    QString qpeEnv = getQpeEnv();
    QString fsoStr = "export QTOPIA_PHONE=Fso";
    chkFso->setChecked(qpeEnv.indexOf(fsoStr) >= 0);

    updatingScreen = false;
    QTimer::singleShot(1000, this, SLOT(updateModem()));
}

static void appendValue(QString desc, QString file, QString * text,
                        QByteArray replaceBefore = "", QByteArray replaceAfter =
                        "")
{
    text->append(desc);
    text->append(": ");

    QFile f(file);
    if (!f.open(QFile::ReadOnly)) {
        text->append("failed to open " + file + " " + f.errorString() + "\n");
    } else {
        QByteArray content = f.readAll();
        if (content.length() == 0) {
            text->append('\n');
        } else {
            if (replaceBefore.count() > 0) {
                content = content.replace(replaceBefore, replaceAfter);
            }
            text->append(content);
        }
        f.close();
    }
}

void NeoControl::updateSysfs()
{
    if (screen != ScreenSysfs) {
        return;
    }

    QString text;
    appendValue(tr("Battery"), "/sys/class/power_supply/bq27000-battery/uevent",
                &text, "POWER_SUPPLY_", "  ");
    appendValue(tr("USB"), "/sys/class/power_supply/twl4030_usb/uevent", &text,
                "POWER_SUPPLY_", "  ");
    appendValue(tr("USB max current"),
                "/sys/class/power_supply/twl4030_usb/max_current", &text);

    label->setText(text);

    QTimer::singleShot(1000, this, SLOT(updateSysfs()));
}

#ifdef QTOPIA
#define CHARGE_LOG_FILE "/var/log/charging"
#else
#define CHARGE_LOG_FILE "/mnt/neo/var/log/charging"
#endif

void NeoControl::updateCharge()
{
    if (screen != ScreenCharge) {
        return;
    }
    updatingScreen = true;

    QFile f(CHARGE_LOG_FILE);
    bool isLogging = f.exists();

    chkCharge->setChecked(isLogging);
    label->setVisible(!isLogging);
    lineEdit->setVisible(!isLogging);
    bSave->setVisible(!isLogging);

    if (isLogging) {
        setFont(smallFont);
        if (f.open(QIODevice::ReadOnly)) {
            chargeLog = f.readAll();
            f.close();
        }
    } else {
        setFont(normalFont);
        label->setText(tr("Logging interval"));
        if (!lineEdit->hasFocus()) {
            QSettings cfg("Trolltech", "qpe");
            cfg.beginGroup("Charging");
            int chargingLogInterval = cfg.value("LogInterval", 300).toInt();
            lineEdit->setText(QString::number(chargingLogInterval));
        }
    }

    QTimer::singleShot(3000, this, SLOT(updateCharge()));
    update();
    updatingScreen = false;
}

void NeoControl::chargeStateChanged(int state)
{
    if (updatingScreen)
        return;

    QFile f(CHARGE_LOG_FILE);
    if (state == Qt::Checked) {
        if (!f.exists()) {
            if (f.open(QIODevice::WriteOnly)) {
                f.close();
            }
            QMessageBox::information(this, tr("Charging log"),
                                     tr
                                     ("Charging log is now in /var/log/charging. It will take a while until logging starts."));
        }
    } else {
        if (QMessageBox::question(this, tr("Stop charging log"),
                                  tr
                                  ("This will delete current log"),
                                  QMessageBox::Yes,
                                  QMessageBox::No) == QMessageBox::Yes) {
            f.remove();
        }
    }
}
