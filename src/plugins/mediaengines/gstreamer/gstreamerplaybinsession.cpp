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

#include <QDebug>
#include <QVideoSurface>
#include <QMediaVideoControlServer>
#include <qtopiavideo.h>

#include <private/qmediahandle_p.h>

#include "gstreamerbushelper.h"
#include "gstreamermessage.h"
#include "gstreamervideowidget.h"

#include "gstreamerplaybinsession.h"

#include <custom.h>

//#define DEBUG_SESSION 1

namespace gstreamer
{

// {{{ PlaybinSessionPrivate
class PlaybinSessionPrivate
{
public:
    bool                    haveStreamInfo;
    bool                    muted;
    gdouble                 volume;
    quint32                 position;
    quint32                 length;
    QtopiaMedia::State      state;
    QtopiaMedia::State      stateBeforeSuspend;
    bool                    suspended;
    QMediaHandle            id;
    QString                 domain;
    QUrl                    url;
    BusHelper*              busHelper;
    Engine*                 engine;
    GstElement*             playbin;
    GstBus*                 bus;
    SinkWidget*             sinkWidget;
    QStringList             interfaces;
    gint64                  jumpPosition;

    QMediaVideoControlServer*   videoControlServer;
};
// }}}

// {{{ PlaybinSession

/*!
    \class gstreamer::PlaybinSession
    \internal
*/

PlaybinSession::PlaybinSession
(
 Engine*        engine,
 QUuid const&   id,
 QUrl const&    url
):
    d(new PlaybinSessionPrivate)
{
    d->haveStreamInfo = false;
    d->muted = false;
    d->volume = 1.0;
    d->position = 0;
    d->length  = quint32(-1);
    d->state = QtopiaMedia::Stopped;
    d->stateBeforeSuspend = QtopiaMedia::Stopped;
    d->engine = engine;
    d->id = QMediaHandle(id);
    d->url = url;
    d->playbin = 0;
    d->bus = 0;
    d->sinkWidget = 0;
    d->videoControlServer = 0;
    d->jumpPosition = 0;
    d->suspended = false;

    d->interfaces << "Basic";

    readySession();
}

PlaybinSession::~PlaybinSession()
{
    if (d->playbin != 0) {
        stop();

        delete d->busHelper;
        delete d->sinkWidget;
        gst_object_unref(GST_OBJECT(d->bus));
        gst_object_unref(GST_OBJECT(d->playbin));
    }

    delete d;
}

bool PlaybinSession::isValid() const
{
    return true;
}

static const char *qtStateName(QtopiaMedia::State state)
{
    switch(state)
    {
        case QtopiaMedia::Playing: return "playing";
        case QtopiaMedia::Paused: return "paused";
        case QtopiaMedia::Stopped: return "stopped";
        case QtopiaMedia::Buffering: return "buffering";
        case QtopiaMedia::Error: return "error";
        default: return "???";
    }
}

// Set QtopiaMedia state
void PlaybinSession::setQtState(QtopiaMedia::State state)
{
    if(state == d->state)
        return;
    
#if DEBUG_SESSION
    qDebug() << "PlaybinSession::setQtState " << qtStateName(d->state) << "->" << qtStateName(state);
#endif
    
    d->state = state;
    emit playerStateChanged(state);
}

// Set gst playbin element state
bool PlaybinSession::setGstState(GstState state)
{
#if DEBUG_SESSION
    qDebug() << "PlaybinSession::setGstState " << gst_element_state_get_name(state);
#endif
    
    if(d->playbin == 0)
        return false;
    
    if(gst_element_set_state(d->playbin, state) != GST_STATE_CHANGE_FAILURE)
        return true;

    qWarning() << "setGstState failed, state=" << gst_element_state_get_name(state) << ", url=" << d->url.toString();
    emit playerStateChanged(QtopiaMedia::Error);
    return false;
}


void PlaybinSession::start()
{
#if DEBUG_SESSION
    qDebug() << "PlaybinSession::start()";
#endif
    setGstState(GST_STATE_PLAYING);
}

void PlaybinSession::pause()
{
#if DEBUG_SESSION
    qDebug() << "PlaybinSession::pause()";
#endif
    setGstState(GST_STATE_PAUSED);
}

void PlaybinSession::stop()
{
#if DEBUG_SESSION
    qDebug() << "PlaybinSession::stop()";
#endif
    if(setGstState(GST_STATE_NULL))
        setQtState(QtopiaMedia::Stopped);
}

void PlaybinSession::suspend()
{
#if DEBUG_SESSION
    qDebug() << "PlaybinSession::suspend()";
#endif
    if ( !d->suspended ) {
        d->suspended = true;
        if (d->playbin != 0) {
            d->stateBeforeSuspend = d->state;
            GstFormat   format = GST_FORMAT_TIME;
            gst_element_query_position(d->playbin, &format, &d->jumpPosition);
        }

        stop();
    }
}

void PlaybinSession::resume()
{
#if DEBUG_SESSION
    qDebug() << "PlaybinSession::resume()";
#endif
    if ( d->suspended ) {
        d->suspended = false;
        if ( d->stateBeforeSuspend == QtopiaMedia::Playing )
            start();
        else
            pause();
    }
}

bool PlaybinSession::isSuspended() const
{
    return d->suspended;
}

void PlaybinSession::seek(quint32 ms)
{
    if (d->playbin != 0)
    {
        gint64  position = (gint64)ms * 1000000;

        gst_element_seek_simple(d->playbin, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH, position);
    }
}

quint32 PlaybinSession::length()
{
    return d->length;
}

#ifndef GST_MAX_VOLUME
#define GST_MAX_VOLUME 1
#endif

void PlaybinSession::setVolume(int volume)
{
    if (d->playbin != 0)
    {
        d->volume = gdouble(volume) / (100/GST_MAX_VOLUME);

        if (!d->muted)
        {
            g_object_set(G_OBJECT(d->playbin), "volume", d->volume, NULL);

            emit volumeChanged(volume);
        }
    }
}

int PlaybinSession::volume() const
{
    return int(d->volume * (10/GST_MAX_VOLUME));
}

void PlaybinSession::setMuted(bool mute)
{
    if (d->playbin != 0)
    {
        d->muted = mute;

        g_object_set(G_OBJECT(d->playbin), "volume", mute ? 0 : d->volume, NULL);

        emit volumeMuted(mute);
    }
}

bool PlaybinSession::isMuted() const
{
    return d->muted;
}

QtopiaMedia::State PlaybinSession::playerState() const
{
    return d->state;
}

QString PlaybinSession::errorString()
{
    return QString();
}

void PlaybinSession::setDomain(QString const& domain)
{
    d->domain = domain;
}

QString PlaybinSession::domain() const
{
    return d->domain;
}

QStringList PlaybinSession::interfaces()
{
    return d->interfaces;
}

QString PlaybinSession::id() const
{
    return d->id.toString();
}

QString PlaybinSession::reportData() const
{
    QStringList sl;

    sl << "engine:GStreamer" << ("uri:" + d->url.toString());

    return sl.join(",");
}

void PlaybinSession::busMessage(Message const& msg)
{
    GstMessage* gm = msg.rawMessage();

    if (gm == 0)
    {   // Null message, query current position
        GstFormat   format = GST_FORMAT_TIME;
        gint64      position = 0;

        if (gst_element_query_position(d->playbin, &format, &position) == TRUE)
        {
            quint32 pos = (position / 1000000) / 1000 * 1000;

            if (pos != d->position)
            {
                d->position = pos;
                emit positionChanged(d->position);
            }
        }
    }
    else if (GST_MESSAGE_SRC(gm) == GST_OBJECT_CAST(d->playbin))
    {
        switch (GST_MESSAGE_TYPE(gm))
        {
        case GST_MESSAGE_DURATION:
            break;

        case GST_MESSAGE_STATE_CHANGED:
            {
                GstState    oldState;
                GstState    newState;
                GstState    pending;

                gst_message_parse_state_changed(gm, &oldState, &newState, &pending);

                switch (newState)
                {
                    case GST_STATE_VOID_PENDING:
                    case GST_STATE_NULL:
                    case GST_STATE_READY:
                        break;
                    case GST_STATE_PAUSED:
                        setQtState(QtopiaMedia::Paused);

                        if ( d->jumpPosition ) {
                            gst_element_seek_simple(d->playbin, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH, d->jumpPosition );
                            d->jumpPosition = 0;
                        }
                        break;
                    case GST_STATE_PLAYING:
                        if (oldState == GST_STATE_PAUSED)
                            getStreamsInfo();

                        setQtState(QtopiaMedia::Playing);
                        break;
                }
            }
            break;

        case GST_MESSAGE_EOS:
            setQtState(QtopiaMedia::Stopped);
            break;

        case GST_MESSAGE_STREAM_STATUS:
        case GST_MESSAGE_UNKNOWN:
        case GST_MESSAGE_ERROR:
        case GST_MESSAGE_WARNING:
        case GST_MESSAGE_INFO:
        case GST_MESSAGE_TAG:
        case GST_MESSAGE_BUFFERING:
        case GST_MESSAGE_STATE_DIRTY:
        case GST_MESSAGE_STEP_DONE:
        case GST_MESSAGE_CLOCK_PROVIDE:
        case GST_MESSAGE_CLOCK_LOST:
        case GST_MESSAGE_NEW_CLOCK:
        case GST_MESSAGE_STRUCTURE_CHANGE:
        case GST_MESSAGE_APPLICATION:
        case GST_MESSAGE_ELEMENT:
        case GST_MESSAGE_SEGMENT_START:
        case GST_MESSAGE_SEGMENT_DONE:
        case GST_MESSAGE_LATENCY:
#if (GST_VERSION_MAJOR >= 0) &&  (GST_VERSION_MINOR >= 10) && (GST_VERSION_MICRO >= 13)
        case GST_MESSAGE_ASYNC_START:
        case GST_MESSAGE_ASYNC_DONE:
#endif
        case GST_MESSAGE_ANY:
            break;
        }
    }
}


void PlaybinSession::getStreamsInfo()
{
    // Get Length if have not before.
    if (d->length == quint32(-1))
    {
        GstFormat   format = GST_FORMAT_TIME;
        gint64      duration = 0;

        if (gst_element_query_duration(d->playbin, &format, &duration) == TRUE)
        {
            d->length = duration / 1000000;
            emit lengthChanged(d->length);
        }
    }

    // Audio/Video
    if (!d->haveStreamInfo)
    {
        enum {
            GST_STREAM_TYPE_UNKNOWN,
            GST_STREAM_TYPE_AUDIO,
            GST_STREAM_TYPE_VIDEO,
            GST_STREAM_TYPE_TEXT,
            GST_STREAM_TYPE_SUBPICTURE,
            GST_STREAM_TYPE_ELEMENT
        };

        GList*      streamInfo;

        g_object_get(G_OBJECT(d->playbin), "stream-info", &streamInfo, NULL);

        for (; streamInfo != 0; streamInfo = g_list_next(streamInfo))
        {
            gint        type;
            GObject*    obj = G_OBJECT(streamInfo->data);

            g_object_get(obj, "type", &type, NULL);

            switch (type)
            {
            case GST_STREAM_TYPE_AUDIO:
                break;

            case GST_STREAM_TYPE_VIDEO:
                {
                    d->videoControlServer = new QMediaVideoControlServer( d->id,
                                                                          0, //target
                                                                          this );

                    d->videoControlServer->setRenderTarget(d->sinkWidget->windowId());
                    d->interfaces << "Video";

                    QVideoSurface *surface = d->sinkWidget->videoSurface();
                    surface->setRotation(d->videoControlServer->videoRotation());
                    surface->setScaleMode(d->videoControlServer->videoScaleMode());
                    connect( d->videoControlServer, SIGNAL(rotationChanged(QtopiaVideo::VideoRotation)),
                             surface, SLOT(setRotation(QtopiaVideo::VideoRotation)) );
                    connect( d->videoControlServer, SIGNAL(scaleModeChanged(QtopiaVideo::VideoScaleMode)),
                             surface, SLOT(setScaleMode(QtopiaVideo::VideoScaleMode)) );

                    emit interfaceAvailable("Video");
                }
                break;

            case GST_STREAM_TYPE_UNKNOWN:
                break;

            case GST_STREAM_TYPE_TEXT:
                break;

            case GST_STREAM_TYPE_SUBPICTURE:
                break;

            case GST_STREAM_TYPE_ELEMENT:
                break;
            }
        }

        d->haveStreamInfo = true;
    }
}

void PlaybinSession::readySession()
{
    d->playbin = gst_element_factory_make("playbin", NULL);
    if (d->playbin != 0) {
        // Pre-set video element, even if no video
        d->sinkWidget = new VideoWidget;
        connect( d->sinkWidget->videoSurface(), SIGNAL(formatsChanged()), this, SLOT(updateSinkFormat()) );
        connect( d->sinkWidget->videoSurface(), SIGNAL(updateRequested()), this, SLOT(repaintLastFrame()) );
        g_object_set(G_OBJECT(d->playbin), "video-sink", d->sinkWidget->element(), NULL);

        // Sort out messages
        d->bus = gst_element_get_bus(d->playbin);
        d->busHelper = new BusHelper(d->bus, this);
        connect(d->busHelper, SIGNAL(message(Message)), SLOT(busMessage(Message)));

        // Initial volume
        g_object_get(G_OBJECT(d->playbin), "volume", &d->volume, NULL);

        // URI for media
        g_object_set(G_OBJECT(d->playbin), "uri", d->url.toString().toLocal8Bit().constData(), NULL);
    }
}


void PlaybinSession::updateSinkFormat()
{
    GstFormat   format = GST_FORMAT_TIME;
    gst_element_query_position(d->playbin, &format, &d->jumpPosition);

    stop();
    start();
}


void PlaybinSession::repaintLastFrame()
{
    if ( d->state != QtopiaMedia::Playing )
        d->sinkWidget->repaintLastFrame();
}

// }}}

}   // ns gstreamer

