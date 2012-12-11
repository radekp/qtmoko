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

/* The claim of this class is that if a headset is plugged into the
   phone, and there are hardware keys on that headset for increasing
   and decreasing the headset volume, and those hardware keys are
   mapped through to Qt as Qt::Key_VolumeUp and Qt::Key_VolumeDown -
   then this class knows how to achieve the desired increase or
   decrease in headset volume.

   It does this by registering for the "Headset" domain and
   implementing increaseVolume() and decreaseVolume() slots.

   Currently on the GTA04 I believe there are a few problems / missing
   pieces that prevent this from really working:

   1. The mixer-setting code needs adjusting to use an appropriate and
   precise GTA04 mixer control name.  There aren't any GTA04 controls
   with "Headphone" in their name.

   2. There isn't any code that sets the volume domain to "Headset".
   Logically I think this should be in the place that detects headset
   jack insertion.

   3. I don't know if there's code to map headset hardware keys to
   Qt::Key_VolumeUp and Qt::Key_VolumeDown.

   I doubt any of this is a priority to fix, because I don't recall
   any GTA04 owners talking about headset hardware keys.  But I think
   it is worth recording what I believe the point of this code is, in
   the context of trying to understand and improve the audio system
   overall.

   - Neil Jerram 2012-11-25 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>

#include <QTimer>
#include <QDebug>
#include <qtopiaserverapplication.h>
#include <qtopiaipcenvelope.h>

#include <QAudioStateConfiguration>
#include <QAudioStateInfo>

#include "gta04headsetvolumeservice.h"

GTA04HeadsetVolumeService::GTA04HeadsetVolumeService()
    : QtopiaIpcAdaptor("QPE/AudioVolumeManager/GTA04HeadsetVolumeService")
{
    publishAll(Slots);
    qLog(AudioState) << __PRETTY_FUNCTION__;

    QTimer::singleShot(0, this, SLOT(registerService()));
}

GTA04HeadsetVolumeService::~GTA04HeadsetVolumeService()
{
}

void GTA04HeadsetVolumeService::setVolume(int volume)
{
    adjustVolume(volume, volume, Absolute);
}

void GTA04HeadsetVolumeService::setVolume(int leftChannel, int rightChannel)
{
    adjustVolume(leftChannel, rightChannel, Absolute);
}

void GTA04HeadsetVolumeService::increaseVolume(int increment)
{
    adjustVolume(increment, increment, Relative);
}

void GTA04HeadsetVolumeService::decreaseVolume(int decrement)
{
    decrement *= -1;

    adjustVolume(decrement, decrement, Relative);
}

void GTA04HeadsetVolumeService::setMute(bool)
{
}

void GTA04HeadsetVolumeService::registerService()
{
    QtopiaIpcEnvelope e("QPE/AudioVolumeManager", "registerHandler(QString,QString)");
    e << QString("Headset") << QString("QPE/AudioVolumeManager/GTA04HeadsetVolumeService");
}

void GTA04HeadsetVolumeService::adjustVolume(int leftChannel, int rightChannel, AdjustType adjust)
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

    initMixer();

    for (elem = snd_mixer_first_elem(mixerFd); elem; elem = snd_mixer_elem_next(elem)) {
        if (snd_mixer_elem_get_type(elem) == SND_MIXER_ELEM_SIMPLE &&
            snd_mixer_selem_is_active(elem)) {

            QString elemName = QString(snd_mixer_selem_get_name(elem));

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

int GTA04HeadsetVolumeService::initMixer()
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

int GTA04HeadsetVolumeService::closeMixer()
{
    int result = snd_mixer_detach(mixerFd, "default");
    result = snd_mixer_close(mixerFd);
    //causes segfault
    //snd_mixer_free(mixerFd); 
    return 0;
}

int GTA04HeadsetVolumeService::saveState()
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

QTOPIA_TASK(GTA04HeadsetVolumeService, GTA04HeadsetVolumeService);
