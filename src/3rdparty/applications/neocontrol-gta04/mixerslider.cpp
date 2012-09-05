/*
 *  Jeremy Compostella <jeremy.compostella@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or (at
 *  your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 */

#include "mixerslider.h"
#include <qdebug.h>

MixerSlider::MixerSlider(QWidget *parent)
    : QSlider( Qt::Horizontal, parent),
    sliding(false),
    snd_mix(NULL),
    updatingValue(false),
    playback(true)
{
    connect(this, SIGNAL(valueChanged(int)),
            this, SLOT(setSndCurrentValue(int)));

    connect(this, SIGNAL(sliderPressed()),
            this, SLOT(sliderPressed()));

    connect(this, SIGNAL(sliderReleased()),
            this, SLOT(sliderReleased()));

    setPageStep(1);
}

MixerSlider::~MixerSlider()
{
}

void MixerSlider::setMixerElem(snd_mixer_elem_t *snd_mix, bool playback)
{
    this->snd_mix = snd_mix;
    this->playback = playback;

    if(snd_mix == NULL)
    {
        setEnabled(false);
        return;
    }
    setEnabled(true);

    updatingValue = true;

    setObjectName(QString(snd_mixer_selem_get_name(snd_mix)));

    if(playback)
    {
        snd_mixer_selem_get_playback_volume_range(snd_mix, &min, &max);
    }
    else
    {
        snd_mixer_selem_get_capture_volume_range(snd_mix, &min, &max);
    }

    setMinimum(min);
    setMaximum(max);

    if(playback)
    {
        snd_mixer_selem_get_playback_volume
                (snd_mix, SND_MIXER_SCHN_FRONT_LEFT, &volume);
    }
    else
    {
        snd_mixer_selem_get_capture_volume
                (snd_mix, SND_MIXER_SCHN_FRONT_LEFT, &volume);
    }

    setSliderPosition(volume);
    updatingValue = false;
}

void MixerSlider::setSndCurrentValue(int value)
{
    if(snd_mix == NULL || updatingValue)
    {
        return;
    }

    if(playback)
    {
        snd_mixer_selem_set_playback_volume
                (snd_mix, SND_MIXER_SCHN_FRONT_LEFT, value);
        snd_mixer_selem_set_playback_volume
                (snd_mix, SND_MIXER_SCHN_FRONT_RIGHT, value);
    }
    else
    {
        snd_mixer_selem_set_capture_volume
                (snd_mix, SND_MIXER_SCHN_FRONT_LEFT, value);
        snd_mixer_selem_set_capture_volume
                (snd_mix, SND_MIXER_SCHN_FRONT_RIGHT, value);
    }
}

void MixerSlider::sliderPressed()
{
    sliding = true;
}

void MixerSlider::sliderReleased()
{
    sliding = false;
}
