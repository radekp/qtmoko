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

bool usePulse;
bool gta04a3;

static bool alsactl(QStringList & args)
{
    if(usePulse) {
        QProcess p;
        args.insert(0, "alsactl");
        args.insert(0, "--");
        qLog(AudioState) << "pasuspender " << args;
        p.start("pasuspender", args);
        p.waitForFinished(-1);
        return true;
    }
    
    qLog(AudioState) << "alsactl " << args;

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
    return false;
}

static bool writeToFile(const char *filename, const char *val, int len)
{
    qLog(AudioState) << "echo " << val << " > " << filename;
    return (bool) Qtopia::writeFile(filename, val, len);
}

QProcess *voicePs = NULL;

static bool gsmVoiceStop()
{
    // Move back alsa config (used e.g. by blueooth a2dp sound)
    if(QFile::exists("/home/root/.asoundrc.tmp")) {
        QFile::rename("/home/root/.asoundrc.tmp", "/home/root/.asoundrc");
    }

    if (voicePs == NULL) {
        return true;
    }
    qLog(AudioState) << "terminating gsm-voice-routing pid " << voicePs->pid();
    voicePs->terminate();
    if (!voicePs->waitForFinished(1000)) {
        qWarning() << "gsm-voice-routing process failed to terminate";
        voicePs->kill();
    }
    delete(voicePs);
    voicePs = NULL;
    return true;
}

static bool gsmVoiceStart()
{
    if (voicePs != NULL) {
        return true;
    }

    // Move away alsa config (used e.g. by blueooth a2dp sound)
    if(QFile::exists("/home/root/.asoundrc")) {
        QFile::rename("/home/root/.asoundrc", "/home/root/.asoundrc.tmp");
    }
    
    voicePs = new QProcess();
    QStringList args;

    // Dump output always to stderr if audio logging is enabled
    if (qLogEnabled(AudioState)) {
        QStringList env = QProcess::systemEnvironment();
        for (int i = 0; i < env.count(); i++) {
            if (env.at(i).startsWith("GSM_VOICE_ROUTING_LOGFILE")) {
                env.removeAt(i);
                voicePs->setEnvironment(env);
                break;
            }
        }
        voicePs->setProcessChannelMode(QProcess::ForwardedChannels);
    }

    if(usePulse) {
        args.insert(0, "gsm-voice-routing");
        args.insert(0, "--");
        qLog(AudioState) << "pasuspender " << args;
        voicePs->start("pasuspender", args);
    } else
        voicePs->start("gsm-voice-routing");

    if (voicePs->waitForStarted(3000)) {
        qLog(AudioState) << "starting gsm-voice-routing pid " << voicePs->pid();
        return true;
    }
    qWarning() << "failed to start gsm-voice-routing: " <<
        voicePs->errorString();
    return false;
}

/* Class for an audio state based on an ALSA state file. */
class StateFileAudioState : public QAudioState
{
    Q_OBJECT
public:
    StateFileAudioState(QByteArray domain,
			QByteArray profile,
			int priority,
			QObject * parent = 0);

    virtual QAudioStateInfo info() const;
    virtual QAudio::AudioCapabilities capabilities() const;

    virtual bool isAvailable() const;
    virtual bool enter(QAudio::AudioCapability capability);
    virtual bool leave();

private:
    QAudioStateInfo m_info;
};

StateFileAudioState::StateFileAudioState(QByteArray domain,
					 QByteArray profile,
					 int priority,
					 QObject * parent) :
    QAudioState(parent)
{
    m_info.setDomain(domain);
    m_info.setProfile(profile);
    m_info.setPriority(priority);
    m_info.setDisplayName(tr(profile));
}

QAudioStateInfo StateFileAudioState::info() const
{
    return m_info;
}

QAudio::AudioCapabilities StateFileAudioState::capabilities() const
{
    return (QAudio::InputOnly |
	    QAudio::OutputOnly |
	    QAudio::InputAndOutput);
}

bool StateFileAudioState::isAvailable() const
{
    return true;
}

bool StateFileAudioState::enter(QAudio::AudioCapability)
{
    if (gta04a3 && (m_info.domain() != "Phone")) {
        gsmVoiceStop();
    }

    QString stateFile = "/opt/qtmoko/etc/alsa/";

    if (gta04a3) {
        stateFile += "a3/";
    } else {
        stateFile += "a4/";
    }

    stateFile += m_info.domain() + m_info.profile() + ".state";

    bool ok = alsactl(QStringList() << "-f" << stateFile << "restore");

    if (gta04a3 && (m_info.domain() == "Phone")) {
        gsmVoiceStart();
    }

    return ok;
}

bool StateFileAudioState::leave()
{
    return true;
}

/* We need a subclass for Headset, so we can indicate when the headset
   is available. */
class HeadsetAudioState : public StateFileAudioState
{
    Q_OBJECT
public:
    HeadsetAudioState(QByteArray domain,
		      int priority,
		      QObject * parent = 0);

    virtual bool isAvailable() const;
    virtual bool enter(QAudio::AudioCapability capability);

private slots:
    void onHeadsetModified();

private:
    QValueSpaceItem *m_headset;
};

HeadsetAudioState::HeadsetAudioState(QByteArray domain,
				     int priority,
				     QObject * parent) :
    StateFileAudioState(domain,
			"Headset",
			priority,
			parent)
{
    m_headset =
        new QValueSpaceItem("/Hardware/Accessories/PortableHandsfree/Present",
                            this);
    connect(m_headset, SIGNAL(contentsChanged()), SLOT(onHeadsetModified()));
}

void HeadsetAudioState::onHeadsetModified()
{
    bool avail = m_headset->value().toBool();

    qLog(AudioState) << __PRETTY_FUNCTION__ << avail;

    emit availabilityChanged(avail);
}

bool HeadsetAudioState::isAvailable() const
{
    return m_headset->value().toBool();
}

bool HeadsetAudioState::enter(QAudio::AudioCapability cap)
{
    bool ok = writeToFile("/sys/devices/virtual/gpio/gpio55/value", "1", 1) // switch off video out
      && StateFileAudioState::enter(cap);

    return ok;
}

#ifdef QTOPIA_BLUETOOTH
class BluetoothAudioState : public QAudioState
{
    Q_OBJECT
public:
    explicit BluetoothAudioState(bool isPhone,
				 int priority,
				 QObject * parent = 0);
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

BluetoothAudioState::BluetoothAudioState(bool isPhone,
					 int priority,
					 QObject * parent):
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
    } else {
        m_info.setDomain("Media");
        m_info.setProfile("MediaBluetoothHeadset");
    }
    m_info.setPriority(priority);

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
    
    usePulse = QFile::exists("/usr/bin/pasuspender");
    
    // On A4+ models use HW sound routing, on A3 do SW routing
    gta04a3 = !(QFile::exists("/sys/class/gpio/gpio186/value"));

    /* Priority ordering for phone calls: Headset (if available),
       Bluetooth (if available), Earpiece, Speaker. */
    m_data->m_states.
      push_back(new HeadsetAudioState("Phone", 1, this));
#ifdef QTOPIA_BLUETOOTH
    m_data->m_states.
      push_back(new BluetoothAudioState(true, 2, this));
#endif
    m_data->m_states.
      push_back(new StateFileAudioState("Phone", "Earpiece", 3, this));
    m_data->m_states.
      push_back(new StateFileAudioState("Phone", "Speaker", 4, this));

    /* Priority ordering for media: Headset (if available),
       Bluetooth (if available), Speaker, Earpiece. */
    m_data->m_states.
      push_back(new HeadsetAudioState("Media", 1, this));
#ifdef QTOPIA_BLUETOOTH
    m_data->m_states.
      push_back(new BluetoothAudioState(false, 2, this));
#endif
    m_data->m_states.
      push_back(new StateFileAudioState("Media", "Speaker", 3, this));
    m_data->m_states.
      push_back(new StateFileAudioState("Media", "Earpiece", 4, this));

    /* Priority ordering for ringtone is the same as for media, but
       BluetoothAudioState doesn't yet support the Ringtone domain. */
    m_data->m_states.
      push_back(new HeadsetAudioState("Ringtone", 1, this));
    m_data->m_states.
      push_back(new StateFileAudioState("Ringtone", "Speaker", 3, this));
    m_data->m_states.
      push_back(new StateFileAudioState("Ringtone", "Earpiece", 4, this));

    /* We shouldn't have to do anything here to set up the initial
       state.  Code elsewhere should know that we initially want the
       Media domain and choose the best available audio state for
       that. */
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
