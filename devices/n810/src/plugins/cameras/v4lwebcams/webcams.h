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

#ifndef WEBCAMS_H
#define WEBCAMS_H

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <libv4l1-videodev.h>
#include <custom.h>

#include <QtCore>
#include <QObject>
#include <QCameraDevice>
#include <QCameraControl>
#include <qcameratools.h>



class V4L2Webcam;
class Preview : public QCameraPreviewCapture
{
    Q_OBJECT
public:

    Preview(V4L2Webcam* w);

    virtual ~Preview() {}

    void start(unsigned int format, QSize resolution, int framerate);
    void stop() ;

    QList<unsigned int> framerates();
    unsigned int framerate() ;

    QtopiaCamera::FormatResolutionMap formats() const;
    unsigned int format();

    QSize resolution();

    bool hasZoom() const ;

    QPair<unsigned int,unsigned int> zoomRange();

    void zoomIn();
    void zoomOut();

    QList<QCameraControl*> controls() const;
    void setValue(quint32 id, int value) ;

    V4L2Webcam *wc;

    QSocketNotifier *notifier;

    unsigned int current_format;
    QSize current_resolution;

    void *preview_buffer_data;
    int preview_buffer_width;
    int preview_buffer_height;
    int preview_buffer_bytesused;
    QVideoFrame::PixelFormat pixelformat;


    void manageBuffers(bool);
    void startCapture();
    void stopCapture();
    void setupCapture(unsigned int, QSize);

    bool preview_active ;
public slots:
    void captureFrame();

};

class Still : public QCameraStillCapture
{
    Q_OBJECT
public:
    Still(V4L2Webcam *w);
    virtual ~Still() {}

    QtopiaCamera::FormatResolutionMap formats() const ;
    unsigned int format();

    QSize resolution();

    void takeStillImage(unsigned int format, QSize resolution, int count = 1, unsigned int msecs = 0);

    QList<QCameraControl*> controls() const;
    void setValue(quint32 id, int value);

    V4L2Webcam *wc;

    unsigned int current_format;
    QSize current_resolution;

};


class  V4L2Webcam : public QCameraDevice
{
    Q_OBJECT
public:

    V4L2Webcam()
    {
        isOpen = false;
        doneSetup = false;

        open();
        close();

        preview = new Preview(this);
        still = new Still(this);
    }

    virtual ~V4L2Webcam()
    {
        delete preview;
        delete still;
    }

    bool open();
    void close();

    unsigned int captureModes() const { return QCameraDevice::StillMode; }
    QCameraPreviewCapture* previewCapture() const {return preview;}
    QCameraStillCapture* stillCapture() const { return still;}
    QCameraVideoCapture* videoCapture() const { return 0;}


    QCameraDevice::Orientation orientation() const  { return  QCameraDevice::Changing;  }
    QString description() const  { return "Generic V4L2 webcam plugin"; }
    QString name() const  { return "V4L2Webcam"; }

    Preview* preview;
    Still* still;


    bool isOpen;
    int fd;
    QMap<QSize, unsigned int> imageTypes;
    QList<QCameraControl*> controls;
    int framerate;

    bool lock;
    pollfd  fpolls;

    QSize resolution;

    bool hasCapture;
    bool hasStreaming;
    bool isReady;

    v4l2_format format;
    v4l2_requestbuffers requestbuffers;
    v4l2_buffer buffer;
    v4l2_capability caps;
    v4l2_buf_type buf_type;
    v4l2_queryctrl queryctrl;
    v4l2_fmtdesc fmt;

    unsigned char* imageBuffer;
    int imageBufferLength;

    QList<unsigned int> supportedFormats;
    QtopiaCamera::FormatResolutionMap previewFormatMap;
    QtopiaCamera::FormatResolutionMap stillFormatMap;
    QtopiaCamera::FormatResolutionMap videoFormatMap;


    void enumerateFormats()
    {
        supportedFormats.clear();
        previewFormatMap.clear();
        stillFormatMap.clear();
        videoFormatMap.clear();

        memset(&fmt, 0, sizeof(v4l2_fmtdesc));

        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        int sanity = 0;

        for (fmt.index = 0;; fmt.index++) {
            if (sanity++ > 8)
                break;
            if(  ioctl(fd, VIDIOC_ENUM_FMT, &fmt) == -1) {
                if(errno == EINVAL)
                    break;
            }
            supportedFormats.append(fmt.pixelformat);
        }

        // Step through supported formats and set a list
        // of supported resolutions for that format
        QList<QSize> all_sizes;

        for(int i = 2;i < 9;++i)
            all_sizes << qResolutionToQSize(i);

        QList<QSize> still_sizes;
        QList<QSize> preview_sizes;
        QList<QSize> video_sizes;

        QList<unsigned int> still_formats;
        still_formats << QtopiaCamera::YUYV << QtopiaCamera::UYVY
                      << QtopiaCamera::JPEG << QtopiaCamera::RGB24
                      << QtopiaCamera::RGB565;

        QList<unsigned int> preview_formats;
        preview_formats << QtopiaCamera::YUYV << QtopiaCamera::UYVY <<QtopiaCamera::SBGGR8
                      << QtopiaCamera::RGB24 << QtopiaCamera::RGB555 <<QtopiaCamera::RGB565;

        QList<unsigned int> video_formats;
        video_formats << QtopiaCamera::MPEG << QtopiaCamera::MJPEG
                      << QtopiaCamera::H263;

        bool p,v,s;
        foreach(unsigned int supported_format, supportedFormats)
        {
            p = v = s = false;
            preview_sizes.clear();
            still_sizes.clear();
            video_sizes.clear();

            QList<QSize>::const_iterator size_iter = all_sizes.begin();

            for(; size_iter != all_sizes.end(); size_iter++) {
                v4l2_format format;

                memset(&format, 0, sizeof(v4l2_format));

                format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                format.fmt.pix.width  = (*size_iter).width();
                format.fmt.pix.height = (*size_iter).height();
                format.fmt.pix.pixelformat = supported_format;
                format.fmt.pix.field  = V4L2_FIELD_NONE;

                if (ioctl(fd, VIDIOC_TRY_FMT, &format) != -1) {

                    if (preview_formats.contains(format.fmt.pix.pixelformat)) {
                        preview_sizes << QSize(format.fmt.pix.width, format.fmt.pix.height);
                        p = true;
                    }
                    if (still_formats.contains(format.fmt.pix.pixelformat)) {
                        still_sizes << QSize(format.fmt.pix.width, format.fmt.pix.height);
                        s = true;
                    }
                    if (video_formats.contains(format.fmt.pix.pixelformat)) {
                        video_sizes << QSize(format.fmt.pix.width, format.fmt.pix.height);
                        v = true;
                    }
                }
            }

            if (p) previewFormatMap.insert(supported_format, preview_sizes);
            if (s) stillFormatMap.insert(supported_format, still_sizes);
            if (v) videoFormatMap.insert(supported_format, video_sizes);
        }

        qDebug()<<"preview formats: "<< previewFormatMap;
        qDebug()<<"video formats: "<< videoFormatMap;
        qDebug()<<"still formats: "<< stillFormatMap;
    }

    bool doneSetup;
};

#endif
