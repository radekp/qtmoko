/****************************************************************************
**
** This file is part of the Qt Extended Opensource Package.
**
** Copyright (C) 2011 Radek Polak <psonek2@seznam.cz>
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

#include "neoaudioplugin.h"

#include <QDir>
#include <QDebug>
#include <Qtopia>
#include <QProcess>
#include <QAudioState>
#include <QAudioStateInfo>
#include <QValueSpaceItem>
#include <QtopiaIpcAdaptor>
#include <QtopiaIpcEnvelope>

#include <qplugin.h>
#include <qtopialog.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#ifdef QTOPIA_BLUETOOTH
#include <QBluetoothAudioGateway>
#endif

// For GTA04 audio system docs please check:
//     http://projects.goldelico.com/p/gta04-kernel/page/Sound/
//
// We support following scenarios:
//       Speaker: stereo output from internal GTA04 speaker + internal MIC on phone call
//      Earpiece: default for GSM calls, ouput from earpiece + internal MIC
//       Headset: external headphones
//     Recording: for recording audio e.g. in voice notes app
//  GSMBluetooth: for gsm calls over bluetooth
//
// It seems that restoring whole state is quite fast on GTA04 (200ms). You can
// find in history version based on amixer and switching just needed controls
// but we switch whole state files now because it's more easily customizable
// for user.

/*
static bool amixerSet(QStringList & args)
{
    qLog(AudioState) << "amixer set " << args;

    args.insert(0, "-q");       // make it quiet
    args.insert(0, "set");
    int ret = QProcess::execute("amixer", args);
    if (ret != 0) {
        qWarning() << "amixer returned " << ret;
    }
    return ret == 0;
}*/

static bool alsactl(QStringList & args)
{
    qLog(AudioState) << "alsactl " << args;
    return true;

    /*
    for(int i = 0; i < 8; i++) {
        
        QProcess p;
        p.start("alsactl", args);
        p.waitForFinished(-1);
        QString output = p.readAllStandardOutput();
        output += p.readAllStandardError();

        if(output.length() == 0)
            return true;

        qWarning() << "alsactl returned " << output << ", running kill-snd-card-users.sh";
        QProcess::execute("kill-snd-card-users.sh");
    }
    return false;   */
}

static bool restoreState(QString stateFile, bool gsm = false)
{
    if (gsm) {
        stateFile = "gsm" + stateFile;
    }
    stateFile += ".state";
    return alsactl(QStringList() << "-f" << "/opt/qtmoko/etc/alsa/" +
                   stateFile << "restore");
}

// Initialise to default state (for audio playback with mic off)
static bool restoreDefaultState()
{
    return restoreState("speaker");
}

static bool writeToFile(const char *filename, const char *val, int len)
{
    qLog(AudioState) << "echo " << val << " > " << filename;
    return (bool) Qtopia::writeFile(filename, val, len);
}

// ========================================================================= //
class SpeakerAudioState : public QAudioState
{
    Q_OBJECT
public:
    SpeakerAudioState(bool isPhone, QObject * parent = 0);

    QAudioStateInfo info() const;
     QAudio::AudioCapabilities capabilities() const;

    bool isAvailable() const;
    bool enter(QAudio::AudioCapability capability);
    bool leave();

private:
     QAudioStateInfo m_info;
    bool m_isPhone;
};

SpeakerAudioState::SpeakerAudioState(bool isPhone, QObject * parent):
QAudioState(parent), m_isPhone(isPhone)
{
    if (isPhone) {
        m_info.setDomain("Phone");
        m_info.setProfile("PhoneSpeaker");
        m_info.setPriority(150);

    } else {
        m_info.setDomain("Media");
        m_info.setProfile("MediaSpeaker");
        m_info.setPriority(100);
    }
    m_info.setDisplayName(tr("Speaker"));
}

QAudioStateInfo SpeakerAudioState::info() const
{
    return m_info;
}

QAudio::AudioCapabilities SpeakerAudioState::capabilities()const
{
    return QAudio::OutputOnly;
}

bool SpeakerAudioState::isAvailable() const
{
    return true;
}

bool SpeakerAudioState::enter(QAudio::AudioCapability)
{
    qLog(AudioState) << "SpeakerAudioState::enter()" << "isPhone" << m_isPhone;
    bool ok = restoreState("speaker", m_isPhone);
    return ok;

}

bool SpeakerAudioState::leave()
{
    qLog(AudioState) << "SpeakerAudioState::leave()";
    return true;
}

// ========================================================================= //
class EarpieceHwAudioState : public QAudioState
{
    Q_OBJECT
public:
    EarpieceHwAudioState(bool isPhone, QObject * parent = 0);

    QAudioStateInfo info() const;
    QAudio::AudioCapabilities capabilities() const;

    bool isAvailable() const;
    bool enter(QAudio::AudioCapability capability);
    bool leave();

private:
    QAudioStateInfo m_info;
    bool m_isPhone;
};

EarpieceHwAudioState::EarpieceHwAudioState(bool isPhone, QObject * parent):
QAudioState(parent), m_isPhone(isPhone)
{
    if (isPhone) {
        m_info.setDomain("Phone");
        m_info.setProfile("PhoneEarpiece");
        m_info.setPriority(100);
    } else {
        m_info.setDomain("Media");
        m_info.setProfile("MediaEarpiece");
        m_info.setPriority(150);
    }
    m_info.setDisplayName(tr("Earpiece"));
}

QAudioStateInfo EarpieceHwAudioState::info() const
{
    return m_info;
}

QAudio::AudioCapabilities EarpieceHwAudioState::capabilities()const
{
    return QAudio::OutputOnly;
}

bool EarpieceHwAudioState::isAvailable() const
{
    return true;
}

bool EarpieceHwAudioState::enter(QAudio::AudioCapability)
{
    qLog(AudioState) << "EarpieceHwAudioState::enter()" << "isPhone" << m_isPhone;
    return restoreState("earpiecehw", m_isPhone);
}

bool EarpieceHwAudioState::leave()
{
    qLog(AudioState) << "EarpieceHwAudioState::leave()";
    return true;
}


// ========================================================================= //
class HeadsetAudioState : public QAudioState
{
    Q_OBJECT
public:
    HeadsetAudioState(bool isPhone, QObject * parent = 0);

    QAudioStateInfo info() const;
    QAudio::AudioCapabilities capabilities() const;

    bool isAvailable() const;
    bool enter(QAudio::AudioCapability capability);
    bool leave();

private slots:
    void onHeadsetModified();

private:
    QAudioStateInfo m_info;
    bool m_isPhone;
    QValueSpaceItem *m_headset;
};

HeadsetAudioState::HeadsetAudioState(bool isPhone, QObject * parent):
QAudioState(parent), m_isPhone(isPhone)
{
    if (m_isPhone) {
        m_info.setDomain("Phone");
        m_info.setProfile("Headset");
    } else {
        m_info.setDomain("Media");
        m_info.setProfile("Headset");
    }

    m_info.setDisplayName(tr("Headphones"));
    m_info.setPriority(50);

    m_headset =
        new QValueSpaceItem("/Hardware/Accessories/PortableHandsfree/Present",
                            this);
    connect(m_headset, SIGNAL(contentsChanged()), SLOT(onHeadsetModified()));
}

QAudioStateInfo HeadsetAudioState::info() const
{
    return m_info;
}

QAudio::AudioCapabilities HeadsetAudioState::capabilities()const
{
    return QAudio::InputOnly | QAudio::OutputOnly;
}

void HeadsetAudioState::onHeadsetModified()
{
    bool avail = m_headset->value().toBool();

    qLog(AudioState) << __PRETTY_FUNCTION__ << avail;

    if (!avail) {
        QtopiaIpcEnvelope e("QPE/AudioVolumeManager/NeoVolumeService",
                            "setAmp(bool)");
        e << true;              //turn on the speaker amp
    } else {
        QtopiaIpcEnvelope e("QPE/AudioVolumeManager/NeoVolumeService",
                            "setAmp(bool)");
        e << false;             //turn of the speaker amp
    }

    emit availabilityChanged(avail);
}

bool HeadsetAudioState::isAvailable() const
{
    return m_headset->value().toBool();
}

bool HeadsetAudioState::enter(QAudio::AudioCapability)
{
    qLog(AudioState) << "HeadsetAudioState::enter()" << "isPhone" << m_isPhone;

    bool ok = writeToFile("/sys/devices/virtual/gpio/gpio55/value", "1", 1) // switch off video out
        && restoreState("headset", m_isPhone);

    return ok;
}

bool HeadsetAudioState::leave()
{
    return true;
}

// ========================================================================= //
class RecordingAudioState : public QAudioState
{
    Q_OBJECT
public:
    RecordingAudioState(QObject * parent = 0);

    QAudioStateInfo info() const;
    QAudio::AudioCapabilities capabilities() const;

    bool isAvailable() const;
    bool enter(QAudio::AudioCapability capability);
    bool leave();

private:
    QAudioStateInfo m_info;
};

RecordingAudioState::RecordingAudioState(QObject * parent):
QAudioState(parent)
{
    m_info.setDomain("Media");
    m_info.setProfile("Recording");
    m_info.setDisplayName(tr("Recording"));
    m_info.setPriority(150);
}

QAudioStateInfo RecordingAudioState::info() const
{
    return m_info;
}

QAudio::AudioCapabilities RecordingAudioState::capabilities()const
{
    return QAudio::InputOnly;
}

bool RecordingAudioState::isAvailable() const
{
    return true;
}

bool RecordingAudioState::enter(QAudio::AudioCapability capability)
{
    Q_UNUSED(capability)
        qLog(AudioState) << "RecordingAudioState::enter()";
    return restoreState("recording.state");
}

bool RecordingAudioState::leave()
{
    qLog(AudioState) << "RecordingAudioState::leave()";
    return true;
}

#ifdef QTOPIA_BLUETOOTH
class BluetoothAudioState : public QAudioState
{
    Q_OBJECT
public:
    explicit BluetoothAudioState(bool isPhone, QObject * parent = 0);
    virtual ~ BluetoothAudioState();

    virtual QAudioStateInfo info() const;
    virtual QAudio::AudioCapabilities capabilities() const;

    virtual bool isAvailable() const;
    virtual bool enter(QAudio::AudioCapability capability);
    virtual bool leave();

private slots:
    void bluetoothAudioStateChanged();
    void headsetDisconnected();
    void headsetConnected();

private:
    bool resetCurrAudioGateway();

private:
    QList < QBluetoothAudioGateway * >m_audioGateways;
    bool m_isPhone;
    QBluetoothAudioGateway *m_currAudioGateway;
    QAudioStateInfo m_info;
    bool m_isActive;
    bool m_isAvail;
};

BluetoothAudioState::BluetoothAudioState(bool isPhone, QObject * parent):
QAudioState(parent),
m_isPhone(isPhone), m_currAudioGateway(0), m_isActive(false), m_isAvail(false)
{
    QBluetoothAudioGateway *hf =
        new QBluetoothAudioGateway("BluetoothHandsfree");
    m_audioGateways.append(hf);
    qLog(AudioState) << "Handsfree audio gateway: " << hf;

    QBluetoothAudioGateway *hs = new QBluetoothAudioGateway("BluetoothHeadset");
    m_audioGateways.append(hs);
    qLog(AudioState) << "Headset audio gateway: " << hs;

    for (int i = 0; i < m_audioGateways.size(); ++i) {
        QBluetoothAudioGateway *gateway = m_audioGateways.at(i);
        connect(gateway, SIGNAL(audioStateChanged()),
                SLOT(bluetoothAudioStateChanged()));
        connect(gateway, SIGNAL(headsetDisconnected()),
                SLOT(headsetDisconnected()));
        connect(gateway, SIGNAL(connectResult(bool, QString)),
                SLOT(headsetConnected()));
        connect(gateway, SIGNAL(newConnection(QBluetoothAddress)),
                SLOT(headsetConnected()));
    }

    if (isPhone) {
        m_info.setDomain("Phone");
        m_info.setProfile("PhoneBluetoothHeadset");
        m_info.setPriority(25);
    } else {
        m_info.setDomain("Media");
        m_info.setProfile("MediaBluetoothHeadset");
        m_info.setPriority(150);
    }

    m_info.setDisplayName(tr("Bluetooth Headset"));
    m_isAvail = resetCurrAudioGateway();
}

BluetoothAudioState::~BluetoothAudioState()
{
    qDeleteAll(m_audioGateways);
}

bool BluetoothAudioState::resetCurrAudioGateway()
{
    for (int i = 0; i < m_audioGateways.size(); ++i) {
        QBluetoothAudioGateway *gateway = m_audioGateways.at(i);
        if (gateway->isConnected()) {
            m_currAudioGateway = gateway;
            qLog(AudioState) << "Returning audiogateway to be:" <<
                m_currAudioGateway;
            return true;
        }
    }

    qLog(AudioState) << "No current audio gateway found";
    return false;
}

void BluetoothAudioState::bluetoothAudioStateChanged()
{
    qLog(AudioState) << "BluetoothAudioState::bluetoothAudioStateChanged()" <<
        m_isActive << m_currAudioGateway;

    if (m_isActive && (m_currAudioGateway || resetCurrAudioGateway())) {
        if (!m_currAudioGateway->audioEnabled()) {
            emit doNotUseHint();
        }
    } else if (!m_isActive && (m_currAudioGateway || resetCurrAudioGateway())) {
        if (m_currAudioGateway->audioEnabled()) {
            emit useHint();
        }
    }
}

void BluetoothAudioState::headsetConnected()
{
    if (!m_isAvail && resetCurrAudioGateway()) {
        m_isAvail = true;
        emit availabilityChanged(true);
    }
}

void BluetoothAudioState::headsetDisconnected()
{
    if (!resetCurrAudioGateway()) {
        m_isAvail = false;
        emit availabilityChanged(false);
    }
}

QAudioStateInfo BluetoothAudioState::info() const
{
    return m_info;
}

QAudio::AudioCapabilities BluetoothAudioState::capabilities()const
{

    if (m_isPhone) {
        return QAudio::OutputOnly;
    } else {
        return QAudio::OutputOnly | QAudio::InputOnly;
    }
}

bool BluetoothAudioState::isAvailable() const
{
    return m_isAvail;
}

bool BluetoothAudioState::enter(QAudio::AudioCapability capability)
{
    Q_UNUSED(capability)

        qLog(AudioState) << "BluetoothAudioState::enter()" << "isPhone" <<
        m_isPhone;

    if (m_currAudioGateway || resetCurrAudioGateway()) {
        m_currAudioGateway->connectAudio();
        //m_isActive = setAudioScenario(Scenario_GSMBluetooth);
    }

    return m_isActive;
}

bool BluetoothAudioState::leave()
{
    if (m_currAudioGateway || resetCurrAudioGateway()) {
        m_currAudioGateway->releaseAudio();
    }

    m_isActive = false;

    return true;
}
#endif

class NeoAudioPluginPrivate
{
public:
    QList < QAudioState * >m_states;
};

NeoAudioPlugin::NeoAudioPlugin(QObject * parent):
QAudioStatePlugin(parent)
{
    m_data = new NeoAudioPluginPrivate;
    
    // On A4+ models use HW sound routing, on A3 do SW routing
    m_data->m_states.push_back(new EarpieceHwAudioState(this));   // default for gsm calls    
    m_data->m_states.push_back(new SpeakerAudioState(false, this)); // ringtones, mp3 etc..
    m_data->m_states.push_back(new SpeakerAudioState(true, this));  // loud gsm
    m_data->m_states.push_back(new HeadsetAudioState(false, this)); // audio in headphones
    m_data->m_states.push_back(new HeadsetAudioState(true, this));  // gsm in headphones
    m_data->m_states.push_back(new RecordingAudioState(this));  // for recording e.g. in voice notes app
#ifdef QTOPIA_BLUETOOTH
    // Can play media through bluetooth. Can record through bluetooth as well.
    m_data->m_states.push_back(new BluetoothAudioState(false, this));
    m_data->m_states.push_back(new BluetoothAudioState(true, this));
#endif

    // Set default alsa state
    restoreDefaultState();
}

NeoAudioPlugin::~NeoAudioPlugin()
{
    qDeleteAll(m_data->m_states);

    delete m_data;
}

QList < QAudioState * >NeoAudioPlugin::statesProvided()const
{
    return m_data->m_states;
}

Q_EXPORT_PLUGIN2(neoaudio_plugin, NeoAudioPlugin)
#include "neoaudioplugin.moc"
