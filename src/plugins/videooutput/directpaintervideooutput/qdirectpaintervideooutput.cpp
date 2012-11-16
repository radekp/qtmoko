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

#include "qdirectpaintervideooutput.h"
#include <QTransform>
#include <QImage>
#include <QDirectPainter>
#include <QPainter>
#include <QDebug>
#include <QTime>
#include <QMutex>

#include <QtGui/qscreen_qws.h>
#include <gfxpainter.h>

#include "qimageplanetransform.h"


#define USE_GFX_PAINTER 0
#define USE_IMAGE_SCALLER 1


class QDirectPainterVideoOutputPrivate {
public:
    QDirectPainterVideoOutputPrivate()
        :blackRegionUpdates(0) {
    }
    QRegion prevClipRegion;

    QSize videoSize;
    QTransform transform;
    QRect imageRect;

    QRegion blackRegion;
    int blackRegionUpdates;

    QImage frameBufferImage;

    QList<QImagePlaneTransformation*> imageTransformations;
    QVideoFormatList preferredFormats;
    QVideoFormatList supportedFormats;
};

QDirectPainterVideoOutput::QDirectPainterVideoOutput( QScreen *_screen, QObject *parent )
    : QAbstractVideoOutput(_screen, parent)
    , d( new QDirectPainterVideoOutputPrivate )
{
    d->frameBufferImage = QImage(screen()->base(),
                                 screen()->width(),
                                 screen()->height(),
                                 screen()->depth() == 16 ? QImage::Format_RGB16 : QImage::Format_RGB32);

    QVideoFrame::PixelFormat nativeFormat = nativeScreenPixelFormat();
    d->preferredFormats << nativeFormat;
    d->supportedFormats << nativeFormat;

    QImagePlaneTransformation t;

    for ( int format = 0; format < QVideoFrame::NVideoFormats; format++ ) {
        if ( format != nativeFormat && t.isTransformationSupported( (QVideoFrame::PixelFormat)format,  nativeFormat ) )
            d->supportedFormats << (QVideoFrame::PixelFormat)format;
    }
}

QDirectPainterVideoOutput::~QDirectPainterVideoOutput()
{
    qDeleteAll( d->imageTransformations );
    d->imageTransformations.clear();
}

void QDirectPainterVideoOutput::doRenderFrame( const QVideoFrame& frame )
{
    if ( frame.isNull() ) {
        screen()->solidFill( Qt::black, clipRegion() );
        screen()->setDirty( clipRegion().boundingRect() );
        return;
    }

    d->prevClipRegion = clipRegion();

    if ( frame.size() != d->videoSize ) {
        d->videoSize = frame.size();
        setModified(true);
    }

    QRegion paintRegion = deviceMappedClipRegion();
    QRect geometry = deviceMappedGeometry();

    //limit paintRegion with screen geometry
    QRect screenGeometry( 0, 0, screen()->width(),  screen()->height() );
    if ( screen()->isTransformed() )
        screenGeometry = screen()->mapToDevice( screenGeometry, screenGeometry.size() );

    paintRegion &= QRegion( screenGeometry );

    if ( isModified() ) {
        setModified(false);
        d->transform.reset();
        QSizeF imageSize = d->videoSize;

        //respect frame aspect ratio
        if ( frame.hasCustomAspectRatio() ) {
            double newWidth = imageSize.height() * frame.aspectRatio();
            d->transform *= QTransform().scale( newWidth / imageSize.width(), 1.0 );
            imageSize.setWidth( newWidth );
        }

        switch ( effectiveRotation() ) {
            case QtopiaVideo::Rotate0:
                break;
            case QtopiaVideo::Rotate90:
                d->transform *= QTransform().rotate(90);
                d->transform *= QTransform().translate( imageSize.height(), 0 );
                imageSize.transpose();
                break;
            case QtopiaVideo::Rotate180:
                d->transform *= QTransform().rotate(180);
                d->transform *= QTransform().translate( imageSize.width(), imageSize.height() );
                break;
            case QtopiaVideo::Rotate270:
                d->transform *= QTransform().rotate(270);
                d->transform *= QTransform().translate( 0, imageSize.width() );
                imageSize.transpose();
                break;
        };

        //scale to fit viewport, if asked
        if ( scaleMode() == QtopiaVideo::FitWindow ) {
            double scaleFactor = qMin( double(geometry.width())/imageSize.width(),
                                       double(geometry.height())/imageSize.height() );

            //don't scale if the size is close to required
            if ( scaleFactor < 0.95 || scaleFactor > 1.2 ) {
                d->transform *= QTransform().scale( scaleFactor, scaleFactor );
                imageSize *= scaleFactor;
            }
        }

        //center image:
        d->imageRect = QRect( QPoint(0,0), imageSize.toSize() );
        d->imageRect.moveCenter( geometry.center() );
        d->transform *= QTransform().translate( d->imageRect.left(), d->imageRect.top() );

        d->blackRegion = paintRegion - QRegion(d->imageRect);
        d->blackRegionUpdates = d->blackRegion.isEmpty() ? 0 : 2;

#if USE_IMAGE_SCALLER
        qDeleteAll( d->imageTransformations );
        d->imageTransformations.clear();
        QTransform invertedTransform = d->transform.inverted();

        int srcLineStep = frame.bytesPerLine(0)/( frame.colorDepth(frame.format(),0)/8 );
        int dstLineStep = screen()->linestep()/(screen()->depth()/8);

        d->blackRegion = paintRegion;

        foreach( QRect r, paintRegion.rects() ) {
            QRectF dstRect = r;
            QRectF srcRect( dstRect.topLeft()*invertedTransform, dstRect.bottomRight()*invertedTransform );
            srcRect = srcRect.normalized();

            QRect srcArea( QPoint(0,0 ), frame.size() );
            QRect dstArea( geometry );

            QImagePlaneTransformation *transformation = new QImagePlaneTransformation();
            transformation->setSrcGeometry( srcRect.toRect(), srcArea, srcLineStep );
            transformation->setDstGeometry( dstRect.toRect(), dstArea, dstLineStep );
            transformation->setRotation( effectiveRotation() );

            d->blackRegion -= transformation->clippedDstGeometry();

            if ( !transformation->clippedDstGeometry().isEmpty() )
                d->imageTransformations << transformation;
            else
                delete transformation;
        }
#endif
    }

    if ( paintRegion.isEmpty() )
        return;

    if ( d->blackRegionUpdates > 0 ) {
        QRegion blackRegion = d->blackRegion;
        switch ( screen()->transformOrientation() ) {
            case 0:
            case 2:
                blackRegion = screen()->mapFromDevice( blackRegion, QSize( screen()->width(), screen()->height() ) );
                break;
            case 1:
            case 3:
                blackRegion = screen()->mapFromDevice( blackRegion, QSize( screen()->height(), screen()->width() ) );
                break;
            default:
                break;
        };
        blackRegion = blackRegion.translated( screen()->offset() );
        screen()->solidFill( Qt::black, blackRegion );
        d->blackRegionUpdates--;
    }

    {
        QImage::Format imageFormat = QImage::Format_Invalid;

        switch ( frame.format() ) {
        case QVideoFrame::Format_RGB565:
            imageFormat = QImage::Format_RGB16;
            break;
        case QVideoFrame::Format_RGB32:
            imageFormat = QImage::Format_RGB32;
            break;
        case QVideoFrame::Format_RGB24:
            imageFormat = QImage::Format_RGB888;
            break;
        default:
            break;
        }

        uchar* imageData = (uchar *) frame.planeData(0);
        int count = frame.planeSize(0).width() * frame.planeSize(0).height() * 4;
        for(int i = 0; i < count; i+=4)
        {
            uchar r = imageData[i];
            imageData[i] = imageData[i+2];
            imageData[i+2] = r;
        }

        //QTime t;
        //t.start();

#if USE_IMAGE_SCALLER
        foreach( QImagePlaneTransformation *transformation, d->imageTransformations ) {
            transformation->transformPlane( imageData, frame.format(), screen()->base(), nativeScreenPixelFormat() );
        }
#elif USE_GFX_PAINTER
        QImage frameImage( imageData,
                           frame.size().width(),
                           frame.size().height(),
                           frame.bytesPerLine(0),
                           imageFormat );

        GfxPainter p( d->frameBufferImage, paintRegion );
        p.drawImageTransformed( d->transform.toAffine(), frameImage );
#else
        QImage frameImage( imageData,
                           frame.size().width(),
                           frame.size().height(),
                           frame.bytesPerLine(0),
                           imageFormat );

        QPainter    p(&d->frameBufferImage);
        p.setClipRegion(paintRegion);
        p.setWorldTransform(d->transform);
        p.drawImage( 0, 0, frameImage );
#endif
        screen()->setDirty( clipRegion().boundingRect() );
    }

}

QVideoFrame::PixelFormat QDirectPainterVideoOutput::nativeScreenPixelFormat()
{
    if ( screen()->depth() == 16 )
        return screen()->pixelType() == QScreen::NormalPixel ? QVideoFrame::Format_RGB565 : QVideoFrame::Format_BGR565;

    if ( screen()->depth() == 24 )
        return screen()->pixelType() == QScreen::NormalPixel ? QVideoFrame::Format_RGB24 : QVideoFrame::Format_BGR24;

    if ( screen()->depth() == 32 )
        return screen()->pixelType() == QScreen::NormalPixel ? QVideoFrame::Format_RGB32 : QVideoFrame::Format_BGR32;

    return QVideoFrame::Format_Invalid;
}

QVideoFormatList QDirectPainterVideoOutput::supportedFormats()
{
    return d->supportedFormats;
}


QVideoFormatList QDirectPainterVideoOutput::preferredFormats()
{
    QVideoFormatList res;
    res << nativeScreenPixelFormat();
    return res;
}

QtopiaVideo::VideoRotation QDirectPainterVideoOutput::effectiveRotation() const
{
    return QtopiaVideo::VideoRotation( ( int(rotation()) - screen()->transformOrientation() + 4 ) % 4 );
}


QDirectPainterVideoOutputFactory::QDirectPainterVideoOutputFactory()
    :QObject(), QVideoOutputFactory()
{
}

QDirectPainterVideoOutputFactory::~QDirectPainterVideoOutputFactory()
{
}

QAbstractVideoOutput*  QDirectPainterVideoOutputFactory::create( QScreen *screen, QObject *parent )
{
    return new QDirectPainterVideoOutput( screen, parent );
}

QVideoFormatList QDirectPainterVideoOutputFactory::supportedFormats()
{
    QVideoFormatList res;
    res << QVideoFrame::Format_RGB565;
    res << QVideoFrame::Format_RGB24;
    res << QVideoFrame::Format_RGB32;

    return res;
}

QVideoFormatList QDirectPainterVideoOutputFactory::preferredFormats()
{
    return supportedFormats();
}

QTOPIA_EXPORT_PLUGIN(QDirectPainterVideoOutputFactory);

