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

#include <QScreen>
#include <QDebug>

#include "gstreamersinkwidget.h"
#include "qvideosurface.h"

#include "gstreamerqtopiavideosink.h"

#include <gst/video/video.h>

namespace gstreamer
{

static GstVideoSinkClass*   parentClass;


// {{{ QtopiaVideoSink

/*!
    \class gstreamer::QtopiaVideoSink
    \internal
*/

GstCaps* QtopiaVideoSink::get_caps(GstBaseSink* sink)
{
    QtopiaVideoSink*    self = G_TYPE_CHECK_INSTANCE_CAST(sink, QtopiaVideoSinkClass::get_type(), QtopiaVideoSink);

    QStringList supportedBpp; // for RGB formats
    QStringList supportedFourcc; // for YUV formats
    QMap< QVideoFrame::PixelFormat, int > bppMap;
    QMap< QVideoFrame::PixelFormat, QString > fourccMap;

    bppMap[QVideoFrame::Format_ARGB32] = 32;
    bppMap[QVideoFrame::Format_RGB32] = 32;
    bppMap[QVideoFrame::Format_RGB24] = 24;
    bppMap[QVideoFrame::Format_RGB565] = 16;

    fourccMap[QVideoFrame::Format_YUV420P] = "I420";
    fourccMap[QVideoFrame::Format_UYVY] = "UYVY";
    fourccMap[QVideoFrame::Format_YV12] = "YV12";

    QVideoFormatList formats = self->widget->videoSurface()->preferredFormats();
    if ( formats.isEmpty() )
        formats = self->widget->videoSurface()->supportedFormats();

    if ( formats.isEmpty() )
        formats << QVideoFrame::Format_RGB32;


    foreach ( QVideoFrame::PixelFormat format, formats ) {
        if ( bppMap.contains(format) )
            supportedBpp << QString::number(bppMap[format]);

        if ( fourccMap.contains(format) )
            supportedFourcc << fourccMap[format];
    }

    QString capsString;

    if (!supportedFourcc.isEmpty()) {
        QString fourccCaps("video/x-raw-yuv, "
                            "framerate = (fraction) [ 0, MAX ], "
                            "width = (int) [ 1, MAX ], "
                            "height = (int) [ 1, MAX ], "
                            "format = (fourcc) { %1 }");
        capsString += fourccCaps.arg(supportedFourcc.join(", "));
    }

    if (!supportedBpp.isEmpty()) {
        QString rgbCaps(GST_VIDEO_CAPS_xRGB_HOST_ENDIAN);

        if (!supportedFourcc.isEmpty())
            capsString += "; ";

        capsString += rgbCaps;
    }

    return gst_caps_from_string(capsString.toLatin1());
}

gboolean QtopiaVideoSink::set_caps(GstBaseSink* sink, GstCaps* caps)
{
    gboolean rc = TRUE;

    if (GST_CAPS_IS_SIMPLE(caps)) {
        GstStructure*       data = gst_caps_get_structure(caps, 0);
        QtopiaVideoSink*    self = G_TYPE_CHECK_INSTANCE_CAST(sink, QtopiaVideoSinkClass::get_type(), QtopiaVideoSink);

        char fourcc[8] = {0,0,0,0,0,0,0,0};

        gst_structure_get_int(data, "width", &self->width);
        gst_structure_get_int(data, "height", &self->height);
        gst_structure_get_int(data, "bpp", &self->bpp);
        gst_structure_get_int(data, "depth", &self->depth);
        bool haveFourcc = gst_structure_get_fourcc(data, "format", (guint32*)fourcc);

        if ( haveFourcc ) {
            // YUV formats
            QMap< QString, QVideoFrame::PixelFormat > formatsMap;
            formatsMap["I420"] = QVideoFrame::Format_YUV420P;
            formatsMap["YV12"] = QVideoFrame::Format_YV12;
            formatsMap["Y444"] = QVideoFrame::Format_YUV444;
            formatsMap["UYVY"] = QVideoFrame::Format_UYVY;
            formatsMap["Y8"] = QVideoFrame::Format_Y8;

            self->pixelFormat = formatsMap.value( QString(fourcc), QVideoFrame::Format_Invalid );
        } else {
            if ( self->bpp == 16 )
                self->pixelFormat = QVideoFrame::Format_RGB565;
            if ( self->bpp == 24 )
                self->pixelFormat = QVideoFrame::Format_RGB24;
            if ( self->bpp == 32 )
                self->pixelFormat = QVideoFrame::Format_RGB32;
        }

        self->widget->setVideoSize(self->width, self->height);
    }
    else
        rc = FALSE;

    return rc;
}

GstStateChangeReturn QtopiaVideoSink::change_state(GstElement* element, GstStateChange transition)
{
    return GST_ELEMENT_CLASS(parentClass)->change_state(element, transition);
}

GstFlowReturn QtopiaVideoSink::render(GstBaseSink* sink, GstBuffer* buf)
{
    GstFlowReturn   rc = GST_FLOW_OK;

    if (buf != 0)
    {
        QtopiaVideoSink*    self = G_TYPE_CHECK_INSTANCE_CAST(sink, QtopiaVideoSinkClass::get_type(), QtopiaVideoSink);

        if ( self->lastBuffer ) {
            gst_buffer_unref(self->lastBuffer);
        }

        self->lastBuffer = buf;
        gst_buffer_ref(self->lastBuffer);

        QVideoFrame frame( self->pixelFormat,
                           QSize( self->width, self->height ),
                           GST_BUFFER_DATA(buf) );

        if (self->widget)
            self->widget->paint( frame );
    }
    else
        rc = GST_FLOW_ERROR;

    return rc;
}

void QtopiaVideoSink::renderLastFrame()
{
    if ( lastBuffer ) {
        QVideoFrame frame( pixelFormat,
                           QSize( width, height ),
                           GST_BUFFER_DATA(lastBuffer) );

        if (widget)
            widget->paint( frame );
    }

}

static GstStaticPadTemplate template_factory =

    GST_STATIC_PAD_TEMPLATE("sink",
                            GST_PAD_SINK,
                            GST_PAD_ALWAYS,
                            GST_STATIC_CAPS("video/x-raw-rgb, "
                                            "framerate = (fraction) [ 0, MAX ], "
                                            "width = (int) [ 1, MAX ], "
                                            "height = (int) [ 1, MAX ]; "
                                            "video/x-raw-yuv, "
                                            "framerate = (fraction) [ 0, MAX ], "
                                            "width = (int) [ 1, MAX ], "
                                            "height = (int) [ 1, MAX ]"));

void QtopiaVideoSink::base_init(gpointer g_class)
{
    gst_element_class_add_pad_template(GST_ELEMENT_CLASS(g_class),
                                       gst_static_pad_template_get(&template_factory));
}


void QtopiaVideoSink::instance_init(GTypeInstance *instance, gpointer g_class)
{
    Q_UNUSED(g_class);

    QtopiaVideoSink* self = reinterpret_cast<QtopiaVideoSink*>(instance);

    self->widget = 0;
    self->width = 0;
    self->height = 0;
    self->bpp = 0;
    self->depth = 0;
    self->pixelFormat = QVideoFrame::Format_Invalid;
    self->lastBuffer = 0;
}
// }}}

// {{{ QtopiaVideoSinkClass
void QtopiaVideoSinkClass::class_init(gpointer g_class, gpointer class_data)
{
    Q_UNUSED(class_data);

    GstBaseSinkClass*   gstBaseSinkClass = (GstBaseSinkClass*)g_class;
    GstElementClass*    gstElementClass = (GstElementClass*)g_class;

    parentClass = reinterpret_cast<GstVideoSinkClass*>(g_type_class_peek_parent(g_class));

    // base
    gstBaseSinkClass->get_caps = QtopiaVideoSink::get_caps;
    gstBaseSinkClass->set_caps = QtopiaVideoSink::set_caps;
//    gstbasesink_class->buffer_alloc = buffer_alloc;
//    gstbasesink_class->get_times = get_times;
    gstBaseSinkClass->preroll = QtopiaVideoSink::render;
    gstBaseSinkClass->render = QtopiaVideoSink::render;

    // element
    gstElementClass->change_state = QtopiaVideoSink::change_state;
}

GType QtopiaVideoSinkClass::get_type()
{
    static GType type = 0;

    if (type == 0)
    {
        static const GTypeInfo info =
        {
            sizeof(QtopiaVideoSinkClass),                       // class_size
            QtopiaVideoSink::base_init,                         // base_init
            NULL,                                               // base_finalize

            QtopiaVideoSinkClass::class_init,                   // class_init
            NULL,                                               // class_finalize
            NULL,                                               // class_data

            sizeof(QtopiaVideoSink),                            // instance_size
            0,                                                  // n_preallocs
            QtopiaVideoSink::instance_init,                     // instance_init
            0                                                   // value_table
        };

        type = g_type_register_static(GST_TYPE_VIDEO_SINK,
                                     "QtopiaVideoSink",
                                      &info,
                                      GTypeFlags(0));
    }

    return type;
}
// }}}

}   // ns gstreamer
