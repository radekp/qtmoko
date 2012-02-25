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

#include "radiobandv4l.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <libv4l1-videodev.h>
#include <sys/soundcard.h>
#include <custom.h>
#include <QSettings>

// This file contains the Video4Linux 1 and 2 radio band handlers.

// The Radio.conf file can be used to override the default device settings.
// The default values are as follows:
//
//      [Device]
//      Name=/dev/radio                 Radio device to use
//      SignalDetectable=true           true if signal changes detectable
//      MuteViaVideo=false              true to mute via the video device
//      VideoName=/dev/video            Video device to use for muting
//      Mixer=radio                     Mixer to use to adjust radio volume

// Default radio device node to use.
#ifndef V4L_RADIO_DEVICE
#define V4L_RADIO_DEVICE            "/dev/radio"
#endif

// Default video device to use for the MuteViaVideo=true setting in Radio.conf.
#ifndef V4L_VIDEO_DEVICE
#define V4L_VIDEO_DEVICE            "/dev/video"
#endif

// Default mixer input to use to adjust the volume on the radio.
#ifndef V4L_RADIO_MIXER
#define V4L_RADIO_MIXER             "radio"
#endif

// Define this in custom.h to apply stricter checks on tuner types.
//#define V4L_STRICT_TUNER_CHECKS 1

RadioBandV4L::RadioBandV4L
        ( int fd, const QByteArray& deviceName, int tuner, bool v4l2,
          bool tunerLow, bool canClose,
          const QString& name, RadioBand::Frequency low,
          RadioBand::Frequency high, QObject *parent )
    : RadioBand( name, low, high, parent )
{
    this->_fd = fd;
    this->deviceName = deviceName;
    this->tuner = tuner;
    this->v4l2 = v4l2;
    this->tunerLow = tunerLow;
    this->canClose = canClose;
    this->_active = false;
    this->_muted = false;
    this->_frequency = low;

    QSettings settings( "Trolltech", "Radio" );
    settings.beginGroup( "Device" );
    this->muteViaVideo = settings.value( "MuteViaVideo" ).toBool();
    this->_signalDetectable =
        settings.value( "SignalDetectable", true ).toBool();

    // Read the current frequency from the device, if possible.
#ifdef HAVE_V4L2
    if ( v4l2 ) {
        // V4L2 has separate frequency tuners for each band.
        struct v4l2_frequency freq;
        memset( &freq, 0, sizeof( freq ) );
        freq.tuner = tuner;
        if ( ioctl( fd, VIDIOC_G_FREQUENCY, &freq ) >= 0 ) {
            if ( ((int)freq.frequency) != -1 ) {    // -1 means not set.
                if ( tunerLow ) {
                    _frequency = ((RadioBand::Frequency)freq.frequency)
                                        * 1000 / 16;
                } else {
                    _frequency =
                        ((RadioBand::Frequency)freq.frequency) * 1000000 / 16;
                }
            }
        }
    } else
#endif
    {
        // V4L1 has a single frequency setting, across all bands.
        // We need to select the band before we can get the frequency.
        struct video_tuner t;
        memset( &t, 0, sizeof(t) );
        t.tuner = tuner;
        if ( ioctl( fd, VIDIOCSTUNER, &t ) >= 0 ) {
            int freq = 0;
            if ( ioctl( fd, VIDIOCGFREQ, &freq ) >= 0 && freq != -1 ) {
                if ( tunerLow )
                    _frequency = ((RadioBand::Frequency)freq) * 1000 / 16;
                else
                    _frequency = ((RadioBand::Frequency)freq) * 1000000 / 16;
            }
        }
    }

    // Make sure that the frequency from the device makes sense.
    if ( _frequency < low || _frequency > high )
        _frequency = low;

    // If the frequency is at the extreme low end of the band, then it is
    // likely that the driver has been initialized for the first time and
    // it is not currently playing a previously set station.  Restore the
    // previously-saved value if there is one.
    if ( _frequency == low ) {
        QSettings settings( "Trolltech", "Radio" );
        settings.beginGroup( "Device" );
        int khz = settings.value
            ( "Band_" + QString::number( low / 1000 ), 0 ).toInt();
        if ( khz != 0 ) {
            _frequency = ((RadioBand::Frequency)khz) * 1000;
            if ( _frequency < low || _frequency > high )
                _frequency = low;
        }
    }

    // Force the frequency to be set: may be needed because we had to guess.
    _muted = true;
    setActive( true );
    setFrequency( _frequency );
    setActive( false );
    _muted = false;
}

RadioBandV4L::~RadioBandV4L()
{
    if ( canClose )
        ::close( _fd );

    // Save the current frequency for restoration next time if the
    // driver is started in an obviously invalid state.
    QSettings settings( "Trolltech", "Radio" );
    settings.beginGroup( "Device" );
    settings.setValue( "Band_" + QString::number( lowFrequency() / 1000 ),
                       (int)( _frequency / 1000 ) );
}

bool RadioBandV4L::createBands( QList<RadioBand *>& bands, QObject *parent )
{
    int fd;
    bool canClose;
    QByteArray deviceName;

    QSettings settings( "Trolltech", "Radio" );
    settings.beginGroup( "Device" );
    deviceName = settings.value( "Name" ).toString().toLatin1();
    if ( deviceName.isEmpty() )
        deviceName = V4L_RADIO_DEVICE;

    // Open the radio device.
    fd = open( deviceName.constData(), O_RDONLY, 0 );
    if ( fd < 0 ) {
        perror( deviceName.constData() );
        return false;
    }
    fcntl( fd, F_SETFD, FD_CLOEXEC );
    canClose = true;

    // Create a volume control object.
    RadioBandV4LVolume *volume = new RadioBandV4LVolume();

    // Get general capability information, and determine if this is a
    // V4L1 or V4L2 device driver.
#ifdef HAVE_V4L2
    struct v4l2_capability cap;
    memset( &cap, 0, sizeof( cap ) );
    if ( ioctl( fd, VIDIOC_QUERYCAP, &cap ) >= 0 ) {

#ifdef V4L_STRICT_TUNER_CHECKS  // Some devices set these flags badly.

        // V4L2 device driver.
        if ( ( cap.capabilities & V4L2_CAP_RADIO ) == 0 ) {
            close( fd );
            fd = -1;
            return false;
        }
        if ( ( cap.capabilities & V4L2_CAP_TUNER ) == 0 ) {
            close( fd );
            fd = -1;
            return false;
        }

#endif

        // Find the number of inputs (tuners) that we have.
        struct v4l2_input input;
        int num_tuners = 0;
        for ( ;; ) {
            memset( &input, 0, sizeof( input ) );
            input.index = num_tuners;
            if ( ioctl( fd, VIDIOC_ENUMINPUT, &input ) < 0 )
                break;
            ++num_tuners;
        }

        // Some devices report 0 tuners via VIDIOC_ENUMINPUT.
        // To work around this, we let the next loop scan a
        // number of likely tuner indexes until some are found.
        // Hopefully VIDIOC_G_TUNER will fail for bad indexes.
        if ( !num_tuners )
            num_tuners = 8;

        // Get the available bands.
        for ( int index = 0; index < num_tuners; ++index ) {
            struct v4l2_tuner tuner;
            memset( &tuner, 0, sizeof( tuner ) );
            tuner.index = index;
            if ( ioctl( fd, VIDIOC_G_TUNER, &tuner ) < 0 )
                continue;

#ifdef V4L_STRICT_TUNER_CHECKS  // Some devices set these flags badly.

            // Skip non-radio tuner types.
            if ( tuner.type != V4L2_TUNER_RADIO )
                continue;

#endif

            // Get the high and low frequencies for the band.
            RadioBand::Frequency low, high;
            bool tunerLow;
            if ( ( tuner.capability & V4L2_TUNER_CAP_LOW ) != 0 ) {
                // Units are 1/16th of a kHz.
                tunerLow = true;
                low = ((RadioBand::Frequency)(tuner.rangelow)) * 1000 / 16;
                high = ((RadioBand::Frequency)(tuner.rangehigh)) * 1000 / 16;
            } else {
                // Units are 1/16th of a MHz.
                tunerLow = false;
                low = ((RadioBand::Frequency)(tuner.rangelow)) * 1000000 / 16;
                high = ((RadioBand::Frequency)(tuner.rangehigh)) * 1000000 / 16;
            }

            // Create an object for the band.
            RadioBandV4L *newBand = new RadioBandV4L
                ( fd, deviceName, index, true, tunerLow, canClose,
                  (char *)tuner.name, low, high, parent );
            newBand->volumeControl = volume;
            if ( canClose )
                volume->setParent( newBand );
            bands.append( newBand );
            canClose = false;
        }

    } else
#endif /* HAVE_V4L2 */
    {

        // V4L1 device driver.
        struct video_capability cap;
        if ( ioctl( fd, VIDIOCGCAP, &cap ) < 0 ) {
            close( fd );
            fd = -1;
            return false;
        }

#ifdef V4L_STRICT_TUNER_CHECKS  // Some devices set these flags badly.

        if ( ( cap.type & VID_TYPE_TUNER ) == 0 ) {
            close( fd );
            fd = -1;
            return false;
        }

#endif

        // Get the available bands.
        for ( int id = 0; id < cap.channels; ++id ) {
            struct video_tuner tuner;
            tuner.tuner = id;
            if ( ioctl( fd, VIDIOCGTUNER, &tuner ) < 0 )
                continue;

            // Get the high and low frequencies for the band.
            RadioBand::Frequency low, high;
            bool tunerLow;
            if ( ( tuner.flags & VIDEO_TUNER_LOW ) != 0 ) {
                // Units are 1/16th of a kHz.
                tunerLow = true;
                low = ((RadioBand::Frequency)(tuner.rangelow)) * 1000 / 16;
                high = ((RadioBand::Frequency)(tuner.rangehigh)) * 1000 / 16;
            } else {
                // Units are 1/16th of a MHz.
                tunerLow = false;
                low = ((RadioBand::Frequency)(tuner.rangelow)) * 1000000 / 16;
                high = ((RadioBand::Frequency)(tuner.rangehigh)) * 1000000 / 16;
            }

            // Create an object for the band.
            RadioBandV4L *newBand = new RadioBandV4L
                ( fd, deviceName, id, false, tunerLow, canClose, tuner.name,
                  low, high, parent );
            newBand->volumeControl = volume;
            if ( canClose )
                volume->setParent( newBand );
            bands.append( newBand );
            canClose = false;
        }
    }

    // Clean up the volume object if we didn't create any bands.
    if ( canClose )
        delete volume;

    // Indicate to the caller if we added some bands to the list.
    return !canClose;
}

bool RadioBandV4L::active() const
{
    return _active;
}

bool RadioBandV4L::muted() const
{
    return _muted;
}

RadioBand::Frequency RadioBandV4L::frequency() const
{
    return _frequency;
}

bool RadioBandV4L::signal() const
{
    if ( !_active )
        return false;
#ifdef HAVE_V4L2
    if ( v4l2 ) {
        struct v4l2_tuner tuner;
        tuner.index = this->tuner;
        if ( ioctl( fd(), VIDIOC_G_TUNER, &tuner ) < 0 )
            return false;
        return ( tuner.signal != 0 );
    } else
#endif
    {
        struct video_tuner tuner;
        tuner.tuner = this->tuner;
        if ( ioctl( fd(), VIDIOCGTUNER, &tuner ) < 0 )
            return false;
        return ( tuner.signal != 0 );
    }
}

bool RadioBandV4L::stereo() const
{
    if ( !_active )
        return false;
#ifdef HAVE_V4L2
    if ( v4l2 ) {
        struct v4l2_tuner tuner;
        tuner.index = this->tuner;
        if ( ioctl( fd(), VIDIOC_G_TUNER, &tuner ) < 0 )
            return false;
        return ( ( tuner.rxsubchans & V4L2_TUNER_SUB_STEREO ) != 0 );
    } else
#endif
    {
        struct video_tuner tuner;
        tuner.tuner = this->tuner;
        if ( ioctl( fd(), VIDIOCGTUNER, &tuner ) < 0 )
            return false;
        return ( ( tuner.flags & VIDEO_TUNER_STEREO_ON ) != 0 );
    }
}

bool RadioBandV4L::rds() const
{
    if ( !_active )
        return false;
#ifdef HAVE_V4L2
    if ( v4l2 ) {
        // Don't know how to detect RDS in V4L2.
        return false;
    } else
#endif
    {
        struct video_tuner tuner;
        tuner.tuner = this->tuner;
        if ( ioctl( fd(), VIDIOCGTUNER, &tuner ) < 0 )
            return false;
        return ( ( tuner.flags & VIDEO_TUNER_RDS_ON ) != 0 );
    }
}

bool RadioBandV4L::signalDetectable() const
{
    return _signalDetectable;
}

int RadioBandV4L::volume() const
{
    return volumeControl->volume();
}

void RadioBandV4L::setActive( bool value )
{
    if ( _active == value )
        return;
    _active = value;
    if ( value ) {
#ifdef HAVE_V4L2
        if ( v4l2 ) {
            // We need to get the tuner's current capabilities before
            // setting so that the driver doesn't change the audio
            // settings, only the index.
            struct v4l2_tuner tuner;
            tuner.index = this->tuner;
            if ( ioctl( fd(), VIDIOC_G_TUNER, &tuner ) >= 0 )
                ioctl( fd(), VIDIOC_S_TUNER, &tuner );
            unsigned int index = (unsigned int)(this->tuner);
            ioctl( fd(), VIDIOC_S_INPUT, &index );
        } else
#endif
        {
            struct video_tuner tuner;
            tuner.tuner = this->tuner;
            ioctl( fd(), VIDIOCSTUNER, &tuner );
        }
    }
    updateMute();
}

void RadioBandV4L::setMuted( bool value )
{
    if ( !_active ) {
        _muted = value;
    } else if ( _muted != value ) {
        _muted = value;
        updateMute();
    }
}

void RadioBandV4L::setFrequency( RadioBand::Frequency value )
{
    // Normalize the frequency to a multiple of the step value.
    RadioBand::Frequency step = scanStep();
    if ( ( value % step ) != 0 ) {
        if ( ( value % step ) < (step / 2) ) {
            value -= value % step;
        } else {
            value += step - value % step;
        }
    }

    // Record the frequency value for later.
    _frequency = value;

    // If this band is not currently active, then nothing more to do.
    if ( !_active )
        return;

    // Set the frequency on the device.
#ifdef HAVE_V4L2
    if ( v4l2 ) {
        // Get the current settings and then modify just the frequency.
        struct v4l2_frequency freq;
        memset( &freq, 0, sizeof( freq ) );
        freq.tuner = tuner;
        if ( ioctl( fd(), VIDIOC_G_FREQUENCY, &freq ) < 0 )
            return;
        if ( tunerLow )
            freq.frequency = (int)(((value * 16) + 500) / 1000);
        else
            freq.frequency = (int)(((value * 16) + 500000) / 1000000);
        ioctl( fd(), VIDIOC_S_FREQUENCY, &freq );
    } else
#endif
    {
        int freq;
        if ( tunerLow )
            freq = (int)(((value * 16) + 500) / 1000);
        else
            freq = (int)(((value * 16) + 500000) / 1000000);
        ioctl( fd(), VIDIOCSFREQ, &freq );
    }
}

void RadioBandV4L::adjustVolume( int diff )
{
    volumeControl->adjustVolume( diff );
}

void RadioBandV4L::updateMute()
{
    bool value = ( !_active || _muted );
#ifdef HAVE_V4L2
    if ( v4l2 ) {
        // Query the mute control's properties.  Bail out if no mute.
        struct v4l2_queryctrl queryctrl;
        memset( &queryctrl, 0, sizeof( queryctrl ) );
        queryctrl.id = V4L2_CID_AUDIO_MUTE;
        if ( ioctl( fd(), VIDIOC_QUERYCTRL, &queryctrl ) < 0 )
            return;

        // Change the mute control's value.
        struct v4l2_control control;
        memset( &control, 0, sizeof( control ) );
        control.id = V4L2_CID_AUDIO_MUTE;
        control.value = (value ? queryctrl.maximum : queryctrl.minimum );
        ioctl( fd(), VIDIOC_S_CTRL, &control );

        // Update the mute via the video device for broken drivers.
        unsigned int input;
        if ( muteViaVideo ) {
            if ( value ) {
                // Shut down the radio device, because if it is open the
                // broken device will unmute again.  It will be automatically
                // re-opened upon the next call to fd() (usually the unmute).
                shutdownfd();

                // Muting radio, so mute the video and switch to its input.
                QSettings settings( "Trolltech", "Radio" );
                settings.beginGroup( "Device" );
                QByteArray device =
                    settings.value( "VideoName" ).toString().toLatin1();
                if ( device.isEmpty() )
                    device = V4L_VIDEO_DEVICE;
                int vfd = open( device.constData(), O_RDONLY, 0 );
                if ( vfd >= 0 ) {
                    memset( &queryctrl, 0, sizeof( queryctrl ) );
                    queryctrl.id = V4L2_CID_AUDIO_MUTE;
                    if ( ioctl( vfd, VIDIOC_QUERYCTRL, &queryctrl ) >= 0 ) {
                        memset( &control, 0, sizeof( control ) );
                        control.id = V4L2_CID_AUDIO_MUTE;
                        control.value = queryctrl.maximum;
                        ioctl( vfd, VIDIOC_S_CTRL, &control );
                    }
                    input = 0;
                    ioctl( vfd, VIDIOC_S_INPUT, &input );
                    close( vfd );
                }
            } else {
                // Unmuting radio, so make sure the radio input is selected.
                struct v4l2_tuner tuner;
                tuner.index = this->tuner;
                if ( ioctl( fd(), VIDIOC_G_TUNER, &tuner ) >= 0 )
                    ioctl( fd(), VIDIOC_S_TUNER, &tuner );
                else
                    perror( "ioctl" );
                input = (unsigned int)(this->tuner);
                ioctl( fd(), VIDIOC_S_INPUT, &input );
            }
        }
    } else
#endif
    {
        struct video_audio audio;
        audio.audio = 0;        // Use audio channel 0 to access the mute.
        if ( ioctl( fd(), VIDIOCGAUDIO, &audio ) < 0 )
            return;
        if ( ( audio.flags & VIDEO_AUDIO_MUTABLE ) == 0 )
            return;
        audio.audio = 0;
        audio.flags = ( value ? VIDEO_AUDIO_MUTE : 0 );
        ioctl( fd(), VIDIOCSAUDIO, &audio );
    }
}

int RadioBandV4L::fd() const
{
    if ( _fd < 0 ) {
        ((RadioBandV4L *)this)->_fd =
            open( deviceName.constData(), O_RDONLY, 0 );
    }
    return _fd;
}

void RadioBandV4L::shutdownfd()
{
    if ( _fd >= 0 ) {
        close( _fd );
        _fd = -1;
    }
}

// Get the number of the mixer to use for volume adjustments.
static int mixerNumber()
{
    QSettings settings( "Trolltech", "Radio" );
    settings.beginGroup( "Device" );
    QString name = settings.value( "Mixer" ).toString();
    if ( name.isEmpty() )
        name = V4L_RADIO_MIXER;

    static const char * const mixerNames[] = SOUND_DEVICE_NAMES;
    #define numMixerNames   ((int)(sizeof(mixerNames) / sizeof(const char *)))
    for ( int index = 0; index < numMixerNames; ++index ) {
        if ( name == mixerNames[index] )
            return index;
    }
    return SOUND_MIXER_RADIO; // Default mixer if otherwise unknown.
}

RadioBandV4LVolume::RadioBandV4LVolume( QObject *parent )
    : QObject( parent )
{
    int fd = ::open( "/dev/mixer", O_RDWR, 0 );
    if ( fd >= 0 ) {
        int volume = 0;
        ::ioctl( fd, MIXER_READ(mixerNumber()), &volume );
        int left = ( volume & 0xFF );
        int right = ( ( volume >> 8 ) & 0xFF );
        if ( left > right )
            savedVolume = left;
        else
            savedVolume = right;
        ::close( fd );
    }
}

RadioBandV4LVolume::~RadioBandV4LVolume()
{
}

int RadioBandV4LVolume::volume() const
{
    return savedVolume;
}

void RadioBandV4LVolume::adjustVolume( int diff )
{
    int fd = ::open( "/dev/mixer", O_RDWR, 0 );
    if ( fd < 0 )
        return;
    int volume = savedVolume + diff;
    if ( volume < 0 )
        volume = 0;
    else if ( volume > 100 )
        volume = 100;
    savedVolume = volume;
    volume += volume << 8;
    ::ioctl( fd, MIXER_WRITE(mixerNumber()), &volume );
    ::close( fd );
}
