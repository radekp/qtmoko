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

#include "neoaudioplugin.h"

#include <QAudioState>
#include <QAudioStateInfo>
#include <QValueSpaceItem>
#include <QtopiaIpcAdaptor>
#include <QtopiaIpcEnvelope>

#ifdef QTOPIA_BLUETOOTH
#include <QBluetoothAudioGateway>
#endif

#include <QDebug>
#include <qplugin.h>
#include <qtopialog.h>

#include <stdio.h>
#include <stdlib.h>
#include <QProcess>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <QDir>

//
// alsactl -f /etc/alsa/gsmbluetooth.state restore
// stereoout.state
// gsmheadset.state
// gsmhandset.state
// gsmbluetooth.state

enum NeoAudioScenario {
    Scenario_StereoOut,
    Scenario_GSMHandset,
    Scenario_GSMHeadset,
    Scenario_GSMBluetooth,
    Scenario_GSMSpeakerout,
    Scenario_CaptureHandset,
    Scenario_VoipHandset,
    Scenario_MediaHeadset
};

static const char* mode_to_string[] = {
    "stereoout",
    "gsmhandset",
    "gsmheadset",
    "gsmbluetooth",
    "gsmspeakerout",
    "capturehandset",
    "voip-handset",
    "headset",
};


static bool setAudioScenario(NeoAudioScenario audioScenario)
{
    QString mode(mode_to_string[static_cast<int>(audioScenario)]);

    QStringList args;
    args.append("-f");
    args.append("/opt/qtmoko/etc/alsa-scenarios/" + mode + ".state");
    args.append("restore");

    QProcess p;
    p.start("/usr/sbin/alsactl", args);
    p.waitForFinished();

    int result = p.exitCode();

    if (result == 0)
        qLog(AudioState) << "setAudioScenario(); using"<< QString( "/opt/qtmoko/etc/alsa-scenarios/%1.state").arg(mode);
    else
        qLog(AudioState)<< QString("Setting audio mode to: %1 failed").arg(mode);

    return result == 0;
}

#ifdef QTOPIA_BLUETOOTH
class BluetoothAudioState : public QAudioState
{
    Q_OBJECT
public:
    explicit BluetoothAudioState(bool isPhone, QObject *parent = 0);
    virtual ~BluetoothAudioState();

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
    QList<QBluetoothAudioGateway*> m_audioGateways;
    bool m_isPhone;
    QBluetoothAudioGateway* m_currAudioGateway;
    QAudioStateInfo m_info;
    bool m_isActive;
    bool m_isAvail;
};

BluetoothAudioState::BluetoothAudioState(bool isPhone, QObject *parent):
    QAudioState(parent),
    m_isPhone(isPhone),
    m_currAudioGateway(0),
    m_isActive(false),
    m_isAvail(false)
{
    QBluetoothAudioGateway *hf = new QBluetoothAudioGateway("BluetoothHandsfree");
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
        connect(gateway, SIGNAL(connectResult(bool,QString)),
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
            qLog(AudioState) << "Returning audiogateway to be:" << m_currAudioGateway;
            return true;
        }
    }

    qLog(AudioState) << "No current audio gateway found";
    return false;
}

void BluetoothAudioState::bluetoothAudioStateChanged()
{
    qLog(AudioState) << "BluetoothAudioState::bluetoothAudioStateChanged()" << m_isActive << m_currAudioGateway;

    if (m_isActive && (m_currAudioGateway || resetCurrAudioGateway())) {
        if (!m_currAudioGateway->audioEnabled()) {
            emit doNotUseHint();
        }
    }
    else if (!m_isActive && (m_currAudioGateway || resetCurrAudioGateway())) {
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

QAudio::AudioCapabilities BluetoothAudioState::capabilities() const
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

    qLog(AudioState) << "BluetoothAudioState::enter()" << "isPhone" << m_isPhone;

    if (m_currAudioGateway || resetCurrAudioGateway()) {
        m_currAudioGateway->connectAudio();
        m_isActive = setAudioScenario(Scenario_GSMBluetooth);
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

class HandsetAudioState : public QAudioState
{
    Q_OBJECT

public:
    HandsetAudioState(bool isPhone,QObject *parent = 0);

    QAudioStateInfo info() const;
    QAudio::AudioCapabilities capabilities() const;

    bool isAvailable() const;
    bool enter(QAudio::AudioCapability capability);
    bool leave();

private:
    QAudioStateInfo m_info;
    bool m_isPhone;

};
///Hardware/Accessories/PortableHandsfree"
HandsetAudioState::HandsetAudioState(bool isPhone, QObject *parent):
    QAudioState(parent),
    m_isPhone(isPhone)
{
    qLog(AudioState) << "HandsetAudioState" << isPhone;

    m_info.setDomain("Phone");
    m_info.setProfile("PhoneSpeaker");
    m_info.setDisplayName(tr("Handset"));

    m_info.setPriority(50);
}

QAudioStateInfo HandsetAudioState::info() const
{
    return m_info;
}

QAudio::AudioCapabilities HandsetAudioState::capabilities() const
{
    return QAudio::InputOnly | QAudio::OutputOnly;
}

bool HandsetAudioState::isAvailable() const
{
    return true;
}

bool HandsetAudioState::enter(QAudio::AudioCapability capability)
{
    Q_UNUSED(capability)

    qLog(AudioState) << "HHandsetAudioState::enter()" << "isPhone" << m_isPhone;

    return setAudioScenario(Scenario_GSMHandset);
}

bool HandsetAudioState::leave()
{
    return true;
}

class MediaSpeakerAudioState : public QAudioState
{
    Q_OBJECT

public:
    MediaSpeakerAudioState(bool isPhone, QObject *parent = 0);

    QAudioStateInfo info() const;
    QAudio::AudioCapabilities capabilities() const;

    bool isAvailable() const;
    bool enter(QAudio::AudioCapability capability);
    bool leave();

private:
    QAudioStateInfo m_info;
    bool m_isPhone;
};

MediaSpeakerAudioState::MediaSpeakerAudioState(bool isPhone, QObject *parent):
    QAudioState(parent),
    m_isPhone(isPhone)
{
    qLog(AudioState)<< "MediaSpeakerAudioState::MediaSpeakerAudioState() "<< m_isPhone;

    m_info.setDomain("Media");
    m_info.setProfile("MediaSpeaker");
    m_info.setDisplayName(tr("Stereo Speaker"));
    m_info.setPriority(150);
}

QAudioStateInfo MediaSpeakerAudioState::info() const
{
    return m_info;
}

QAudio::AudioCapabilities MediaSpeakerAudioState::capabilities() const
{
    return QAudio::OutputOnly;
}

bool MediaSpeakerAudioState::isAvailable() const
{
    return true;
}

bool MediaSpeakerAudioState::enter(QAudio::AudioCapability capability)
{
    qLog(AudioState) << "MediaSpeakerAudioState::enter()" << "isPhone" << m_isPhone;

    Q_UNUSED(capability)

    return setAudioScenario(Scenario_StereoOut);
}

bool MediaSpeakerAudioState::leave()
{
    qLog(AudioState) << "MediaSpeakerAudioState::leave()";

    return true;
}


class MediaCaptureAudioState : public QAudioState
{
    Q_OBJECT

public:
    MediaCaptureAudioState(bool isPhone, QObject *parent = 0);

    QAudioStateInfo info() const;
    QAudio::AudioCapabilities capabilities() const;

    bool isAvailable() const;
    bool enter(QAudio::AudioCapability capability);
    bool leave();

private:
    QAudioStateInfo m_info;
    bool m_isPhone;
};

MediaCaptureAudioState::MediaCaptureAudioState(bool isPhone, QObject *parent):
    QAudioState(parent),
    m_isPhone(isPhone)
{
    qLog(AudioState) << "MediaCaptureAudioState::MediaCaptureAudioState()" << "isPhone" << isPhone;

    m_info.setDomain("Media");
    m_info.setProfile("MediaCapture");
    m_info.setDisplayName(tr("Stereo Speaker"));
    m_info.setPriority(150);
}

QAudioStateInfo MediaCaptureAudioState::info() const
{
    return m_info;
}

QAudio::AudioCapabilities MediaCaptureAudioState::capabilities() const
{
    return QAudio::InputOnly;
}

bool MediaCaptureAudioState::isAvailable() const
{
    return true;
}

bool MediaCaptureAudioState::enter(QAudio::AudioCapability capability)
{
    Q_UNUSED(capability)

    qLog(AudioState) << "MediaCaptureAudioState::enter()" << "isPhone" << m_isPhone;

    return setAudioScenario(Scenario_VoipHandset);
}

bool MediaCaptureAudioState::leave()
{
    qLog(AudioState) << "MediaCaptureAudioState::leave()";

    return true;
}

class HeadphonesAudioState : public QAudioState
{
    Q_OBJECT

public:
    HeadphonesAudioState(bool isPhone, QObject *parent = 0);

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

HeadphonesAudioState::HeadphonesAudioState(bool isPhone, QObject *parent):
    QAudioState(parent),
    m_isPhone(isPhone)
{
    qLog(AudioState) << __PRETTY_FUNCTION__ << "isPhone" << m_isPhone;

    if (m_isPhone) {
        m_info.setDomain("Phone");
        m_info.setProfile("Headphone");
    } else {
        m_info.setDomain("Media");
        m_info.setProfile("Headphone");
    }

    m_info.setDisplayName(tr("Headphones"));
    m_info.setPriority(25);

    m_headset = new QValueSpaceItem("/Hardware/Accessories/PortableHandsfree/Present", this);
    connect(m_headset, SIGNAL(contentsChanged()), SLOT(onHeadsetModified()));
}

QAudioStateInfo HeadphonesAudioState::info() const
{
    return m_info;
}

QAudio::AudioCapabilities HeadphonesAudioState::capabilities() const
{
    return QAudio::OutputOnly;
}

void HeadphonesAudioState::onHeadsetModified()
{
    bool avail = m_headset->value().toBool();

    qLog(AudioState) << __PRETTY_FUNCTION__ << avail;

    if (!avail) {
        QtopiaIpcEnvelope e("QPE/AudioVolumeManager/NeoVolumeService", "setAmp(bool)");
        e << true; //turn on the speaker amp
    } else {
        QtopiaIpcEnvelope e("QPE/AudioVolumeManager/NeoVolumeService", "setAmp(bool)");
        e << false; //turn of the speaker amp
    }

    emit availabilityChanged(avail);
}

bool HeadphonesAudioState::isAvailable() const
{
    return m_headset->value().toBool();
}

bool HeadphonesAudioState::enter(QAudio::AudioCapability/* capability*/)
{
    qLog(AudioState) << __PRETTY_FUNCTION__ << "isPhone"<<m_isPhone;

    if( !m_isPhone) {
        return setAudioScenario(Scenario_MediaHeadset);
    } else {
        return setAudioScenario(Scenario_GSMHeadset);
    }
}

bool HeadphonesAudioState::leave()
{
    qLog(Hardware) << __PRETTY_FUNCTION__ << m_isPhone;
    return true;
}

class SpeakerphoneAudioState : public QAudioState
{
    Q_OBJECT

public:
    SpeakerphoneAudioState(QObject *parent = 0);

    QAudioStateInfo info() const;
    QAudio::AudioCapabilities capabilities() const;

    bool isAvailable() const;
    bool enter(QAudio::AudioCapability capability);
    bool leave();

private:
    QAudioStateInfo m_info;
};

SpeakerphoneAudioState::SpeakerphoneAudioState(QObject *parent)
    : QAudioState(parent)
{
    m_info.setDomain("Phone");
    m_info.setProfile("PhoneSpeakerphone");
    m_info.setDisplayName(tr("Speakerphone"));
    m_info.setPriority(100);
}

QAudioStateInfo SpeakerphoneAudioState::info() const
{
    return m_info;
}

QAudio::AudioCapabilities SpeakerphoneAudioState::capabilities() const
{
    return QAudio::InputOnly | QAudio::OutputOnly;
}

bool SpeakerphoneAudioState::isAvailable() const
{
    return true;
}

bool SpeakerphoneAudioState::enter(QAudio::AudioCapability capability)
{
    Q_UNUSED(capability)

    qLog(AudioState)<< __PRETTY_FUNCTION__ ;

    return setAudioScenario(Scenario_GSMSpeakerout);
}

bool SpeakerphoneAudioState::leave()
{
    qLog(AudioState)<<__PRETTY_FUNCTION__ ;

    return true;
}


class RingtoneAudioState : public QAudioState
{
    Q_OBJECT

public:
    RingtoneAudioState(QObject *parent = 0);

    QAudioStateInfo info() const;
    QAudio::AudioCapabilities capabilities() const;

    bool isAvailable() const;
    bool enter(QAudio::AudioCapability capability);
    bool leave();

private:
    QAudioStateInfo m_info;
};

RingtoneAudioState::RingtoneAudioState(QObject *parent):
    QAudioState(parent)
{
    m_info.setDomain("RingTone");
    m_info.setProfile("RingToneSpeaker");
    m_info.setDisplayName(tr("Stereo"));
    m_info.setPriority(120);
}

QAudioStateInfo RingtoneAudioState::info() const
{
    return m_info;
}

QAudio::AudioCapabilities RingtoneAudioState::capabilities() const
{
    return QAudio::InputOnly | QAudio::OutputOnly;
}

bool RingtoneAudioState::isAvailable() const
{
    return true;
}

bool RingtoneAudioState::enter(QAudio::AudioCapability)
{
    qLog(AudioState)<<" RingtoneAudioState::enter";

    return setAudioScenario(Scenario_StereoOut);
}

bool RingtoneAudioState::leave()
{
    qLog(AudioState)<<"RingtoneAudioState::leave()";

    return true;
}


class NeoAudioPluginPrivate
{
public:
    QList<QAudioState*> m_states;
};

NeoAudioPlugin::NeoAudioPlugin(QObject *parent):
    QAudioStatePlugin(parent)
{
    m_data = new NeoAudioPluginPrivate;

    m_data->m_states.push_back(new HandsetAudioState(this));

    m_data->m_states.push_back(new MediaSpeakerAudioState(this));
    m_data->m_states.push_back(new MediaCaptureAudioState(this));

    m_data->m_states.push_back(new HeadphonesAudioState(false, this));
    m_data->m_states.push_back(new HeadphonesAudioState(true, this));

#ifdef QTOPIA_BLUETOOTH
    // Can play media through bluetooth. Can record through bluetooth as well.
    m_data->m_states.push_back(new BluetoothAudioState(false, this));
    m_data->m_states.push_back(new BluetoothAudioState(true, this));
#endif

    m_data->m_states.push_back(new SpeakerphoneAudioState(this));
    m_data->m_states.push_back(new RingtoneAudioState(this));

    //TODO: Need to enable Bluetooth RingTone
}

NeoAudioPlugin::~NeoAudioPlugin()
{
    qDeleteAll(m_data->m_states);

    delete m_data;
}

QList<QAudioState *> NeoAudioPlugin::statesProvided() const
{
    return m_data->m_states;
}

Q_EXPORT_PLUGIN2(neoaudio_plugin, NeoAudioPlugin)

#include "neoaudioplugin.moc"
