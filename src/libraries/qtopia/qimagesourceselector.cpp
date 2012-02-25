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

#include "qimagesourceselector.h"

#include <QDSAction>
#include <QDSData>
#include <QDSServices>
#include <QImageDocumentSelector>
#include <QLabel>
#include <QLayout>
#include <QPushButton>
#include <QThumbnail>
#include <QPixmap>
#include <QImageReader>

#include <qtopiaapplication.h>
#include <qdesktopwidget.h>

#include "resourcesourceselector_p.h"

// The selector class is currently not exported:

class QImageSourceSelectorPrivate;

class /*QTOPIA_EXPORT*/ QImageSourceSelector : public QWidget
{
    Q_OBJECT

public:
    QImageSourceSelector( QWidget *parent );
    virtual ~QImageSourceSelector();

    void setMaximumImageSize( const QSize &s );
    QSize maximumImageSize() const;

    QContent content() const;

public slots:
    virtual void setContent( const QContent &image );

    void change();
    void remove();

    void serviceRequest( const QString& type, QDSAction& action );

protected:
    virtual void resizeEvent( QResizeEvent* );

private:
    void init();

    QImageSourceSelectorPrivate* d;
};

class QImageSourceSelectorPrivate : public ResourceSourceSelector
{
    Q_OBJECT
public:
    QImageSourceSelectorPrivate( QWidget* parent )
        : ResourceSourceSelector( parent ),
          maxSize( 0, 0 ),
          imageSize( 0, 0 ),
          pixmapSize( 0, 0 )
    {
    }

    QSize maxSize;

    QContent imageResource;
    QPixmap temporaryImage;
    QSize imageSize;

    QPixmap pixmap;
    QSize pixmapSize;

    QSize labelSize() 
    { 
        return label()->size(); 
    }

    QSize drawSize() 
    { 
        // The drawn image must fit on the label
        QSize visibleSize = labelSize();
        if ( !imageSize.isNull() )
            visibleSize = visibleSize.boundedTo( imageSize );

        return visibleSize;
    }

    void setLabel( const QString& text )
    {
        QFont f = font();
        f.setItalic( true );
        label()->setFont( f );
        label()->setText( text );
    }

    void setLabel( QImage& image )
    {
        if ( !image.isNull() )
        {
            pixmap = QPixmap::fromImage( image );
            setLabelPixmap( pixmap );
        }
        else
        {
            pixmap = QPixmap();
        }
    }

    void setLabel( QDataStream& stream )
    {
        imageResource = QContent();
        stream >> temporaryImage;

        if ( !temporaryImage.isNull() )
        {
            setLabelPixmap( temporaryImage );
        }
        else
        {
            pixmap = QPixmap();
        }
    }

    void redrawLabel()
    {
        if (!pixmap.isNull() && !pixmapSize.isNull())
        {
            QSize newSize = labelPixmapSize( pixmap );
            if (newSize != pixmapSize)
            {
                // Resize the pixmap to be the new label size
                pixmapSize = newSize;
                label()->setPixmap( pixmap.scaled( pixmapSize, Qt::KeepAspectRatio, Qt::SmoothTransformation ) );
            }
        }
    }

    QPixmap getPixmap() const
    {
        if ( imageResource.isValid() )
        {
            QIODevice* io = imageResource.open();
            QImageReader reader( io );
            QPixmap result = QPixmap::fromImage( reader.read() );
            delete io;

            return result;
        }
        else if ( !temporaryImage.isNull() )
        {
            return temporaryImage;
        }

        return QPixmap();
    }

    QContent content()
    {
        if ( !imageResource.isValid() && !temporaryImage.isNull() )
        {
            // We need to save our temporary data into the content system
            QString name = tr("%1 %2","date,time")
                .arg(QTimeString::localYMD(QDate::currentDate(),QTimeString::Short))
                .arg(QTimeString::localHMS(QTime::currentTime()));

            imageResource.setName( name );
            imageResource.setType( "image/png" );

            // TODO: categorise this image?

            QIODevice* io = imageResource.open(QIODevice::WriteOnly);
            temporaryImage.save(io, "PNG");
            delete io;

            imageResource.commit();
        }

        return imageResource;
    }

private:
    QSize labelPixmapSize( QPixmap& pixmap )
    {
        QSize currentSize = pixmap.size();

        // Ensure the image is drawn to fit onto the label
        QSize visibleSize = drawSize();

        if ( currentSize.width() > visibleSize.width() || currentSize.height() > visibleSize.height() )
            currentSize.scale( visibleSize, Qt::KeepAspectRatio );

        return currentSize;
    }

    void setLabelPixmap( QPixmap& pixmap )
    {
        if (isVisible()) 
        {
            // Ensure the image is drawn to fit onto the label
            pixmapSize = labelPixmapSize(pixmap);

            if (pixmapSize == pixmap.size())
                label()->setPixmap(pixmap);
            else
                label()->setPixmap(pixmap.scaled( pixmapSize, Qt::KeepAspectRatio, Qt::SmoothTransformation ));
        }
    }

    void showEvent( QShowEvent* event )
    {
        ResourceSourceSelector::showEvent(event);

        if (label()->pixmap() == 0)
        {
            if ( !temporaryImage.isNull() )
                setLabelPixmap(temporaryImage);
            else if ( !pixmap.isNull() )
                setLabelPixmap(pixmap);
        }
    }
};

//===========================================================================

QImageSourceSelector::QImageSourceSelector( QWidget *parent )
    : QWidget( parent ),
      d( new QImageSourceSelectorPrivate( this ) )
{
    init();
}

QImageSourceSelector::~QImageSourceSelector()
{
}

void QImageSourceSelector::init()
{
    // Describe the services we will exopse
    ResourceSourceSelector::ServicesDescriptor
        getSvcs( QStringList( "get" ),
                 "x-size/x-qsize",
                 "image/x-qpixmap",
                 ResourceSourceSelector::NoContentRequired );

    ResourceSourceSelector::ServicesDescriptor
        editSvcs( QStringList( "edit" ),
                  "image/x-qpixmap",
                  "image/x-qpixmap",
                  ResourceSourceSelector::ContentRequired );

    ResourceSourceSelector::ServicesList list;
    list.append( getSvcs );
    list.append( editSvcs );

    d->init( ResourceSourceSelector::HorizontalArrangement, &list );

    d->changeButton()->setText( tr("Pictures") );
    d->removeButton()->setText( tr("Remove") );

    d->connectSignals ( this );

    QVBoxLayout* l = new QVBoxLayout( this );
    l->setContentsMargins(0, 0, 0, 0);
    l->addWidget( d );
}

void QImageSourceSelector::setMaximumImageSize( const QSize &s )
{
    d->maxSize = s;
}

QSize QImageSourceSelector::maximumImageSize() const
{
    return d->maxSize;
}

void QImageSourceSelector::setContent( const QContent& image )
{
    bool haveImage = false;

    d->imageResource = image;
    if( d->imageResource.isValid() )
    {
        QIODevice* io = d->imageResource.open();
        QImageReader reader( io );
        if ( reader.supportsOption( QImageIOHandler::Size ) )
        {
            d->imageSize = reader.size();

            // We would prefer to down-scale the image as we load it - the downscaled
            // size can be larger than the current paint size, because we might resize
            // the size to paint it at larger when we redraw.
            // We will load at a size constrained by image size and screen geometry
            QSize loadSize = d->imageSize;
            loadSize = loadSize.boundedTo( QApplication::desktop()->availableGeometry().size() );

            // See if the image needs to be down-scaled during load
            if ( d->imageSize.width() > loadSize.width() || d->imageSize.height() > loadSize.height() )
            {
                // And the loaded size should maintain the image aspect ratio
                QSize reducedSize( d->imageSize );
                reducedSize.scale( loadSize, Qt::KeepAspectRatio );
                reader.setQuality( 49 ); // Otherwise Qt smooth scales
                reader.setScaledSize( reducedSize );
            }
        }

        QImage image = reader.read();
        if ( !reader.supportsOption( QImageIOHandler::Size ) )
            d->imageSize = image.size();

        delete io;

        d->setLabel( image );
        haveImage = !image.isNull();
    }

    d->haveResource( haveImage );

    if ( !haveImage )
    {
        d->setLabel( tr("No Image") );
        d->imageSize = QSize( 0, 0 );
        d->pixmapSize = QSize( 0, 0 );
    }
}

QContent QImageSourceSelector::content() const
{
    return d->content();
}

void QImageSourceSelector::change()
{
    QImageDocumentSelectorDialog *dlg = new QImageDocumentSelectorDialog( this );
    dlg->setModal(true);

    if( QtopiaApplication::execDialog( dlg ) == QDialog::Accepted ) 
    {
        setContent( dlg->selectedDocument() );
    }

    delete dlg;
}

void QImageSourceSelector::remove()
{
    setContent( QContent() );
}

void QImageSourceSelector::serviceRequest( const QString& type, QDSAction& action )
{
    QMimeType mimeType;
    QByteArray parametersArray;
    {
        QDataStream stream( &parametersArray, QIODevice::WriteOnly );

        if ( type == "get" )
        {
            stream << d->maxSize;
            mimeType = QMimeType( "x-size/x-qsize" );
        }
        else if ( type == "edit" )
        {
            stream << d->getPixmap();
            mimeType = QMimeType( "image/x-qpixmap" );
        }
    }
    QDSData parameters( parametersArray, mimeType );

    if ( action.exec( parameters ) == QDSAction::CompleteData ) {
        QDataStream stream( action.responseData().toIODevice() );

        if ( type == "get" || type == "edit" )
        {
            d->setLabel( stream );
        }
    } else {
	qWarning( "%s", action.errorMessage().toLatin1().constData() );
    }
}

void QImageSourceSelector::resizeEvent( QResizeEvent *e )
{
    QWidget::resizeEvent( e );

    d->redrawLabel();
}

//===========================================================================

/*!
    \class QImageSourceSelectorDialog
    \inpublicgroup QtBaseModule

    \brief The QImageSourceSelectorDialog class allows the user to select an image from a variety of sources.
    \ingroup documentselection

    QImageSourceSelectorDialog allows the user to select an image from the documents
    system or from any QDS service on the device that allows a image to be retrieved, such
    as a camera.  It allows the selected image to be modified by any QDS service on the
    device that can modify images, and returns the user's selection to the client
    code as a QContent object.

    The dialog will allow the user to invoke any QDS service that returns data in 
    "image/x-qpixmap" form and has the "get" attribute, or to select an image document.  
    Once an image is selected, the dialog will allow the user to invoke any QDS service 
    that returns data in "image/x-qpixmap" form and has the "edit" attribute.

    The following code uses QImageSourceSelectorDialog to allow the user
    to select an image from any source:

    \code
    QImageSourceSelectorDialog dialog( this );
    if( QtopiaApplication::execDialog( &dialog ) ) {
        // Accept
        QContent image = dialog.content();
    } else {
        // Reject
    }
    \endcode

    \sa QImageDocumentSelector, QDSServiceInfo
*/

/*!
    Constructs a QImageSourceSelectorDialog as a child of \a parent.
*/
QImageSourceSelectorDialog::QImageSourceSelectorDialog( QWidget *parent )
    : QDialog( parent )
{
    selector = new QImageSourceSelector( this );

    QVBoxLayout *l = new QVBoxLayout( this );
    l->setContentsMargins(0, 0, 0, 0);
    l->addWidget( selector );
}

/*!
    Destroys the QImageSourceSelectorDialog object.
*/
QImageSourceSelectorDialog::~QImageSourceSelectorDialog()
{
}

/*!
    Sets the maximum size forwarded to the image retrieval QDS services to \a size.
    This size might be used by a camera, for example, to determine at what size to
    take a picture.
*/
void QImageSourceSelectorDialog::setMaximumImageSize( const QSize &size )
{
    selector->setMaximumImageSize( size );
}

/*!
    Returns the maximum size used by the image retrieval QDS services.
*/
QSize QImageSourceSelectorDialog::maximumImageSize() const
{
    return selector->maximumImageSize();
}

/*! 
    Sets the dialog to show \a image as the selected image.
*/
void QImageSourceSelectorDialog::setContent( const QContent& image )
{
    selector->setContent( image );
}

/*!
    Returns the image selected by the user.  If the user accepts the dialog 
    while the currently selected image is not stored, the dialog will store 
    the image in PNG format.
*/
QContent QImageSourceSelectorDialog::content() const
{
    return selector->content();
}

#include "qimagesourceselector.moc"
