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

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>

#include <QTimer>
#include <QDebug>
#include <QValueSpaceItem>
#include <qtopiaserverapplication.h>
#include <qtopiaipcenvelope.h>

#include <QAudioStateConfiguration>
#include <QAudioStateInfo>
#include <QValueSpaceObject>

#include "neovolumeservice.h"

NeoVolumeService::NeoVolumeService()
    : QtopiaIpcAdaptor("QPE/AudioVolumeManager/NeoVolumeService")
{
    publishAll(Slots);
    qLog(AudioState) << __PRETTY_FUNCTION__;

    m_adaptor = new QtopiaIpcAdaptor("QPE/NeoModem", this);
    m_vsoVolumeObject = new QValueSpaceObject("/Hardware/Audio");

    QValueSpaceItem *ampmode = new QValueSpaceItem("/System/Tasks/NeoVolumeService/ampMode");
    QObject::connect(ampmode, SIGNAL(contentsChanged()),
                     this, SLOT(toggleAmpMode()));

    QTimer::singleShot(0, this, SLOT(registerService()));
}

NeoVolumeService::~NeoVolumeService()
{
}

void NeoVolumeService::setVolume(int volume)
{
    adjustVolume(volume, volume, Absolute);
}

void NeoVolumeService::setVolume(int leftChannel, int rightChannel)
{
    adjustVolume(leftChannel, rightChannel, Absolute);
}

void NeoVolumeService::increaseVolume(int increment)
{
    adjustVolume(increment, increment, Relative);
}

void NeoVolumeService::decreaseVolume(int decrement)
{
    decrement *= -1;

    adjustVolume(decrement, decrement, Relative);
}

void NeoVolumeService::setMute(bool)
{
}

void NeoVolumeService::registerService()
{
    QtopiaIpcEnvelope e("QPE/AudioVolumeManager", "registerHandler(QString,QString)");
    e << QString("Headset") << QString("QPE/AudioVolumeManager/NeoVolumeService");

    QTimer::singleShot(0, this, SLOT(initVolumes()));
}

void NeoVolumeService::initVolumes()
{
    qLog(AudioState) << __PRETTY_FUNCTION__;

    initMixer();
    snd_mixer_elem_t *elem;
    long minPVolume;
    long maxPVolume;
    long volume;

    for (elem = snd_mixer_first_elem(mixerFd); elem; elem = snd_mixer_elem_next(elem)) {
        if (snd_mixer_elem_get_type(elem) == SND_MIXER_ELEM_SIMPLE &&
            snd_mixer_selem_is_active(elem)) {

            elemName = QString(snd_mixer_selem_get_name(elem));

            if (elemName == "Headphone") { // Master output
                snd_mixer_selem_get_playback_volume_range(elem, &minPVolume, &maxPVolume);

                m_adaptor->send(MESSAGE(setSpeakerVolumeRange(int,int)), (int)minPVolume, (int)maxPVolume);

                if(snd_mixer_selem_has_playback_volume(elem) > 0)
                    snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_FRONT_LEFT, &volume);

                m_adaptor->send(MESSAGE(setOutputVolume(int)), (int)volume);
            }

            if (elemName == "ALC Capture Target") { // mic??
                snd_mixer_selem_get_capture_volume_range(elem, &minPVolume, &maxPVolume);

                m_adaptor->send(MESSAGE(setMicVolumeRange(int,int)), (int)minPVolume, (int)maxPVolume);

                if (snd_mixer_selem_has_capture_volume(elem) > 0)
                    snd_mixer_selem_get_capture_volume(elem, SND_MIXER_SCHN_FRONT_LEFT, &volume);

                m_adaptor->send(MESSAGE(setMicVolume(int)), (int)volume);
            }
        }
    }

    closeMixer();
}

void NeoVolumeService::adjustVolume(int leftChannel, int rightChannel, AdjustType adjust)
{
    qLog(AudioState) << __PRETTY_FUNCTION__;

    unsigned int leftright = 0;
    int left;
    int right;

    if (adjust == Relative) {
        left = (leftright & 0xff00) >> 8;
        right = (leftright & 0x00ff);

        left = qBound(m_minOutputVolume, left + leftChannel, m_maxOutputVolume);
        right = qBound(m_minOutputVolume, right + rightChannel, m_maxOutputVolume);
    } else {
        left = leftChannel;
        right = rightChannel;
    }

    leftright = (left << 8) | right;
    leftright = (m_maxOutputVolume << 8) | m_maxOutputVolume;
    adjustSpeakerVolume(left, right);
}

void NeoVolumeService::adjustSpeakerVolume(int left, int right)
{
    initMixer();

    for (elem = snd_mixer_first_elem(mixerFd); elem; elem = snd_mixer_elem_next(elem)) {
        if (snd_mixer_elem_get_type(elem) == SND_MIXER_ELEM_SIMPLE &&
            snd_mixer_selem_is_active(elem)) {

            elemName = QString(snd_mixer_selem_get_name(elem));

            // Master output, could use PCM
            if (elemName == QLatin1String("Headphone")) { 
                if (snd_mixer_selem_has_playback_volume(elem) > 0) {
                    snd_mixer_selem_set_playback_volume(elem, SND_MIXER_SCHN_FRONT_LEFT, (long)&left);
                    snd_mixer_selem_set_playback_volume(elem, SND_MIXER_SCHN_FRONT_RIGHT, (long)&right);
                }
            }
        }
    }

    closeMixer();
    saveState();
}

void NeoVolumeService::adjustMicrophoneVolume(int volume)
{
    initMixer();

    for (elem = snd_mixer_first_elem(mixerFd); elem; elem = snd_mixer_elem_next(elem)) {
        if (snd_mixer_elem_get_type(elem) == SND_MIXER_ELEM_SIMPLE &&
            snd_mixer_selem_is_active(elem)) {

            elemName = QString(snd_mixer_selem_get_name(elem));

            // Mic Input
            if (elemName == "VLC Capture Target") { 
                if (snd_mixer_selem_has_playback_volume(elem) > 0)
                    snd_mixer_selem_set_capture_volume(elem,SND_MIXER_SCHN_FRONT_LEFT, (long)&volume);
            }
        }
    }

    closeMixer();
    saveState();
}

int NeoVolumeService::initMixer()
{
    int result;

    if ((result = snd_mixer_open(&mixerFd, 0)) < 0) {
        qWarning()<<"snd_mixer_open error"<< result;
        mixerFd = NULL;
        return result;
    }

    /*  hw:0 hw:0,0*/
    if ((result = snd_mixer_attach(mixerFd, "default")) < 0) {
        qWarning() << "snd_mixer_attach error" << result;
        snd_mixer_close(mixerFd);
        mixerFd = NULL;
        return result;
    }

    if ((result = snd_mixer_selem_register(mixerFd, NULL, NULL)) < 0) {
        qWarning() << "snd_mixer_selem_register error" << result;
        snd_mixer_close(mixerFd);
        mixerFd = NULL;
        return result;
    }

    if ((result = snd_mixer_load(mixerFd)) < 0) {
        qWarning() << "snd_mixer_load error" << result;
        snd_mixer_close(mixerFd);
        mixerFd = NULL;
        return result;
    }

    return result;
}

int NeoVolumeService::closeMixer()
{
    int result = snd_mixer_detach(mixerFd, "default");
    result = snd_mixer_close(mixerFd);
    //causes segfault
    //snd_mixer_free(mixerFd); 
    return 0;
}

int NeoVolumeService::saveState()
{
    QAudioStateConfiguration *audioState;
    audioState = new QAudioStateConfiguration(this);

    QString currentProfile(audioState->currentState().profile());
    QString m_mode;

    if(currentProfile == "MediaSpeaker"
       || currentProfile == "MediaHeadphones"
       || currentProfile == "RingToneSpeaker") {
        m_mode = "stereoout";
    }

    if (currentProfile == "PhoneSpeaker")
        m_mode = "gsmhandset";
    else if (currentProfile == "PhoneHeadphones")
        m_mode = "gsmheadset";
    else if (currentProfile == "PhoneBluetoothHeadset" /*|| "MediaBluetoothHeadset"*/)
        m_mode = "gsmbluetooth";

    QString confDir;
    if (QDir("/usr/share/openmoko/scenarios").exists())
        confDir="/usr/share/openmoko/scenarios/";
    else
        confDir="/etc/";

    QString cmd = "/usr/sbin/alsactl -f " + confDir + m_mode + ".state store";
    qLog(AudioState) << cmd;
    system(cmd.toLocal8Bit());
    return 0;
}

/*
  sets the vso to current amp mode from mixer
 */
void NeoVolumeService::changeAmpModeVS()
{
    char itemname[40];
    unsigned int item = 0;
    initMixer();

    for (elem = snd_mixer_first_elem(mixerFd); elem; elem = snd_mixer_elem_next(elem)) {
        if (snd_mixer_elem_get_type(elem) == SND_MIXER_ELEM_SIMPLE &&
            snd_mixer_selem_is_enumerated(elem) &&
            snd_mixer_selem_is_active(elem)) {

            elemName = QString(snd_mixer_selem_get_name(elem));
            if (elemName == "Amp Spk") {
                //current selection
                snd_mixer_selem_get_enum_item(elem, (snd_mixer_selem_channel_id_t)0, &item);
                snd_mixer_selem_get_enum_item_name(elem, item, sizeof(itemname) - 1, itemname);
                m_vsoVolumeObject->setAttribute("Amp",itemname);
            }
        }
      }
      closeMixer();
}

/*
set Amp spk on neo's audio card
*/
void NeoVolumeService::setAmp(bool mode)
{
    qLog(AudioState) << __PRETTY_FUNCTION__ << mode;
    QValueSpaceItem *device = new QValueSpaceItem("/Hardware/Neo/Device");

    if (device->value().toString() == "GTA01") {
        set1973Amp(mode);
        return;
    }

    QValueSpaceItem ampVS("/Hardware/Audio/Amp");
    QString ok = ampVS.value().toString();

    initMixer();
    unsigned int item = 0;

    for (elem = snd_mixer_first_elem(mixerFd); elem; elem = snd_mixer_elem_next(elem)) {
        if (snd_mixer_elem_get_type(elem) == SND_MIXER_ELEM_SIMPLE &&
             //   snd_mixer_selem_is_enumerated(elem) &&
             snd_mixer_selem_is_active(elem)) {

            elemName = QString(snd_mixer_selem_get_name(elem));

            if (elemName.contains("Amp Spk")) {
                qLog(AudioState) << "switch says" << elemName << mode;

                if (snd_mixer_selem_has_playback_switch(elem)) {
                    snd_mixer_selem_set_playback_switch(elem,
                                                        snd_mixer_selem_channel_id_t(item),
                                                        mode ? 1:0);
                    m_vsoVolumeObject->setAttribute("Amp",mode);
                }
            }
        }
    }

    closeMixer();
}

/*
  1973 needs this to switch off headphones
*/
void NeoVolumeService::set1973Amp(bool mode)
{
    qLog(AudioState) << __PRETTY_FUNCTION__ << mode;

    char itemname[40];
    unsigned int item = 0;
    QString elemName;
    QString currentMode;
    QValueSpaceItem ampVS("/Hardware/Audio/Amp");
    QString ampMode = ampVS.value().toString();

    m_vsoVolumeObject->setAttribute("Amp", mode);
    ampMode = mode ? "Stereo Speakers" : "Headphones";

    initMixer();

    for (elem = snd_mixer_first_elem(mixerFd); elem; elem = snd_mixer_elem_next(elem)) {
        if (snd_mixer_elem_get_type(elem) == SND_MIXER_ELEM_SIMPLE &&
            snd_mixer_selem_is_enumerated(elem) &&
            snd_mixer_selem_is_active(elem)) {

            elemName = QString(snd_mixer_selem_get_name(elem));
            if (elemName == "Amp Mode") {
                snd_mixer_selem_get_enum_item(elem, (snd_mixer_selem_channel_id_t)0, &item);
                snd_mixer_selem_get_enum_item_name(elem, item, sizeof(itemname) - 1, itemname);

                currentMode = itemname;

                int enumItems = snd_mixer_selem_get_enum_items(elem);
                if (enumItems < 0) {
                    qWarning() << "snd_mixer_selem_get_enum_items error " << enumItems;
                } else {
                    for (item = 0; item < (unsigned int)enumItems; item++) {
                        snd_mixer_selem_get_enum_item_name(elem, item, sizeof(itemname) - 1, itemname);
                        if (QString(itemname) == ampMode) {
                            snd_mixer_selem_set_enum_item(elem, (snd_mixer_selem_channel_id_t)0, item);
                            break;
                        }
                    }
                }
            }
        }
    }

    if (currentMode != "Off")
        ampMode = currentMode;
    m_vsoVolumeObject->setAttribute("Amp",ampMode);

    closeMixer();
}


void NeoVolumeService::toggleAmpMode()
{
    QValueSpaceItem ampVS("/Hardware/Audio/Amp");
    QString ampMode = ampVS.value().toString();

    if (ampMode == "Off")
        setAmp(false);
    else
        setAmp(true);
}

QTOPIA_TASK(NeoVolumeService, NeoVolumeService);

