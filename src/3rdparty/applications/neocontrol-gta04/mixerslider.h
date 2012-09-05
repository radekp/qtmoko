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

#ifndef MIXERSLIDER_H
#define MIXERSLIDER_H

#include <qslider.h>
#include <alsa/asoundlib.h>

class MixerSlider : public QSlider
{
    Q_OBJECT
public:
    MixerSlider(QWidget *parent);
    ~MixerSlider();
    void setMixerElem(snd_mixer_elem_t *snd_mix, bool playback);
    bool sliding;
    long volume;

private slots:
    void setSndCurrentValue(int value);
    void sliderPressed();
    void sliderReleased();

private:
    snd_mixer_elem_t *snd_mix;
    long min, max;
    bool updatingValue;
    bool playback;
};

#endif /* MIXERSLIDER_H */
