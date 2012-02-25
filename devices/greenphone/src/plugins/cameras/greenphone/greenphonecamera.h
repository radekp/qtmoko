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

#ifndef OMEGACAMERA_H
#define OMEGACAMERA_H


#include <QtCore>
#include <QObject>
#include <QCameraDevice>
#include <QCameraControl>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <libv4l1-videodev.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>

#include <sys/stat.h>
#include <sys/poll.h>
#include <custom.h>

#include <QSocketNotifier>
#include <QVideoFrame>

#define WCAM_VIDIOCSINFOR 215
#define WCAM_VIDIOCSFRATE 224
#define WCAM_VIDIOCSB 219
#define WCAM_VIDIOCSCS 223
#define WCAM_VIDIOCSCONT 227
#define WCAM_VIDIOCSAWB 220
#define WCAM_VIDIOCSZOOM  225
#define WCAM_VIDIOCSN     221
#define PREVIEW             13
#define CAPTURE             13
#define VIDEO_PREVIEW       25
#define VIDEO_RECORD        15

#include <QDebug>

class Greenphone;
class GPreview : public QCameraPreviewCapture
{
    Q_OBJECT
public:

    GPreview(Greenphone* g) : gp(g)
    {
        notifier = 0;
        preview_active = false;
        sequence = 0;

        current_resolution = QSize(320,240);
    }
    virtual ~GPreview() {}

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

    Greenphone *gp;

    QSocketNotifier *notifier;

    unsigned char *frames;
    unsigned char* imageBuf;
    struct video_mbuf mbuf;

    struct QRawDataBuffer
    {
        void* data;
        int width, height;
        int bytesused;
        int sequence;

    };
    QRawDataBuffer preview_buffer;
    QRawDataBuffer still_buffer;

    void manageBuffers(bool);
    void convertFrame();
    unsigned int sequence;
    unsigned int currentFrame;
    QSize current_resolution;
    void lock();
    void unlock();
    bool preview_active ;
    struct pollfd ufds;
    public slots:
    void captureFrame();
    void forceFrameCapture();

};

class GStill : public QCameraStillCapture
{
    Q_OBJECT
public:
    GStill(Greenphone *g)  : gp(g)
    {
       current_resolution = QSize(640, 480);
    }
    virtual ~GStill() {}

    QtopiaCamera::FormatResolutionMap formats() const ;
    unsigned int format();

    QSize resolution();

    void takeStillImage(unsigned int format, QSize resolution, int count = 1, unsigned int msecs = 0);

    QList<QCameraControl*> controls() const;
    void setValue(quint32 id, int value);

    Greenphone *gp;
    unsigned int current_format;
    QSize current_resolution;
    bool revert;
    bool wasStarted;
public slots:
    void takeShot(int, int msec );
    void takeShotsDone();
};


class Greenphone : public QCameraDevice
{
    Q_OBJECT
public:

    Greenphone()
    {
        preview = new GPreview(this);
        still = new GStill(this);
        isOpen = false;
    }

    virtual ~Greenphone()
    {
        delete preview;
        delete still;

        preview = 0;
        still = 0;
    }

    bool open();
    void close();

    unsigned int captureModes() const { return QCameraDevice::StillMode; }
    QCameraPreviewCapture* previewCapture() const { return preview; }
    QCameraStillCapture* stillCapture() const { return still; }
    QCameraVideoCapture* videoCapture() const { return 0;}

    Orientation orientation() const  { return QCameraDevice::BackFacing; }
    QString description() const { return "Greenphone camera plugin"; }
    QString name() const { return "greenphone"; }

    GPreview* preview;
    GStill *still;
    int fd;
    bool isOpen;
    friend class GPreview;
    friend class GStill;
};

#endif
