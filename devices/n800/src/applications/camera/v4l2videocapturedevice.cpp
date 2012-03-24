/****************************************************************************
**
** Copyright (C) 2000-2008 TROLLTECH ASA. All rights reserved.
**
** This file is part of the Opensource Edition of the Qtopia Toolkit.
**
** This software is licensed under the terms of the GNU General Public
** License (GPL) version 2.
**
** See http://www.trolltech.com/gpl/ for GPL licensing information.
**
** Contact info@trolltech.com if any conditions of this licensing are
** not clear to you.
**
**
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
****************************************************************************/

#include <qset.h>
#include <QMapIterator>

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

#ifdef HAVE_V4L2

#include "v4l2videocapturedevice.h"
#include "formatconverter.h"

#define VIDEO_DEVICE            "/dev/video0"



inline
bool operator<(QSize const& lhs, QSize const& rhs)
{
    return (lhs.width() + lhs.height()) > (rhs.width() + rhs.height()); // yes, inverse
}



namespace camera
{


V4L2VideoCaptureDevice::V4L2VideoCaptureDevice(int fd):
    m_fd(fd),
    m_imageBuffer(NULL),
    m_converter(NULL)
{
    setupCamera();
}

V4L2VideoCaptureDevice::~V4L2VideoCaptureDevice()
{
    shutdown();
}

bool V4L2VideoCaptureDevice::hasCamera() const
{
    return m_fd != -1;
}

void V4L2VideoCaptureDevice::getCameraImage(QImage& img, bool copy)
{
    Q_UNUSED(copy);

    // sync on queued
    pollfd          fpolls;
    v4l2_buffer     buffer;

    fpolls.fd = m_fd;
    fpolls.events = POLLIN;
    fpolls.revents = 0;

    if (poll(&fpolls, 1, -1) > 0)
    {
        // queue next
        memset(&buffer, 0, sizeof(buffer));

        buffer.index = 0;
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory = V4L2_MEMORY_MMAP;

        if (ioctl(m_fd, VIDIOC_DQBUF, &buffer) == 0)
        {
            // give back current image
            img = QImage(m_converter->convert(m_imageBuffer),
                            m_size.width(),
                            m_size.height(),
                            QImage::Format_RGB32);

            ioctl(m_fd, VIDIOC_QBUF, &buffer);  // if fails?
        }
    }
}

QList<QSize> V4L2VideoCaptureDevice::photoSizes() const
{
    return m_imageTypes.keys();
}

QList<QSize> V4L2VideoCaptureDevice::videoSizes() const
{
    return QList<QSize>();
}

QSize V4L2VideoCaptureDevice::recommendedPhotoSize() const
{
    return m_imageTypes.keys().front();
}

QSize V4L2VideoCaptureDevice::recommendedVideoSize() const
{
    return QSize(0, 0);
}

QSize V4L2VideoCaptureDevice::recommendedPreviewSize() const
{
    return m_imageTypes.keys().back();
}

void V4L2VideoCaptureDevice::setCaptureSize(QSize size)
{
    endCapture();

    m_size = size;

    beginCapture();
}

QSize V4L2VideoCaptureDevice::captureSize() const
{
    return m_size;
}

uint V4L2VideoCaptureDevice::refocusDelay() const
{
    return 250; // TODO: copied from v4l1
}

int V4L2VideoCaptureDevice::minimumFramePeriod() const
{
    return 40;  // TODO: copied from v4l1
}


void V4L2VideoCaptureDevice::setupCamera()
{
    bool                success = false;
    v4l2_capability     capability;

    // Open the video device.
//    if ((m_fd = open(VIDEO_DEVICE, O_RDWR)) != -1)        TODO: see factory
    if (m_fd != -1)
    {
        memset(&capability, 0, sizeof(capability));

        if (ioctl(m_fd, VIDIOC_QUERYCAP, &capability) >= 0)
        {
            if ((capability.capabilities & V4L2_CAP_VIDEO_CAPTURE) != 0 &&
                (capability.capabilities & V4L2_CAP_STREAMING) != 0)
            {
                v4l2_input  input;

                memset(&input, 0, sizeof(input));

                // check for camera
                for (; ioctl(m_fd, VIDIOC_ENUMINPUT, &input) >= 0; ++input.index)
                {
                    if (input.type == V4L2_INPUT_TYPE_CAMERA ||
                        input.type == 0 /* buggy driver returns unknown type */) {

                        // select as input first camera type device
                        // TODO: api docs say this ioctl to take int*, does not work, takes int.
                        if (ioctl(m_fd, VIDIOC_S_INPUT, input.index) != 0)
                        {
                            calcPhotoSizes();

                            m_size = m_imageTypes.keys().front();

                            beginCapture();

                            success = true;
                            break;
                        }
                        else
                            qWarning("%s: Failed select input", VIDEO_DEVICE);
                    }
                }
            }
            else {
                qWarning("%s is not a suitable capture device", VIDEO_DEVICE);
            }
        }
        else {
            qWarning("%s: could not retrieve the video capabilities", VIDEO_DEVICE);
        }
    }
    else {
        qWarning("Unable to open %s: %s", VIDEO_DEVICE, strerror(errno));
    }

    if (!success && m_fd != -1) {
        close(m_fd);
        m_fd = -1;
    }
}

void V4L2VideoCaptureDevice::shutdown()
{
    if (m_fd != -1)
    {
        if (munmap(m_imageBuffer, m_imageBufferLength) != -1)
        {
            ioctl(m_fd, VIDIOC_STREAMOFF);

            close(m_fd);
        }
    }
}

void V4L2VideoCaptureDevice::beginCapture()
{
    v4l2_format     format;

    // adjust camera
    memset(&format, 0, sizeof(format));

    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width = m_size.width();
    format.fmt.pix.height = m_size.height();
    format.fmt.pix.pixelformat = m_imageTypes[m_size];
    format.fmt.pix.field = V4L2_FIELD_NONE;

    if (ioctl(m_fd, VIDIOC_S_FMT, &format) == 0)
    {
        // create a converter for this format type
        m_converter = FormatConverter::createFormatConverter(m_imageTypes[m_size], m_size.width(), m_size.height());

        v4l2_requestbuffers requestBuffers;

        memset(&requestBuffers, 0, sizeof(requestBuffers));

        requestBuffers.count = 1;
        requestBuffers.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        requestBuffers.memory = V4L2_MEMORY_MMAP;

        if (ioctl(m_fd, VIDIOC_REQBUFS, &requestBuffers) == 0)
        {
            v4l2_buffer     buffer;

            memset(&buffer, 0, sizeof(buffer));

            buffer.index = 0;
            buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buffer.memory = V4L2_MEMORY_MMAP;

            if (ioctl(m_fd, VIDIOC_QUERYBUF, &buffer) == 0)
            {

                m_imageBuffer  = (unsigned char*)mmap(NULL,
                                                      buffer.length,
                                                      PROT_READ | PROT_WRITE,
                                                      MAP_SHARED,
                                                      m_fd,
                                                      buffer.m.offset);

                if (m_imageBuffer != (unsigned char*) MAP_FAILED)
                {
                    v4l2_buf_type   type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

                    if (ioctl(m_fd, VIDIOC_QBUF, &buffer) == 0 &&
                        ioctl(m_fd, VIDIOC_STREAMON, &type) == 0)
                    {
                        m_imageBufferLength = buffer.length;
                    }
                }
            }
        }
    }
}

void V4L2VideoCaptureDevice::endCapture()
{
    FormatConverter::releaseFormatConverter(m_converter);
}

void V4L2VideoCaptureDevice::calcPhotoSizes()
{
    QList<QSize>    resolutions;

    // standard resolutions - XGA, SVGA, VGA, CGA (roughly) and postage stamp
    resolutions << QSize(1024, 768) << QSize(800, 600) << QSize(640, 480) << QSize(320, 200) << QSize(120, 120);
//    resolutions << QSize(120, 120);

    // test if the resolutions are supported (write back modifications)
    QMutableListIterator<QSize> sizeIt(resolutions);

    while (sizeIt.hasNext())
    {
        v4l2_format     format;
        QSize&          currentSize = sizeIt.next();

        memset(&format, 0, sizeof(format));

        format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        format.fmt.pix.width = currentSize.width();
        format.fmt.pix.height = currentSize.height();
        format.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB32;
        format.fmt.pix.field = V4L2_FIELD_NONE;

        if (ioctl(m_fd, VIDIOC_TRY_FMT, &format) == 0)
        {
            qWarning("V4L2: Tried resolution & format (%d, %d, RGB4) - device wants (%d, %d, %4s)",
                    currentSize.width(),
                    currentSize.height(),
                    format.fmt.pix.width,
                    format.fmt.pix.height,
                    (char const*)&format.fmt.pix.pixelformat);

            unsigned int&   imageType = m_imageTypes[QSize(format.fmt.pix.width, format.fmt.pix.height)];

            if (imageType != V4L2_PIX_FMT_RGB32)    // have been here? keep RGB32 if we can
            {
                imageType = format.fmt.pix.pixelformat;
            }
        }
    }
}

} // ns camera

#endif // HAVE_V4L2
