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

#include "qaudiosourceselector.h"

#include <QDSAction>
#include <QDSData>
#include <QDSServices>
#include <QLabel>
#include <QLayout>
#include <QPushButton>
#include <QMimeType>

#include <qtopiaapplication.h>
#include <qdocumentselector.h>

#include "resourcesourceselector_p.h"

//===========================================================================

// Perhaps this should be exported as QAudioDocumentSelectorDialog?
class AudioDocumentSelectorDialog : public QDialog
{
    Q_OBJECT

public:
    AudioDocumentSelectorDialog(QWidget *parent)
        : QDialog(parent)
    {
        setWindowTitle(tr("Select Audio"));
        setModal( true );

        QtopiaApplication::setMenuLike(this, true);

        fileSelector = new QDocumentSelector(this);
        fileSelector->setFilter( QContentFilter( QContent::Document ) & 
                                 QContentFilter( QContentFilter::MimeType, "audio/*" ) );
        fileSelector->disableOptions( QDocumentSelector::ContextMenu );
        connect(fileSelector, SIGNAL(documentSelected(QContent)),
                this, SLOT(documentSelected(QContent)));

        QVBoxLayout *vb = new QVBoxLayout(this);
        vb->addWidget(fileSelector);
    }

    QContent selected() const { return file; }

protected slots:
    void documentSelected(const QContent &doc) 
    {
        file = doc;
        accept();
    }

private:
    QDocumentSelector *fileSelector;
    QContent file;
};

//===========================================================================

// This class is not currently exported:

class QAudioSourceSelectorPrivate;

class /*QTOPIA_EXPORT*/ QAudioSourceSelector : public QWidget
{
    Q_OBJECT

public:
    QAudioSourceSelector( QWidget *parent );
    virtual ~QAudioSourceSelector();

    void setDefaultAudio(const QString &type, const QString &subFormat, int fr, int ch);

    QContent content() const;

public slots:
    void setContent( const QContent& audio );

    void change();
    void remove();

    void serviceRequest( const QString& type, QDSAction& action );

private:
    void init();

    QAudioSourceSelectorPrivate* d;
};

//===========================================================================

class QAudioSourceSelectorPrivate : public ResourceSourceSelector
{
public:
    QAudioSourceSelectorPrivate( QWidget* parent )
        : ResourceSourceSelector( parent ) {}

    QContent audioResource;
    QString audioType;
    QString audioSubFormat;
    int audioFrequency;
    int audioChannels;
};

QAudioSourceSelector::QAudioSourceSelector( QWidget *parent )
    : QWidget( parent ),
      d( new QAudioSourceSelectorPrivate( this ) )
{
    init();
}

QAudioSourceSelector::~QAudioSourceSelector()
{
}

void QAudioSourceSelector::init()
{
    // Describe the resource services we want to expose
    ResourceSourceSelector::ServicesDescriptor
        getSvcs( QStringList( "get" ),
                 "x-parameters/x-audioparameters",
                 "audio/x-qstring",
                 ResourceSourceSelector::NoContentRequired );

    ResourceSourceSelector::ServicesList list;
    list.append( getSvcs );

    d->init( ResourceSourceSelector::VerticalArrangement, &list );

    d->changeButton()->setText( tr("Audio Files") );
    d->removeButton()->setText( tr("Remove") );

    d->connectSignals( this );

    QVBoxLayout* l = new QVBoxLayout( this );
    l->addWidget( d );
}

void QAudioSourceSelector::setDefaultAudio( const QString &type, const QString &subFormat, int fr, int ch )
{
    d->audioType = type;
    d->audioSubFormat = subFormat;
    d->audioFrequency = fr;
    d->audioChannels = ch;
}

void QAudioSourceSelector::setContent( const QContent &doc )
{
    d->audioResource = doc;

    QFont f = font();
    if ( !doc.fileKnown() ) {
        d->haveResource( false );

        f.setItalic( true );
        d->label()->setFont( f );
        d->label()->setText( tr("No Audio") );
    } else {
        d->haveResource( true );

        d->label()->setFont( f );
        d->label()->setText( doc.name() );
    }
}

QContent QAudioSourceSelector::content() const
{
    return d->audioResource;
}

void QAudioSourceSelector::change()
{
    AudioDocumentSelectorDialog *dlg = new AudioDocumentSelectorDialog(parentWidget());
    dlg->showMaximized();

    if (QtopiaApplication::execDialog(dlg) == QDialog::Accepted) {
        if (dlg->selected().fileKnown())
            setContent(dlg->selected());
    }

    delete dlg;
}

void QAudioSourceSelector::remove()
{
    setContent(QContent());
}

void QAudioSourceSelector::serviceRequest( const QString& type, QDSAction& action )
{
    if ( type != "get" )
        return;

    QByteArray parametersArray;
    {
        QDataStream stream( &parametersArray, QIODevice::WriteOnly );

        stream << QMimeType(d->audioType).id()
               << d->audioSubFormat
               << d->audioFrequency
               << d->audioChannels;
    }
    QDSData parameters( parametersArray, QMimeType( "x-parameters/x-audioparameters" ) );

    if ( action.exec( parameters ) == QDSAction::CompleteData ) {
        QDataStream stream( action.responseData().toIODevice() );
        QString filename;
        stream >> filename;
        setContent( QContent( filename ) );
    } else {
	qWarning( "%s", action.errorMessage().toLatin1().constData() );
    }
}

//===========================================================================

/*!
    \class QAudioSourceSelectorDialog
    \inpublicgroup QtBaseModule

    \brief The QAudioSourceSelectorDialog class allows the user to select an audio document from a variety of sources.
    \ingroup documentselection

    QAudioSourceSelectorDialog allows the user to select an audio document from the documents
    system or from any QDS service on the device that allows an audio document to be retrieved, such
    as a microphone recording.  It returns the user's selection to the client code as a QContent object.

    The dialog will allow the user to invoke any QDS service that returns data in "audio/x-qstring" form
    and has the "get" attribute, or to select an existing audio document.

    The following code uses QAudioSourceSelectorDialog to allow the user to select an audio 
    document from any source:

    \code
    QAudioSourceSelectorDialog dialog( this );
    if( QtopiaApplication::execDialog( &dialog ) ) {
        // Accept
        QContent audio = dialog.content();
    } else {
        // Reject
    }
    \endcode

    \sa QDSServiceInfo, QDocumentSelectorDialog
*/

/*!
    Constructs a QAudioSourceSelectorDialog as a child of \a parent.
*/
QAudioSourceSelectorDialog::QAudioSourceSelectorDialog( QWidget *parent )
    : QDialog( parent )
{
    selector = new QAudioSourceSelector( this );

    QVBoxLayout *l = new QVBoxLayout( this );
    l->addWidget( selector );
}

/*!
    Destroys the QAudioSourceSelectorDialog object.
*/
QAudioSourceSelectorDialog::~QAudioSourceSelectorDialog()
{
}

/*!
    Sets the audio parameters to use if the user chooses to acquire new audio data.

    The new audio data is requested to be of MIME type \a type, formatted as \a subFormat with
    the supplied \a frequency and number of \a channels.
*/
void QAudioSourceSelectorDialog::setDefaultAudio( const QString &type, const QString &subFormat, int frequency, int channels )
{
    selector->setDefaultAudio( type, subFormat, frequency, channels );
}

/*!
    Sets the dialog to show \a audio as the selected audio document.
*/
void QAudioSourceSelectorDialog::setContent( const QContent &audio )
{
    selector->setContent( audio );
}

/*!
    Returns the audio document selected by the user.
*/
QContent QAudioSourceSelectorDialog::content() const
{
    return selector->content();
}

#include "qaudiosourceselector.moc"
