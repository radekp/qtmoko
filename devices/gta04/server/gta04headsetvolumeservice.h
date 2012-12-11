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

#ifndef __QTOPIA_MEDIA_DEFAULTVOLUMEPROVIDER_H
#define __QTOPIA_MEDIA_DEFAULTVOLUMEPROVIDER_H

#include <QtopiaIpcAdaptor>

#include <alsa/asoundlib.h>

class GTA04HeadsetVolumeService : public QtopiaIpcAdaptor
{
    Q_OBJECT
    enum AdjustType { Relative, Absolute };

public:
    GTA04HeadsetVolumeService();
    ~GTA04HeadsetVolumeService();

public slots:
    void setVolume(int volume);
    void setVolume(int leftChannel, int rightChannel);
    void increaseVolume(int increment);
    void decreaseVolume(int decrement);
    void setMute(bool mute);

private slots:
    void registerService();

private:
    void adjustVolume(int leftChannel, int rightChannel, AdjustType);

protected:
    snd_mixer_t *mixerFd;
    snd_mixer_elem_t *elem;

    int m_minOutputVolume;
    int m_maxOutputVolume;

    int initMixer();
    int closeMixer();
    int saveState();
};

#endif  // __QTOPIA_MEDIA_DEFAULTVOLUMEPROVIDER_H
