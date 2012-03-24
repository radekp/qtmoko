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

#include <math.h>

#include <QList>
#include <QTimer>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QSemaphore>
#include <QMediaDecoder>
#include <QAudioOutput>
#include <QDebug>
#include <QFile>

#include <qtopialog.h>

#include <custom.h>

#include "cruxusoutputthread.h"
#include "audioresampler.h"

//#define DEBUG_ENGINE

namespace cruxus
{

// {{{ OutputThreadPrivate
class OutputThreadPrivate : public QThread
{
    Q_OBJECT
public:

    static const int MAX_VOLUME = 100;

#ifdef CRUXUS_OUTPUT_FREQ
    static const int request_frequency = CRUXUS_OUTPUT_FREQ;
#else
    static const int request_frequency = 44100;
#endif

    static const int request_bitsPerSample = 16;
    static const int request_channels = 2;

#ifdef CRUXUS_FRAMESIZE
    static const int frame_milliseconds = CRUXUS_FRAMESIZE;
#else
    static const int frame_milliseconds = 100;
#endif
    static const int max_silence_duration = 60;

    static const int silence_frame_duration = 20;

    static const int default_frame_size = (48000 / (1000 / frame_milliseconds)) *
                                          (request_bitsPerSample / 8) *
                                          request_channels;

    bool                    opened;
    bool                    running;
    bool                    quit;
    bool                    paused;
    int                     silenceDuration;
    QMutex                  mutex;
    QWaitCondition          condition;
    QAudioOutput*           audioOutput;
    QMediaDevice::Info      inputInfo;
    QList<QMediaDevice*>    activeSessions;
    QList<AudioResampler*>  audioResamplers;

    char    mixbuf[default_frame_size];

    void suspend();
    void resume();
    void close();

protected:
    void run();

private:
    int readFromDevice(QMediaDevice* device, QMediaDevice::Info const& info, char* working);
    int resampleAndMix(AudioResampler* audioResampler, QMediaDevice::Info const& deviceInfo, char* src, int dataAmt, bool first);
    qint32 addSamples(qint32 s1,qint32 s2);

};

void OutputThreadPrivate::run()
{
    unsigned long   timeout = 30000;

    quit = false;
    paused = false;
    silenceDuration = max_silence_duration;

    audioOutput = new QAudioOutput;

    audioOutput->setFrequency(request_frequency);
    audioOutput->setChannels(request_channels);
    audioOutput->setBitsPerSample(request_bitsPerSample);

    inputInfo.type = QMediaDevice::Info::PCM;
    inputInfo.frequency = audioOutput->frequency();
    inputInfo.bitsPerSample = audioOutput->bitsPerSample();
    inputInfo.channels = audioOutput->channels();

    qLog(Media) << "OutputThreadPrivate::run(); opened device with " <<
                inputInfo.frequency << inputInfo.bitsPerSample << inputInfo.channels;
#if defined(DEBUG_ENGINE)
    QFile* file = new QFile("/tmp/test.raw");
#endif

    do
    {
        QMutexLocker    conditionLock(&mutex);

        int             sc = activeSessions.size();

        if (sc == 0 && silenceDuration >= max_silence_duration) {
            // No sessions to process, added silence, we are finished
            if(opened)
                audioOutput->close();
	    opened = false;
	    // Set next wait to never timeout
	    timeout = ULONG_MAX;
        } else {
            if (!paused) {
                // Active session, process some data
                if(!audioOutput->isOpen()) {
                    qLog(Media) <<"OutputThreadPrivate::run(); open output";
                    audioOutput->open(QIODevice::ReadWrite|QIODevice::Unbuffered);
                }

                bool    first = true;
                int     mixLength = 0;
                char    working[default_frame_size*8];

                for (int i = 0; i < sc; ++i) {
                    QMediaDevice* input = activeSessions.at(i);
                    QMediaDevice::Info const& info = input->dataType();
                    AudioResampler *resampler = audioResamplers[i];

                    int read = readFromDevice(input, info, working);


                    if (read > 0) {
                        mixLength = qMax(resampleAndMix(resampler, info, working, read, first), mixLength);
                        first = false;
                    }
                    else {
                        activeSessions.removeAt(i);
                        delete audioResamplers[i];
                        audioResamplers.removeAt(i);
                        --sc;
                        --i;
                    }
                }

                if (mixLength > 0) {
                    silenceDuration = 0;
                } else {
                    if ( silenceDuration < max_silence_duration ) {
                        silenceDuration += silence_frame_duration;
                        int  expectedBufferSize = (inputInfo.frequency / (1000 / silence_frame_duration)) *
                                                  (inputInfo.bitsPerSample/8) * inputInfo.channels;

                        mixLength = expectedBufferSize; //use shorter frames for silence
                        memset( mixbuf, 0, mixLength );
                    }
                }


                if (mixLength > 0) {

#if defined(DEBUG_ENGINE)
                    file->open(QIODevice::WriteOnly | QIODevice::Append );
                    file->write(mixbuf, mixLength);
                    file->close();
#endif
                    audioOutput->write( mixbuf, mixLength );
                }
                if(silenceDuration >= max_silence_duration)
                    timeout = activeSessions.size() > 0 ? 0 : 30000;
                else
                    timeout = 0;
            } else {
                // Has been suspended...
                silenceDuration = max_silence_duration;
                qLog(Media) <<"OutputThreadPrivate::run(); wait";
            }
        }
	condition.wait(&mutex,timeout);

    } while (!quit);

    delete audioOutput;
    audioOutput = 0;

    qLog(Media) << "OutputThreadPrivate::run(); exiting";
}

void OutputThreadPrivate::suspend()
{
    paused = true;
    if(audioOutput && opened) {
        QMutexLocker lock(&mutex);
        opened = false;
        audioOutput->close();
    }
    qLog(Media) << "OutputThreadPrivate::suspend()";
}

void OutputThreadPrivate::resume()
{
    if(audioOutput && !opened && !activeSessions.isEmpty() )
        opened = audioOutput->open(QIODevice::ReadWrite | QIODevice::Unbuffered);
    silenceDuration = max_silence_duration;
    paused = false;
    qLog(Media) << "OutputThreadPrivate::resume()";
}

void OutputThreadPrivate::close()
{
    if(audioOutput && opened) {
        QMutexLocker lock(&mutex);
        opened = false;
        audioOutput->close();
    }
    qLog(Media) << "OutputThreadPrivate::close()";
}

inline int OutputThreadPrivate::readFromDevice
(
 QMediaDevice* device,
 QMediaDevice::Info const& info,
 char* working
)
{
#ifndef ALSA_PERIOD_SIZE
    int requestedDataSize = ( info.frequency / (1000 / frame_milliseconds)) *
                                 (info.bitsPerSample / 8) *
                                 info.channels;
#else
    int requestedDataSize = ALSA_PERIOD_SIZE *
                            (info.bitsPerSample/8) * info.channels;
#endif

    return device->read(working, requestedDataSize);
}

int OutputThreadPrivate::resampleAndMix
(
 AudioResampler* audioResampler,
 QMediaDevice::Info const& deviceInfo,
 char* src,
 int dataAmt,
 bool first
)
{
    int converted = 0;

    if ( deviceInfo.bitsPerSample == 16 && deviceInfo.volume == MAX_VOLUME ) {
        converted = audioResampler->resample( (const qint16*)src, (qint16*)mixbuf, dataAmt/2, !first );
    } else {
        int samplesCount = dataAmt * 8 / deviceInfo.bitsPerSample;
        qint16 samples[samplesCount];

        switch ( deviceInfo.bitsPerSample ) {
        case 8:
            for ( int i=0; i<samplesCount; i++ )
                samples[i] = (int(src[i]) - 128)*256;
            break;
        case 16:
            memcpy( samples, src, dataAmt );
            break;
        case 32:
            {
                int *srcSamples = (int*)src;
                for ( int i=0; i<samplesCount; i++ )
                    samples[i] = srcSamples[i]; // is this correct?
            }
            break;
        default:
            qWarning() << deviceInfo.bitsPerSample << "bits per sample is not supported";
            memset( samples, 0, sizeof(samples) );
        }

        if ( deviceInfo.volume < MAX_VOLUME ) {
            for ( int i=0; i<samplesCount; i++ )
                samples[i] = qint32(samples[i])*deviceInfo.volume/MAX_VOLUME;
        }

        converted = audioResampler->resample( samples, (qint16*)mixbuf, samplesCount, !first );
    }

    return converted*2;
}
// }}}

// {{{ OutputThread

/*!
    \class cruxus::OutputThread
    \internal
*/

OutputThread::OutputThread():
    d(new OutputThreadPrivate)
{
    qLog(Media) << "OutputThread::OutputThread()";
    d->opened = false;
    d->paused = false;
    d->start(QThread::HighPriority);
}

OutputThread::~OutputThread()
{
    QMutexLocker    lock(&d->mutex);

    qLog(Media) << "OutputThread::~OutputThread()";

    d->quit = true;

    d->condition.wakeOne();
    d->wait();

    delete d;
}

QMediaDevice::Info const& OutputThread::dataType() const
{
    Q_ASSERT(false);        // Should never be called

    QMutexLocker    lock(&d->mutex);
    return d->inputInfo;
}

bool OutputThread::connectToInput(QMediaDevice* input)
{
    QMutexLocker    lock(&d->mutex);

    if (input->dataType().type != QMediaDevice::Info::PCM)
        return false;

    connect(input, SIGNAL(readyRead()), SLOT(deviceReady()));
    return true;
}

void OutputThread::disconnectFromInput(QMediaDevice* input)
{
    QMutexLocker    lock(&d->mutex);

    input->disconnect(this);

    for ( int i=0; i<d->activeSessions.count(); i++ ) {
        if ( d->activeSessions[i] == input ) {
            d->activeSessions.removeAt(i);
            delete d->audioResamplers[i];
            d->audioResamplers.removeAt(i);
        }
    }
}

bool OutputThread::open(QIODevice::OpenMode mode)
{
    QMutexLocker    lock(&d->mutex);

    if (!d->opened)
        d->opened = QIODevice::open(QIODevice::WriteOnly | QIODevice::Unbuffered);

    return d->opened;

    Q_UNUSED(mode);
}

void OutputThread::close()
{
    d->close();
}

// private slots:
void OutputThread::deviceReady()
{
    QMutexLocker    lock(&d->mutex);

    if(!d->opened) {
        d->audioOutput->open(QIODevice::ReadWrite | QIODevice::Unbuffered);
        d->opened = true;
    }

    QMediaDevice* mediaDevice = qobject_cast<QMediaDevice*>(sender());
    QMediaDevice::Info const& info = mediaDevice->dataType();


    AudioResampler *resampler = new AudioResampler( info.frequency, info.channels,
                                                    d->request_frequency, d->frame_milliseconds );

    d->activeSessions.append( mediaDevice );
    d->audioResamplers.append( resampler );

    d->condition.wakeOne();
}

void OutputThread::suspend()
{
    d->suspend();
}

void OutputThread::resume()
{
    d->resume();
}


// private:
qint64 OutputThread::readData(char *data, qint64 maxlen)
{
    Q_UNUSED(data);
    Q_UNUSED(maxlen);

    return 0;
}

qint64 OutputThread::writeData(const char *data, qint64 len)
{
    Q_UNUSED(data);
    Q_UNUSED(len);

    return 0;
}
// }}}

}   // ns cruxus

#include "cruxusoutputthread.moc"



