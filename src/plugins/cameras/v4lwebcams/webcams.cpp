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

#include "webcams.h"
#include <qtopianamespace.h>

#ifdef QTOPIA_HAVE_V4L2

bool V4L2Webcam::open()
{
    static bool isCamera = true;
    if(!isCamera) return false;
    if (isOpen) return true;
    if ((fd = ::open("/dev/video", O_RDWR)) == -1) {
        return isOpen = false;
    }

    if(!doneSetup) {

        v4l2_input input;
        memset(&input, 0, sizeof(input));
        for (; ioctl(fd, VIDIOC_ENUMINPUT, &input) >= 0; ++input.index) {
            if(input.type == V4L2_INPUT_TYPE_CAMERA || input.type == 0) {
                   // TODO: api docs say this ioctl to take int*, does not work, takes int.
                if(ioctl(fd, VIDIOC_S_INPUT, input.index) != 0)
                    isCamera = true;
                else {
                    qDebug()<<"/dev/video is not of input type: camera";
                    isCamera = false;
                }
                break;
            }
       }
        if(isCamera ) {
            enumerateFormats();
            if (previewFormatMap.isEmpty())
                isCamera = false;
        }
        doneSetup = true;
    }

    fpolls.fd = fd;
    fpolls.events = POLLIN;
    fpolls.revents = 0;
    return (isOpen = isCamera);
}



void V4L2Webcam::close()
{
    if(isOpen)
        ::close(fd);
    isOpen = false;
}


///////// PREVIEW /////////////
void Preview::startCapture()
{
    memset(&wc->requestbuffers, 0, sizeof(wc->requestbuffers));
    wc->requestbuffers.count = 1;
    wc->requestbuffers.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    wc->requestbuffers.memory = V4L2_MEMORY_MMAP;

    if (ioctl(wc->fd, VIDIOC_REQBUFS, &wc->requestbuffers) == 0) {
        memset(&wc->buffer, 0, sizeof(wc->buffer));

        wc->buffer.index = 0;
        wc->buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        wc->buffer.memory = V4L2_MEMORY_MMAP;

        if (ioctl(wc->fd, VIDIOC_QUERYBUF, &wc->buffer) == 0) {

             wc->imageBuffer = (unsigned char*) mmap(NULL,
                                 wc->buffer.length,
                                 PROT_READ | PROT_WRITE,
                                 MAP_SHARED,
                                 wc->fd,
                                 wc->buffer.m.offset);
             if (wc->imageBuffer != (unsigned char*)MAP_FAILED) {
                 wc->buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
             }
             else {
                 qDebug()<<"CAMERA MMAP FAIL!!!!!!!!!!!!!";
             }

            preview_buffer_data = wc->imageBuffer;
            preview_buffer_bytesused = wc->buffer.length;
            preview_buffer_width = current_resolution.width();
            preview_buffer_height = current_resolution.height();


             int ret=ioctl(wc->fd, VIDIOC_QBUF, &wc->buffer);
             // queue up next buffer
             if (ret != 0) {
                 if(errno == EAGAIN) {
                     qDebug()<<"nonblocking io slected O_NONBLOCK and no buffer was  in outgoing queue";
                 }  else if(errno == EINVAL) {
                     qDebug()<<"buffer type not supported or index OOB or no buffers have been alloced";
                 }else if(errno == ENOMEM) {
                     qDebug()<<"insufficient memory";
                 }else if(errno == EIO) {
                     qDebug()<<"internal error";
                 }
             }

             if( ioctl(wc->fd, VIDIOC_STREAMON, &wc->buf_type) == 0) {
                 wc->imageBufferLength = wc->buffer.length;
             }
             else {
                 qDebug()<<"Cannot start Streaming!!!!!!!!!";
             }

        }else {
            if(errno == EINVAL)
                qDebug()<<"EINVAL";
        }
     } else {
         if(errno == EBUSY) {
             qDebug()<<"busy";
         }
         if( errno == EINVAL) {
             qDebug()<<"einval";
         }
          qDebug()<<"REQBUFS failed: ";
     }
     notifier->setEnabled(true);
}

void Preview::stopCapture()
{
    notifier->setEnabled(false);
    if (ioctl(wc->fd, VIDIOC_STREAMOFF, &wc->buf_type) == -1) {
    }

    if (munmap(wc->imageBuffer, wc->imageBufferLength) == -1) {
    }

}

void Preview::setupCapture(unsigned int fourcc, QSize resolution)
{
    memset(&wc->format, 0, sizeof(wc->format));

    wc->format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    wc->format.fmt.pix.width = resolution.width();
    wc->format.fmt.pix.height = resolution.height();
    wc->format.fmt.pix.pixelformat = fourcc;
    wc->format.fmt.pix.field = V4L2_FIELD_NONE;

    if (ioctl(wc->fd, VIDIOC_S_FMT, &wc->format) == -1) {
        if(errno == EBUSY)
            qDebug()<<"setupCapture : busy";
        if(errno ==  EINVAL)
            qDebug()<<"type field invalid / buffer type not suported ";
    }
}

void Preview::captureFrame()
{
    if (poll(&wc->fpolls, 1, 50) > 0) {
        // dequeue buffer
        if (ioctl(wc->fd, VIDIOC_DQBUF, &wc->buffer) == 0) {

            QVideoFrame vframe( pixelformat, QSize(preview_buffer_width, preview_buffer_height),
                        reinterpret_cast<uchar*>(preview_buffer_data));
            emit frameReady(vframe);

            int ret = ioctl(wc->fd, VIDIOC_QBUF, &wc->buffer);

            // queue up next buffer
            if(ret == -1) {
                if(errno == EAGAIN) {
                    qDebug()<<"nonblocking io slected O_NONBLOCK and no buffer was  in outgoing queue";
                }  else if(errno == EINVAL) {
                    qDebug()<<"buffer type not supported or index OOB or no buffers have been alloced";
                }else if(errno == ENOMEM) {
                    qDebug()<<"insufficient memory";
                }else if(errno == EIO) {
                    qDebug()<<"internal error";
                }
            }
        }
    }
}

Preview::Preview(V4L2Webcam* w) : wc(w)
{
    preview_active = false;
    current_format = 0;
    current_resolution = QSize();
    if (wc->previewFormatMap.isEmpty()) {
        qWarning()<<"V4L2Webcam: No preview formats were detected!";
        return;
    }

    current_format = wc->previewFormatMap.keys().first();

    QList<QSize> preferred;
    preferred <<QSize(640,480) << QSize(320,240);
    bool found = false;

    for (int  i = 0; i < 2; i++) {
        if (wc->previewFormatMap.value(current_format).contains(preferred[i]))
            current_resolution = preferred[i];
            found = true;
            break;
    }
    if (!found) {
        int size = wc->previewFormatMap.value(current_format).size();
        current_resolution = wc->previewFormatMap.value(current_format)[size-1];
    }
}

void Preview::start(unsigned int format, QSize resolution, int framerate)
{
    if (wc->previewFormatMap.isEmpty() || !wc->previewFormatMap.keys().contains(format)) return;
    Q_UNUSED(framerate);
    if (preview_active) return;

    pixelformat = qFourccToVideoFormat(format);
    qDebug()<<"pixelformat: "<<pixelformat;

    if (pixelformat == QVideoFrame::Format_Invalid) {
        qWarning()<<"v4lwebcam: pixel format not understood by video library";
        return;
    }

    wc->open();
    setupCapture(current_format = format, current_resolution = resolution);


    notifier = new QSocketNotifier(wc->fd, QSocketNotifier::Read , this);
    notifier->setEnabled(false);
    connect(notifier, SIGNAL(activated(int)), this, SLOT(captureFrame()));

    startCapture();
    preview_active = true;
}

void Preview::stop()
{
    if (!preview_active) return;
    stopCapture();

    disconnect(notifier);
    delete notifier;
    notifier = 0;

    wc->close();
    preview_active = false;
}

QList<unsigned int> Preview::framerates()
{
    return QList<unsigned int> () << 30;
}

unsigned int Preview::framerate()
{
    return 30;
}



QtopiaCamera::FormatResolutionMap Preview::formats() const
{
    return wc->previewFormatMap;
}

unsigned int Preview::format()
{
    return current_format;
}

QSize Preview::resolution()
{
    return current_resolution;
}

bool Preview::hasZoom() const
{
    return false;
}

QPair<unsigned int,unsigned int> Preview::zoomRange()
{
    return QPair<unsigned int,unsigned int>(0,0);
}

void Preview::zoomIn()
{
}

void Preview::zoomOut()
{
}

QList<QCameraControl*> Preview::controls() const
{
    return QList<QCameraControl*>();
}

void Preview::setValue(quint32 id, int value)
{
    Q_UNUSED(id)
    Q_UNUSED(value)
}






/////////// STILLL/////////////
Still::Still(V4L2Webcam *w):
    wc(w)
{
    current_format = 0;
    current_resolution = QSize();
    if (wc->previewFormatMap.isEmpty()) return;

    current_format = wc->previewFormatMap.keys().first();

    QList<QSize> preferred;
    preferred <<QSize(640,480) << QSize(1024,768);
    bool found = false;

    for (int  i = 0; i < 2; i++) {
        if (wc->previewFormatMap.value(current_format).contains(preferred[i]))
            current_resolution = preferred[i];
            found = true;
            break;
    }
    if (!found) {
        int size = wc->previewFormatMap.value(current_format).size();
        current_resolution = wc->previewFormatMap.value(current_format)[size-1];
    }
}

QtopiaCamera::FormatResolutionMap Still::formats() const
{
    return wc->previewFormatMap;
}

unsigned int Still::format()
{
    return current_format;
}

QSize Still::resolution()
{
    return current_resolution;
}

void Still::takeStillImage(unsigned int format, QSize resolution, int count , unsigned int msecs )
{
    if (count < 1 || !wc->stillFormatMap.keys().contains(format) ||
        !wc->stillFormatMap.value(format).contains(resolution)) return;

    Q_UNUSED(msecs);
    bool previewWasStarted = false;
    bool revert = false;
    QSize saved_res = wc->preview->resolution();
    unsigned int saved_format = wc->preview->format();

    if(!wc->preview->preview_active) {
        wc->preview->start(wc->preview->format(), resolution, wc->preview->framerate());
        if(!wc->preview->preview_active) return;
        previewWasStarted = true;
    } else if (resolution != wc->preview->resolution()) {
        wc->preview->stop();
        Qtopia::msleep(500);
        wc->preview->start(format, resolution, wc->preview->framerate());
        revert = true;
    }
    // give camera time to adjust
    Qtopia::msleep(2000);
    current_resolution = resolution;
    current_format = format;

    int bu = wc->preview->preview_buffer_bytesused;
    QByteArray ba(reinterpret_cast<const char*>(wc->preview->preview_buffer_data), bu);

    emit imageReady(ba, QSize(wc->preview->preview_buffer_width, wc->preview->preview_buffer_height), true);

    if(previewWasStarted) {
        wc->preview->stop();
    } else if (revert) {
        wc->preview->stop();
        wc->preview->start(saved_format, saved_res, wc->preview->framerate());
    }
}


QList<QCameraControl*> Still::controls() const
{
    return QList<QCameraControl*>();
}

void Still::setValue(quint32 id, int value)
{
    Q_UNUSED(id);
    Q_UNUSED(value);
}

#endif /* QTOPIA_HAVE_V4L2 */
#include <libv4l1-videodev.h>

V4L1Preview::V4L1Preview(V4L1Webcam* w) : wc(w)
{
    current_format = QtopiaCamera::RGB32;
    connect(this, SIGNAL(cameraError(QtopiaCamera::CameraError,QString)),
        wc, SIGNAL(cameraError(QtopiaCamera::CameraError,QString)));
}


void V4L1Preview::start(unsigned int format, QSize size, int framerate)
{
    Q_UNUSED(format);
    Q_UNUSED(size);
    Q_UNUSED(framerate);

    wc->frames = 0;
    wc->currentFrame = 0;

    if (!wc->open())
        emit cameraError(QtopiaCamera::FatalError, QString("cannot open device"));

    notifier = new QSocketNotifier(wc->fd, QSocketNotifier::Read , this);
    notifier->setEnabled(false);
    connect(notifier, SIGNAL(activated(int)), this, SLOT(captureFrame()));

    struct video_picture pict;
    memset( &pict, 0, sizeof( pict ) );
    ioctl( wc->fd, VIDIOCGPICT, &pict );
    pict.palette = VIDEO_PALETTE_RGB24;
    if ( ioctl( wc->fd, VIDIOCSPICT, &pict ) < 0 ) {
        qWarning( "%s: could not set the picture mode", V4L_VIDEO_DEVICE );
        emit cameraError(QtopiaCamera::FatalError, QString("VIDIOCSPICT"));
        wc->close( );
        return;
    }

    current_format = QtopiaCamera::RGB32;
    // Set the new capture window.
    struct video_window wind;
    memset( &wind, 0, sizeof( wind ) );

    wind.x = 0;
    wind.y = 0;
    wind.width = wc->current_resolution.width();
    wind.height = wc->current_resolution.height();
    if ( ioctl( wc->fd, VIDIOCSWIN, &wind ) < 0 ) {
        emit cameraError(QtopiaCamera::FatalError, QString("could not set the capture window"));
        wc->close();
        return;
    }

    // Enable mmap-based access to the camera.
    memset( &wc->mbuf, 0, sizeof( wc->mbuf ) );
    if ( ioctl( wc->fd, VIDIOCGMBUF, &wc->mbuf ) < 0 ) {
        emit cameraError(QtopiaCamera::FatalError, QString("mmap-based camera access is not available"));
        wc->close( );
        return;
    }

    // Mmap the designated memory region.
    wc->frames = (unsigned char *)mmap( 0, wc->mbuf.size, PROT_READ | PROT_WRITE,
                                    MAP_SHARED, wc->fd, 0 );
    if ( !wc->frames || wc->frames == (unsigned char *)(long)(-1) ) {
        emit cameraError(QtopiaCamera::FatalError, QString("could not mmap the device"));
        wc->close( );
        return;
    }
    preview_buffer_bytesused = wc->mbuf.size;
    startCapture();
    preview_active = true;
}

void V4L1Preview::captureFrame()
{
    // Start capturing the next frame (we alternate between 0 and 1).
    int frame = wc->currentFrame;
    struct video_mmap capture;
    if ( wc->mbuf.frames > 1 ) {
        wc->currentFrame = !wc->currentFrame;
        capture.frame = wc->currentFrame;
        capture.width = wc->width;
        capture.height = wc->height;
        capture.format = VIDEO_PALETTE_RGB32;
        ioctl( wc->fd, VIDIOCMCAPTURE, &capture );
    }

    // Wait for the current frame to complete.
    ioctl( wc->fd, VIDIOCSYNC, &frame );


    preview_buffer_width = wc->width;
    preview_buffer_height = wc->height;
    preview_buffer_data = wc->frames + wc->mbuf.offsets[frame];

    QVideoFrame vframe(QVideoFrame::Format_RGB32, QSize(wc->width, wc->height),
                        reinterpret_cast<uchar*>(preview_buffer_data));
    emit frameReady(vframe);

    // Queue up another frame if the device only supports one at a time.
    if ( wc->mbuf.frames <= 1 ) {
        capture.frame = wc->currentFrame;
        capture.width = wc->width;
        capture.height = wc->height;
        capture.format = VIDEO_PALETTE_RGB32;
        ioctl( wc->fd, VIDIOCMCAPTURE, &capture );
    }

}

void V4L1Preview::startCapture()
{
    // Start capturing of the first frame.
    struct video_mmap capture;
    wc->currentFrame = 0;
    capture.frame = wc->currentFrame;
    capture.width = wc->width;
    capture.height = wc->height;
    capture.format = VIDEO_PALETTE_RGB32;
    ioctl( wc->fd, VIDIOCMCAPTURE, &capture );
    notifier->setEnabled(true);
}

void V4L1Preview::stopCapture()
{
    notifier->setEnabled(false);
    if ( wc->frames != 0 ) {
        munmap( wc->frames, wc->mbuf.size );
        wc->frames = 0;
    }
    ioctl( wc->fd, VIDIOCSYNC, 0);
    int flag = 0;
    ioctl( wc->fd, VIDIOCCAPTURE, &flag );
}

void V4L1Preview::stop()
{
    stopCapture();
    wc->close();
    preview_active = false;
}

QList<unsigned int> V4L1Preview::framerates()
{
    return QList<unsigned int>()<<15;
}

unsigned int V4L1Preview::framerate()
{
    return 15;
}

QtopiaCamera::FormatResolutionMap V4L1Preview::formats() const
{
    QtopiaCamera::FormatResolutionMap f;
    f.insert(QtopiaCamera::RGB32, QList<QSize>() << wc->current_resolution);
    return f;
}

unsigned int V4L1Preview::format()
{
    return QtopiaCamera::RGB32;
}

QSize V4L1Preview::resolution()
{
    return wc->current_resolution;
}

bool V4L1Preview::hasZoom() const
{
    return false;
}

QPair<unsigned int,unsigned int> V4L1Preview::zoomRange()
{
    return QPair<unsigned int, unsigned int>(0,0);
}


void V4L1Preview::zoomIn()
{
}

void V4L1Preview::zoomOut()
{
}

QList<QCameraControl*> V4L1Preview::controls() const
{
    return QList<QCameraControl*>();
}

void V4L1Preview::setValue(quint32 id, int value)
{
    Q_UNUSED(id);
    Q_UNUSED(value);
}


// STILL

QtopiaCamera::FormatResolutionMap V4L1Still::formats() const
{
    QtopiaCamera::FormatResolutionMap f;
    f.insert(QtopiaCamera::RGB32, QList<QSize>() << wc->current_resolution);
    return f;

}

unsigned int V4L1Still::format()
{
    return QtopiaCamera::RGB32;
}

QSize V4L1Still::resolution()
{
    return wc->current_resolution;
}

void V4L1Still::takeStillImage(unsigned int format, QSize resolution, int count, unsigned int msecs)
{
    Q_UNUSED(count);
    Q_UNUSED(msecs);

    if(format != QtopiaCamera::RGB32 && resolution != wc->current_resolution) return;

    bool wasActivated = false;

    if(!wc->preview->preview_active) {
        wc->preview->start(QtopiaCamera::RGB32, wc->current_resolution, 15);
        if(!wc->preview->preview_active) return;
        wasActivated = true;
    }

    QByteArray ba(reinterpret_cast<char*>(wc->preview->preview_buffer_data), wc->preview->preview_buffer_bytesused);
    emit imageReady(ba, QSize(wc->preview->preview_buffer_width, wc->preview->preview_buffer_height), true);


    if(wasActivated)
        wc->preview->stop();
}

QList<QCameraControl*> V4L1Still::controls() const
{
    return QList<QCameraControl*>();
}

void V4L1Still::setValue(quint32 id, int value)
{
    Q_UNUSED(id);
    Q_UNUSED(value);
}

