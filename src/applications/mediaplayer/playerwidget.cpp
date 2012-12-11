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

#include "playerwidget.h"

#include "elidedlabel.h"
#include "visualization.h"
#include "keyfilter.h"
#include "keyhold.h"
#include "mediaplayer.h"

#include <media.h>
#include <qmediatools.h>
#include <qmediawidgets.h>
#ifndef NO_HELIX
#include <qmediahelixsettingscontrol.h>
#endif
#include <qmediavideocontrol.h>

#include <qsoftmenubar.h>
#include <custom.h>

#include <qtranslatablesettings.h>
#include <qtopiaapplication.h>
#include <qthumbnail.h>

#include <QMarqueeLabel>
#include <QScreenInformation>
#include <QFrame>
#include <qscreen_qws.h>
#include <qwsevent_qws.h>
#include <QMediaPlaylist>

#include <private/qmediametainfocontrol_p.h>

#ifndef NO_HELIX
class HelixLogo : public QWidget
{
    Q_OBJECT
public:
    HelixLogo( QWidget* parent = 0 );

    // QWidget
    QSize sizeHint() const { return QSize( m_helixlogo.width(), m_helixlogo.height() ); }

protected:
    // QWidget
    void resizeEvent( QResizeEvent* e );
    void paintEvent( QPaintEvent* e );
    void showEvent ( QShowEvent * event );

private:
    QImage m_helixlogo;
    QPoint m_helixlogopos;
    QTimeLine timeLine;
    qreal opacity;

private slots:
    void setOpacity(int);
    void checkTimeLine();
};

static const QString HELIX_LOGO_PATH = ":image/mediaplayer/helixlogo";

HelixLogo::HelixLogo( QWidget* parent )
    : QWidget( parent )
{
    m_helixlogo = QImage( HELIX_LOGO_PATH );
    timeLine.setFrameRange(0, 255);
    connect(&timeLine, SIGNAL(frameChanged(int)), this, SLOT(setOpacity(int)));
    connect(&timeLine, SIGNAL(finished()), this, SLOT(checkTimeLine()));
}

void HelixLogo::resizeEvent( QResizeEvent* )
{
    m_helixlogopos = QPoint( (width() - m_helixlogo.width()) / 2,
        (height() - m_helixlogo.height()) / 2 );
}

void HelixLogo::paintEvent( QPaintEvent* )
{
    QPainter painter( this );
    qreal oldopacity = painter.opacity();
    painter.setOpacity( opacity );
    painter.drawImage( m_helixlogopos, m_helixlogo );
    painter.setOpacity( oldopacity );
}

void HelixLogo::showEvent( QShowEvent * )
{
    timeLine.setDirection(QTimeLine::Forward);
    timeLine.setDuration(500);
    timeLine.start();
}

void HelixLogo::setOpacity(int pOpacity)
{
    qreal oldopacity=opacity;
    opacity=(qreal)pOpacity/255.0;
    if(oldopacity != opacity)
        repaint();
}

void HelixLogo::checkTimeLine()
{
    if(timeLine.direction()==QTimeLine::Forward)
    {
        timeLine.setDirection(QTimeLine::Backward);
        timeLine.setDuration(1500);
        timeLine.start();
    }
    else
    {
        hide();
    }
}

#endif

#ifndef NO_HELIX
MediaPlayerSettingsDialog::MediaPlayerSettingsDialog( QWidget* parent )
    : QDialog( parent )
{
    static const int TIMEOUT_MIN = 5;
    static const int TIMEOUT_MAX = 99;

    QVBoxLayout *layout = new QVBoxLayout;
    layout->setMargin( 4 );

    QGroupBox *group = new QGroupBox( tr( "Network Settings" ), this );

    QVBoxLayout *vbox = new QVBoxLayout;
    vbox->addWidget( new QLabel( tr( "Connection Speed" ) ) );

    m_speedcombo = new QComboBox;
    vbox->addWidget( m_speedcombo );

    QFrame *hr = new QFrame;
    hr->setFrameStyle( QFrame::HLine | QFrame::Plain );
    vbox->addWidget( hr );

    QGridLayout *grid = new QGridLayout;
    grid->addWidget( new QLabel( tr( "Timeouts:" )), 0, 0, 1, 2, Qt::AlignCenter );
    grid->addWidget( new QLabel( tr( "Connection" ) ), 1, 0, Qt::AlignLeft );

    m_validator = new QIntValidator( TIMEOUT_MIN, TIMEOUT_MAX, this );

    m_connecttimeout = new QLineEdit;
    m_connecttimeout->setAlignment( Qt::AlignRight );
    m_connecttimeout->setValidator( m_validator );
    grid->addWidget( m_connecttimeout, 1, 1, Qt::AlignRight );

    grid->addWidget( new QLabel( tr( "Server" ) ), 2, 0, Qt::AlignLeft );

    m_servertimeout = new QLineEdit;
    m_servertimeout->setAlignment( Qt::AlignRight );
    m_servertimeout->setValidator( m_validator );
    grid->addWidget( m_servertimeout, 2, 1, Qt::AlignRight );

    vbox->addLayout( grid );
    vbox->addStretch();

    group->setLayout( vbox );

    layout->addWidget( group );
    setLayout( layout );

    // Initialize settings
    readConfig();

    applySettings();
}

void MediaPlayerSettingsDialog::accept()
{
    writeConfig();
    applySettings();

    QDialog::accept();
}

void MediaPlayerSettingsDialog::readConfig()
{
    QTranslatableSettings config( "Trolltech", "MediaPlayer" );
    config.beginGroup( "Network" );

    int size = config.beginReadArray( "ConnectionSpeed" );
    for( int i = 0; i < size; ++i ) {
        config.setArrayIndex( i );
        m_speedcombo->addItem( config.value( "Type" ).toString(), config.value( "Speed" ) );
    }
    config.endArray();

    if( !m_speedcombo->count() ) {
        m_speedcombo->addItem( tr("GPRS (32 kbps)"), 32000 );
        m_speedcombo->addItem( tr("EGPRS (128 kbps)"), 128000 );
    }

    QVariant value = config.value( "ConnectionSpeedIndex" );
    if( value.isNull() || value.toInt() > m_speedcombo->count() ) {
        value = 0;
    }
    m_speedcombo->setCurrentIndex( value.toInt() );

    value = config.value( "ConnectionTimeout" );
    if( value.isNull() ) {
        value = "5";
    }
    m_connecttimeout->setText( value.toString() );

    value = config.value( "ServerTimeout" );
    if( value.isNull() ) {
        value = "5";
    }
    m_servertimeout->setText( value.toString() );
}

void MediaPlayerSettingsDialog::writeConfig()
{
    QTranslatableSettings config( "Trolltech", "MediaPlayer" );
    config.beginGroup( "Network" );

    config.beginWriteArray( "ConnectionSpeed" );
    for( int i = 0; i < m_speedcombo->count(); ++i ) {
        config.setArrayIndex( i );
        config.setValue( "Type", m_speedcombo->itemText( i ) );
        config.setValue( "Speed", m_speedcombo->itemData( i ) );
    }
    config.endArray();

    config.setValue( "ConnectionSpeedIndex", m_speedcombo->currentIndex() );

    config.setValue( "ConnectionTimeout", m_connecttimeout->text() );
    config.setValue( "ServerTimeout", m_servertimeout->text() );
}

void MediaPlayerSettingsDialog::applySettings()
{
    QMediaHelixSettingsControl settings;

    settings.setOption( "Bandwidth",
        m_speedcombo->itemData( m_speedcombo->currentIndex() ) );

    settings.setOption( "ConnectionTimeOut", m_connecttimeout->text() );
    settings.setOption( "ServerTimeOut", m_servertimeout->text() );
}
#endif

class ThumbnailWidget : public QWidget
{
    Q_OBJECT
public:
    ThumbnailWidget( PlayerControl* control, QWidget* parent = 0 );

    void setPlaylist( const QMediaPlaylist &playlist );

    QSize sizeHint() const { return m_thumb.size(); }

protected:
    // Load thumbnail and draw onto widget
    void paintEvent( QPaintEvent* );
    void resizeEvent( QResizeEvent * ) { updatePlaylistThumbnail(); }

private slots:
    void hideThumbnail();
    void showThumbnail();
    void updatePlaylistThumbnail();
    void findInfo(QMediaContent* content);
    void metaInfoChanged(QString const& value);

private:
    QMediaPlaylist m_playlist;

    QPixmap m_thumb;
    QPixmap m_scaledThumb;
    QPoint m_thumbpos;

    bool showingThumbnail;
    bool showingHelixLogo;
    bool isHelixLogo;
    PlayerControl *m_control;
    QMediaControlNotifier *m_notifier;
    QMediaMetaInfoControl* m_metaInfo;
};

ThumbnailWidget::ThumbnailWidget( PlayerControl* control, QWidget* parent )
    : QWidget(parent)
        , showingThumbnail(true)
        , showingHelixLogo(false)
        , isHelixLogo(false)
        , m_control(control)
{
    m_notifier = new QMediaControlNotifier( QMediaVideoControl::name(), this );
    connect( m_notifier, SIGNAL(valid()), this, SLOT(hideThumbnail()) );
    connect( m_notifier, SIGNAL(invalid()), this, SLOT(showThumbnail()) );

    connect( m_control, SIGNAL(contentChanged(QMediaContent*)), m_notifier, SLOT(setMediaContent(QMediaContent*)));
    connect( &m_playlist, SIGNAL(playingChanged(QModelIndex)), this, SLOT(updatePlaylistThumbnail()) );

    connect(m_control, SIGNAL(contentChanged(QMediaContent*)), SLOT(findInfo(QMediaContent*)));
}

void ThumbnailWidget::hideThumbnail()
{
    showingThumbnail = false;
    updatePlaylistThumbnail();
}

void ThumbnailWidget::showThumbnail()
{
    showingThumbnail = true;
    updatePlaylistThumbnail();
}

void ThumbnailWidget::findInfo(QMediaContent* content)
{
    if (content != 0) {
        m_metaInfo = new QMediaMetaInfoControl(content);
        connect(m_metaInfo, SIGNAL(valueChanged(QString)), SLOT(metaInfoChanged(QString)));
    }
}

void ThumbnailWidget::metaInfoChanged(QString const& value)
{
    if (value == "engine" &&
        m_metaInfo->value("engine") == "Helix" &&
        !showingHelixLogo) {

        showingHelixLogo = true;
        update();
    }
}

void ThumbnailWidget::paintEvent(QPaintEvent*)
{
    if (!showingThumbnail || m_scaledThumb.isNull() || (isHelixLogo && !showingHelixLogo))
        return;

    QPainter painter(this);

    m_thumbpos = QPoint((width() - m_scaledThumb.width()) / 2, (height() - m_scaledThumb.height()) / 2);

    if (!isHelixLogo)
        qDrawShadeRect(&painter, m_thumbpos.x()-1, m_thumbpos.y()-1, m_scaledThumb.width()+2, m_scaledThumb.height()+2, style()->standardPalette());

    painter.drawPixmap(m_thumbpos, m_scaledThumb);
}

void ThumbnailWidget::setPlaylist( const QMediaPlaylist &playlist )
{
    m_playlist = playlist;

    updatePlaylistThumbnail();
}

void ThumbnailWidget::updatePlaylistThumbnail()
{
    isHelixLogo = false;
    showingHelixLogo = false;
    if(showingThumbnail)
    {
        QModelIndex playing = m_playlist.playing();
        m_thumb = m_playlist.data( playing, QMediaPlaylist::AlbumCover ).value<QPixmap>();
#ifndef NO_HELIX
        if(m_thumb.isNull()) {
            m_thumb = QPixmap(HELIX_LOGO_PATH);
            isHelixLogo = true;
        }
#endif
    }
    else
    {
#ifndef NO_HELIX
        isHelixLogo = true;
        m_thumb = QPixmap(HELIX_LOGO_PATH);
#else
        m_thumb = QPixmap();
#endif
    }

    if(!m_thumb.isNull() && (m_thumb.width() > width()-2 || m_thumb.height() > height()-2))
        m_scaledThumb = m_thumb.scaled(QSize(width()-2, height()-2), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    else
        m_scaledThumb = m_thumb;

    updateGeometry();
    update();
}

static QImage load_scaled_image( const QString& filename, const QSize& size, Qt::AspectRatioMode mode = Qt::IgnoreAspectRatio )
{
    QImageReader reader( filename );

    QSize scaled;
    if( mode == Qt::IgnoreAspectRatio ) {
        scaled = size;
    } else {
        scaled = reader.size();
        scaled.scale( size, mode );
    }

    reader.setQuality( 49 ); // Otherwise Qt smooth scales
    reader.setScaledSize( scaled );
    return reader.read();
}

static QImage invert( const QImage& image )
{
    QImage ret;

    switch( image.format() )
    {
    case QImage::Format_RGB32:
    case QImage::Format_ARGB32:
        ret = image;
        break;
    case QImage::Format_ARGB32_Premultiplied:
        ret = image.convertToFormat( QImage::Format_ARGB32 );
        break;
    default:
        // Unsupported
        return image;
    }

    quint32 *p = (quint32*)ret.bits();
    quint32 *end = p + ret.numBytes()/4;
    while( p != end ) {
        *p++ ^= 0x00ffffff;
    }

    return ret;
}

static QImage contrast( const QColor& color, const QImage& image )
{
    static const int INVERT_THRESHOLD = 128;

    int x, v;
    color.getHsv( &x, &x, &v );
    if( v > INVERT_THRESHOLD ) {
        return invert( image );
    }

    return image;
}

class IconWidget : public QWidget
{
public:
    IconWidget( QWidget* parent = 0 )
        : QWidget( parent )
    { }

    void setFile( const QString& file );

    // QWidget
    QSize sizeHint() const;

protected:
    void reizeEvent( QResizeEvent* ) { m_buffer = QImage(); }

    void paintEvent( QPaintEvent* );
private:
    QString m_filename;

    QImage m_buffer;
};

void IconWidget::setFile( const QString& file )
{
    m_filename = file;

    m_buffer = QImage();
    update();
}

QSize IconWidget::sizeHint() const
{
    int height = QtopiaApplication::style()->pixelMetric(QStyle::PM_SmallIconSize);

    return QSize( height, height );
}

void IconWidget::paintEvent( QPaintEvent* )
{
    if( m_buffer.isNull() ) {
        m_buffer = contrast( palette().windowText().color(), load_scaled_image( m_filename, size() ) );
    }

    QPainter painter( this );
    painter.drawImage( QPoint( 0, 0 ), m_buffer );
}

class PlaylistLabel : public QWidget
{
    Q_OBJECT
public:
    PlaylistLabel( QWidget* parent = 0 );

    void setPlaylist( const QMediaPlaylist &playlist );

private slots:
    void updateLabel();

private:
    QMediaPlaylist m_playlist;

    QLabel *m_label;
    IconWidget *m_myshuffleicon;
};

PlaylistLabel::PlaylistLabel( QWidget* parent )
    : QWidget( parent ), m_myshuffleicon( 0 )
{
    connect( &m_playlist, SIGNAL(playingChanged(QModelIndex)),
        this, SLOT(updateLabel()) );
    connect( &m_playlist, SIGNAL(rowsInserted(QModelIndex,int,int)),
        this, SLOT(updateLabel()) );
    connect( &m_playlist, SIGNAL(rowsRemoved(QModelIndex,int,int)),
        this, SLOT(updateLabel()) );

    m_label = new QLabel ( tr( "- of -", "song '- of -'") );

    QVBoxLayout *layout = new QVBoxLayout;
    layout->setMargin( 0 );

    layout->addWidget( m_label );

    setLayout( layout );
}

void PlaylistLabel::setPlaylist( const QMediaPlaylist &playlist )
{
    m_playlist = playlist;

    if( playlist.isShuffle() ) {
        m_label->hide();
        if(m_myshuffleicon == NULL)
        {
            m_myshuffleicon = new IconWidget;
            m_myshuffleicon->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed );
            m_myshuffleicon->setMinimumSize( QtopiaApplication::style()->pixelMetric(QStyle::PM_SmallIconSize), QtopiaApplication::style()->pixelMetric(QStyle::PM_SmallIconSize) );
            m_myshuffleicon->setFile( ":image/mediaplayer/black/shuffle" );
            layout()->addWidget( m_myshuffleicon );
        }
        m_myshuffleicon->show();
    } else {

        updateLabel();

        m_label->show();
        if(m_myshuffleicon != NULL)
            m_myshuffleicon->hide();
    }
}

void PlaylistLabel::updateLabel()
{
    int count = m_playlist.rowCount();

    if( count ) {
        QModelIndex playing = m_playlist.playing();
        if( playing.isValid() ) {
            m_label->setText( tr( "%1 of %2", " e.g. '4 of 6' as in song 4 of 6" ).arg( playing.row() + 1 ).arg( count ) );
        } else {
            m_label->setText( tr( "- of %2", "- of 8" ).arg( count ) );
        }
    } else {
        m_label->setText( tr( "- of -", "song '- of -'" ) );
    }
}

class RepeatState : public QObject
{
    Q_OBJECT
public:
    RepeatState( QObject* parent = 0 )
        : QObject( parent ), m_state( RepeatNone )
    { }

    enum State { RepeatOne, RepeatAll, RepeatNone };

    State state() const { return m_state; }
    void setState( State state );

signals:
    void stateChanged( RepeatState::State state );

private:
    State m_state;
};

void RepeatState::setState( State state )
{
    m_state = state;

    emit stateChanged( state );
}

class ProgressView : public QWidget
{
    Q_OBJECT
public:
    ProgressView( RepeatState* repeatstate, QWidget* parent = 0 );

    void setPlaylist( const QMediaPlaylist &playlist );

    QWidget* keyEventHandler() const { return m_progress; }

public slots:
    void setMediaContent( QMediaContent* content );

private slots:
    void repeatStateChanged( RepeatState::State state );
    void init();

private:
    QMediaContentContext *m_context;
    QMediaProgressWidget *m_progress;
    PlaylistLabel *m_playlistlabel;
    RepeatState *m_repeatstate;
    IconWidget *m_repeaticon;
};

ProgressView::ProgressView( RepeatState* repeatstate, QWidget* parent )
    : QWidget( parent )
    , m_context( 0 )
    , m_progress( 0 )
    , m_playlistlabel( 0 )
    , m_repeatstate( repeatstate )
    , m_repeaticon( 0 )
{
    QTimer::singleShot(1, this, SLOT(init()));
}

void ProgressView::repeatStateChanged( RepeatState::State state )
{
    if(!m_context)
        init();
    switch( state )
    {
    case RepeatState::RepeatOne:
        m_repeaticon->show();
        m_playlistlabel->hide();
        break;
    case RepeatState::RepeatAll:
        m_repeaticon->show();
        m_playlistlabel->show();
        break;
    case RepeatState::RepeatNone:
        m_repeaticon->hide();
        m_playlistlabel->show();
        break;
    }
}

void ProgressView::setMediaContent( QMediaContent* content )
{
    if(!m_context)
        init();
    m_context->setMediaContent( content );
}

void ProgressView::setPlaylist( const QMediaPlaylist &playlist )
{
    if(!m_context)
        init();
    m_playlistlabel->setPlaylist( playlist );
}

void ProgressView::init()
{
    if(m_context)
        return;
    // Connect to repeat state
    connect( m_repeatstate, SIGNAL(stateChanged(RepeatState::State)),
        this, SLOT(repeatStateChanged(RepeatState::State)) );

    QVBoxLayout *layout = new QVBoxLayout;
    layout->setMargin( 0 );

    m_progress = new QMediaProgressWidget;
    layout->addWidget( m_progress );

    QHBoxLayout *hbox = new QHBoxLayout;

    m_playlistlabel = new PlaylistLabel;
    hbox->addWidget( m_playlistlabel );

    m_repeaticon = new IconWidget;
    m_repeaticon->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed );
    m_repeaticon->setMinimumSize( QtopiaApplication::style()->pixelMetric(QStyle::PM_SmallIconSize), QtopiaApplication::style()->pixelMetric(QStyle::PM_SmallIconSize) );
    m_repeaticon->setFile( ":image/mediaplayer/black/repeat" );
    hbox->addWidget( m_repeaticon );

    hbox->addStretch();

    QMediaProgressLabel *progresslabel = new QMediaProgressLabel( QMediaProgressLabel::ElapsedTotalTime );
    hbox->addWidget( progresslabel );

    layout->addLayout( hbox );
    setLayout( layout );

    m_context = new QMediaContentContext( this );
    m_context->addObject( m_progress );
    m_context->addObject( progresslabel );

    // Initialize view
    repeatStateChanged( m_repeatstate->state() );
}

class VolumeView : public QWidget
{
    Q_OBJECT
public:
    VolumeView( QWidget* parent = 0 );

    QWidget* keyEventHandler() const { return m_volume; }

public slots:
    void setMediaContent( QMediaContent* content );

private:
    QMediaVolumeWidget *m_volume;
};

VolumeView::VolumeView( QWidget* parent )
    : QWidget( parent )
{
    QVBoxLayout *layout = new QVBoxLayout;
    layout->setMargin( 0 );

    m_volume = new QMediaVolumeWidget;
    layout->addWidget( m_volume );

    QHBoxLayout *hbox = new QHBoxLayout;
    hbox->addWidget( new QMediaVolumeLabel( QMediaVolumeLabel::MinimumVolume ) );
    hbox->addStretch();
    hbox->addWidget( new QMediaVolumeLabel( QMediaVolumeLabel::MaximumVolume ) );

    layout->addLayout( hbox );
    setLayout( layout );
}

void VolumeView::setMediaContent( QMediaContent* content )
{
    m_volume->setMediaContent( content );
}

class SeekView : public QWidget
{
    Q_OBJECT
public:
    SeekView( QWidget* parent = 0 );

    QWidget* keyEventHandler() const { return m_seekwidget; }

public slots:
    void setMediaContent( QMediaContent* content );

private:
    QMediaSeekWidget *m_seekwidget;
};

SeekView::SeekView( QWidget* parent )
    : QWidget( parent )
{
    QVBoxLayout *layout = new QVBoxLayout;
    layout->setMargin( 0 );

    m_seekwidget = new QMediaSeekWidget;
    layout->addWidget( m_seekwidget );

    QHBoxLayout *hbox = new QHBoxLayout;

    QMediaProgressLabel *elapsed = new QMediaProgressLabel( QMediaProgressLabel::ElapsedTime );
    connect( m_seekwidget, SIGNAL(lengthChanged(quint32)),
        elapsed, SLOT(setTotal(quint32)) );
    connect( m_seekwidget, SIGNAL(positionChanged(quint32)),
        elapsed, SLOT(setElapsed(quint32)) );
    hbox->addWidget( elapsed );
    hbox->addStretch();

    QMediaProgressLabel *remaining = new QMediaProgressLabel( QMediaProgressLabel::RemainingTime );
    connect( m_seekwidget, SIGNAL(lengthChanged(quint32)),
        remaining, SLOT(setTotal(quint32)) );
    connect( m_seekwidget, SIGNAL(positionChanged(quint32)),
        remaining, SLOT(setElapsed(quint32)) );
    hbox->addWidget( remaining );

    layout->addLayout( hbox );
    setLayout( layout );
}

void SeekView::setMediaContent( QMediaContent* content )
{
    m_seekwidget->setMediaContent( content );
}

class WhatsThisLabel : public QLabel
{
public:
    WhatsThisLabel( QWidget* parent = 0 )
        : QLabel( parent )
    { }

    void addWidgetToMonitor( QWidget* widget );

    // QObject
    bool eventFilter( QObject* o, QEvent* e );
};

void WhatsThisLabel::addWidgetToMonitor( QWidget* widget )
{
    widget->installEventFilter( this );
}

bool WhatsThisLabel::eventFilter( QObject* o, QEvent* e )
{
    if( e->type() == QEvent::FocusIn ) {
        QWidget *widget = qobject_cast<QWidget*>(o);
        if( widget ) {
            setText( widget->whatsThis() );
        }
    }

    return false;
}

class PlayerDialog : public QDialog
{
public:
    PlayerDialog( QWidget* parent, Qt::WindowFlags f );
};

PlayerDialog::PlayerDialog( QWidget* parent, Qt::WindowFlags f )
    : QDialog( parent, f )
{
    static const int BACKGROUND_ALPHA = 190;

    // Use custom palette
    QPalette pal = palette();
    QColor button = pal.button().color();
    button.setAlpha( BACKGROUND_ALPHA );
    pal.setColor( QPalette::Window, button );
    setPalette( pal );

    // Remove title bar from dialog
    setWindowFlags( windowFlags() | Qt::FramelessWindowHint );
}

class ToolButtonStyle : public QWindowsStyle
{
public:
    void drawComplexControl( ComplexControl cc, const QStyleOptionComplex *opt,
        QPainter *p, const QWidget *widget ) const;

private:
    mutable QPixmap m_buttonbuffer;
    mutable QPixmap m_focusedbuttonbuffer;
};

void ToolButtonStyle::drawComplexControl( ComplexControl cc, const QStyleOptionComplex *opt,
    QPainter *p, const QWidget *widget ) const
{
    switch( cc )
    {
    // Polished tool button
    case CC_ToolButton:
        if (const QStyleOptionToolButton *toolbutton
            = qstyleoption_cast<const QStyleOptionToolButton *>(opt)) {
            QRect button, menuarea;
            button = subControlRect(cc, toolbutton, SC_ToolButton, widget);

            QPixmap *icon;

            if( toolbutton->state & State_HasFocus ) {
                if( !m_focusedbuttonbuffer.isNull() || m_focusedbuttonbuffer.size() != button.size() ) {
                    m_focusedbuttonbuffer = QIcon( ":icon/mediaplayer/black/vote-button-focused" ).pixmap( button.size() );
                }
                    icon = &m_focusedbuttonbuffer;
            } else {
                if( !m_buttonbuffer.isNull() || m_buttonbuffer.size() != button.size() ) {
                    m_buttonbuffer = QIcon( ":icon/mediaplayer/black/vote-button" ).pixmap( button.size() );
                }

                icon = &m_buttonbuffer;
            }

            p->drawPixmap( button, *icon );

            QStyleOptionToolButton label = *toolbutton;
            int fw = pixelMetric(PM_DefaultFrameWidth, opt, widget);
            label.rect = button.adjusted(fw, fw, -fw, -fw);

            drawControl(CE_ToolButtonLabel, &label, p, widget);
        }
        break;
    default:
        QWindowsStyle::drawComplexControl( cc, opt, p, widget );
    }
}

class ToolButtonDialog : public PlayerDialog
{
public:
    ToolButtonDialog( QWidget* parent, Qt::WindowFlags f );
    ~ToolButtonDialog();

protected:
    QToolButton* addToolButton( const QIcon& icon, const QString& whatsthis );
    bool eventFilter( QObject *object, QEvent *event );

private:
    ToolButtonStyle *m_style;
    QHBoxLayout *m_layout;
    WhatsThisLabel *m_label;
    int m_iconsize;
    int m_buttonsize;
};

ToolButtonDialog::ToolButtonDialog( QWidget* parent, Qt::WindowFlags f )
    : PlayerDialog( parent, f )
{
    m_style = new ToolButtonStyle;

    m_iconsize = QtopiaApplication::style()->pixelMetric(QStyle::PM_SmallIconSize) + 4;
    m_buttonsize = m_iconsize + 12;

    m_label = new WhatsThisLabel( this );
    m_label->setAlignment( Qt::AlignHCenter );

    QFont smallfont = font();
    smallfont.setPointSize( smallfont.pointSize() - 2 );
    smallfont.setItalic( true );
    smallfont.setBold( true );

    m_label->setFont( smallfont );

    m_layout = new QHBoxLayout;
    m_layout->setSpacing( 18 );

    QVBoxLayout *vbox = new QVBoxLayout;
    vbox->setMargin( 6 );
    vbox->addLayout( m_layout );
    vbox->addWidget( m_label );

    setLayout( vbox );
}

QToolButton* ToolButtonDialog::addToolButton( const QIcon& icon, const QString& whatsthis )
{
    QToolButton *button = new QToolButton;
    button->setIconSize( QSize( m_iconsize, m_iconsize ) );
    button->setMinimumSize( m_buttonsize, m_buttonsize );
    button->setStyle( m_style );

    button->setIcon( icon );
    button->setWhatsThis( whatsthis );

    button->installEventFilter( this );

    m_label->addWidgetToMonitor( button );
    m_layout->addWidget( button );

    return button;
}

bool ToolButtonDialog::eventFilter( QObject *object, QEvent *event )
{
    if( event->type() == QEvent::KeyPress && QApplication::isRightToLeft() )
    {
        QKeyEvent *keyEvent = static_cast< QKeyEvent * >( event );

        switch( keyEvent->key() )
        {
        case Qt::Key_Left:
            return object->event( new QKeyEvent( QEvent::KeyPress, Qt::Key_Tab, keyEvent->modifiers() ) );
        case Qt::Key_Right:
            return object->event( new QKeyEvent( QEvent::KeyPress, Qt::Key_Backtab, keyEvent->modifiers() ) );
        }
    }

    return PlayerDialog::eventFilter( object, event );
}

ToolButtonDialog::~ToolButtonDialog()
{
    delete m_style;
}

class VoteDialog : public ToolButtonDialog
{
    Q_OBJECT
public:
    VoteDialog( QWidget* parent = 0, Qt::WindowFlags f = 0 );

signals:
    void favoriteVoted();
    void snoozeVoted();
    void banVoted();

private slots:
    void voteFavorite();
    void voteSnooze();
    void voteBan();
};

VoteDialog::VoteDialog( QWidget* parent, Qt::WindowFlags f )
    : ToolButtonDialog( parent, f )
{
    QToolButton *button;

    button = addToolButton( QIcon( ":icon/mediaplayer/black/favorite" ),
        tr( "Play this one more often" ) );
    connect( button, SIGNAL(clicked()), this, SLOT(voteFavorite()) );

    button = addToolButton( QIcon( ":icon/mediaplayer/black/snooze" ),
        tr( "Getting tired of this one" ) );
    connect( button, SIGNAL(clicked()), this, SLOT(voteSnooze()) );

    button = addToolButton( QIcon( ":icon/mediaplayer/black/ban" ),
        tr( "Never play this one again" ) );
    connect( button, SIGNAL(clicked()), this, SLOT(voteBan()) );
}

void VoteDialog::voteFavorite()
{
    if( MediaPlayer::instance()->playlist().isShuffle() ) {
        const_cast<QMediaPlaylist&>(MediaPlayer::instance()->playlist()).setProbability( MediaPlayer::instance()->playlist().playing(), QMediaPlaylist::Frequent );

        emit favoriteVoted();
    }

    accept();
}

void VoteDialog::voteSnooze()
{
    if( MediaPlayer::instance()->playlist().isShuffle() ) {
        const_cast<QMediaPlaylist&>(MediaPlayer::instance()->playlist()).setProbability( MediaPlayer::instance()->playlist().playing(), QMediaPlaylist::Infrequent );

        emit snoozeVoted();
    }

    accept();
}

void VoteDialog::voteBan()
{
    if( MediaPlayer::instance()->playlist().isShuffle() ) {
        const_cast<QMediaPlaylist&>(MediaPlayer::instance()->playlist()).setProbability( MediaPlayer::instance()->playlist().playing(), QMediaPlaylist::Never );

        emit banVoted();
    }

    accept();
}

class SkipDialog : public ToolButtonDialog
{
    Q_OBJECT
public:
    SkipDialog( QWidget* parent = 0, Qt::WindowFlags f = 0 );

    QTimer*       timer;

private slots:
    void next();
    void previous();
    void play();
    void stop();
    void volumeUp();
    void volumeDown();
    void timeout();

public slots:
    void setPlayIcon();
    void setPauseIcon();

protected:
    // QWidget
    void keyPressEvent( QKeyEvent* e );
private:
    PlayerWidget* p;
    QToolButton*  nxt;
    QToolButton*  prv;
    QToolButton*  stp;
    QToolButton*  ply;
    QToolButton*  volup;
    QToolButton*  voldn;

    QIcon playIcon;
    QIcon pauseIcon;
};

SkipDialog::SkipDialog( QWidget* parent, Qt::WindowFlags f )
    : ToolButtonDialog( parent, f )
{
    p = qobject_cast<PlayerWidget*>(parent);

    voldn = addToolButton( QIcon( ":icon/mediaplayer/black/speaker-minus" ),
        tr( "Decrease volume" ) );
    connect( voldn, SIGNAL(clicked()), this, SLOT(volumeDown()) );
    voldn->setFocusPolicy(Qt::NoFocus);

    prv = addToolButton( QIcon( ":icon/mediaplayer/black/skip-prev" ),
        tr( "Skip to previous track") );
    connect( prv, SIGNAL(clicked()), this, SLOT(previous()) );
    prv->setFocusPolicy(Qt::NoFocus);

    playIcon = QIcon( ":icon/mediaplayer/black/touch-play" );
    pauseIcon = QIcon( ":icon/mediaplayer/black/touch-pause" );

    ply = addToolButton( pauseIcon, tr( "Play track") );
    connect( ply, SIGNAL(clicked()), this, SLOT(play()) );
    ply->setFocusPolicy(Qt::NoFocus);

    stp = addToolButton( QIcon( ":icon/mediaplayer/black/touch-stop" ),
        tr( "Stop track") );
    connect( stp, SIGNAL(clicked()), this, SLOT(stop()) );
    stp->setFocusPolicy(Qt::NoFocus);

    nxt = addToolButton( QIcon( ":icon/mediaplayer/black/skip-next" ),
        tr( "Skip to next track" ) );
    connect( nxt, SIGNAL(clicked()), this, SLOT(next()) );
    nxt->setFocusPolicy(Qt::NoFocus);

    volup = addToolButton( QIcon( ":icon/mediaplayer/black/speaker-plus" ),
        tr( "Increase volume" ) );
    connect( volup, SIGNAL(clicked()), this, SLOT(volumeUp()) );
    volup->setFocusPolicy(Qt::NoFocus);

    timer = new QTimer(this);
    connect(timer,SIGNAL(timeout()), this, SLOT(timeout()));

    connect(p->m_statewidget, SIGNAL(playing()), this, SLOT(setPauseIcon()));
    connect(p->m_statewidget, SIGNAL(paused()), this, SLOT(setPlayIcon()));
    connect(p->m_statewidget, SIGNAL(stopped()), this, SLOT(setPlayIcon()));
}

void SkipDialog::keyPressEvent( QKeyEvent* e )
{
    switch( e->key() )
    {
    case Qt::Key_1:
        e->accept();
        previous();
        break;
    default:
        ToolButtonDialog::keyPressEvent( e );
        break;
    }
}

void SkipDialog::previous()
{
    timer->start(3000);
    if( p->m_mediacontrol && p->m_mediacontrol->position() > 3000 ) {
        p->m_mediacontrol->seek( 0 );
    } else {
            p->m_playlist.playPrevious();
    }
}

void SkipDialog::play()
{
    if (p->m_mediacontrol) {
        timer->start(3000);
        if( p->m_mediacontrol->playerState() == QtopiaMedia::Paused ||
                p->m_mediacontrol->playerState() == QtopiaMedia::Stopped) {
            p->m_mediacontrol->start();
            p->changeState(QtopiaMedia::Playing);
            p->m_playercontrol->setState( PlayerControl::Playing );
            setPauseIcon();
        } else {
            p->m_mediacontrol->pause();
            p->changeState(QtopiaMedia::Paused);
            p->m_playercontrol->setState( PlayerControl::Paused );
            setPlayIcon();
        }
    }
}

void SkipDialog::stop()
{
    if (p->m_mediacontrol) {
        timer->start(3000);
        p->m_mediacontrol->stop();
        p->m_playercontrol->setState( PlayerControl::Stopped );
        p->changeState(QtopiaMedia::Stopped);
        setPlayIcon();
    }
}

void SkipDialog::next()
{
    timer->start(3000);
    QModelIndex index = p->m_playlist.nextIndex();
    if( index.isValid() ) {
        p->m_playlist.setPlaying( index );
    } else {
        // If repeat state is repeat all, skip to beginning
        if( p->m_repeatstate->state() == RepeatState::RepeatAll ) {
            p->m_playlist.playFirst();
        }
    }
}

void SkipDialog::volumeUp()
{
    if( p->m_mediacontrol ) {
        timer->start(3000);
        int v = p->m_mediacontrol->volume();
        if(v <= 90) v+=10;
        else v=100;
        p->m_mediacontrol->setVolume(v);
    }
}

void SkipDialog::volumeDown()
{
    if( p->m_mediacontrol ) {
        timer->start(3000);
        int v = p->m_mediacontrol->volume();
        if(v >= 10) v-=10;
        else v=0;
        p->m_mediacontrol->setVolume(v);
    }
}

void SkipDialog::timeout()
{
    timer->stop();
    hide();
}

void SkipDialog::setPauseIcon()
{
    ply->setIcon( pauseIcon );
}

void SkipDialog::setPlayIcon()
{
    ply->setIcon( playIcon );
}

class RepeatDialog : public ToolButtonDialog
{
    Q_OBJECT
public:
    RepeatDialog( RepeatState* repeatstate, QWidget* parent = 0, Qt::WindowFlags f = 0 );

private slots:
    void repeatOne();
    void repeatAll();
    void repeatNone();

protected:
    // QWidget
    void keyPressEvent( QKeyEvent* e );

private:
    RepeatState *m_repeatstate;
};

RepeatDialog::RepeatDialog( RepeatState* repeatstate, QWidget* parent, Qt::WindowFlags f )
    : ToolButtonDialog( parent, f ), m_repeatstate( repeatstate )
{
    QToolButton *button;

    button = addToolButton( QIcon( ":icon/mediaplayer/black/repeat-one" ),
        tr( "Repeat this one only","Repeat this track only" ) );
    connect( button, SIGNAL(clicked()), this, SLOT(repeatOne()) );

    button = addToolButton( QIcon( ":icon/mediaplayer/black/repeat-all" ),
        tr( "Repeat entire playlist" ) );
    connect( button, SIGNAL(clicked()), this, SLOT(repeatAll()) );

    button = addToolButton( QIcon( ":icon/mediaplayer/black/repeat-none" ),
        tr( "Don't repeat anything" ) );
    connect( button, SIGNAL(clicked()), this, SLOT(repeatNone()) );
}

void RepeatDialog::keyPressEvent( QKeyEvent* e )
{
    switch( e->key() )
    {
    case Qt::Key_1:
        e->accept();
        repeatOne();
        break;
    case Qt::Key_Asterisk:
        e->accept();
        repeatAll();
        break;
    case Qt::Key_0:
        e->accept();
        repeatNone();
        break;
    default:
        ToolButtonDialog::keyPressEvent( e );
        break;
    }
}

void RepeatDialog::repeatOne()
{
    m_repeatstate->setState( RepeatState::RepeatOne );
    accept();
}

void RepeatDialog::repeatAll()
{
    m_repeatstate->setState( RepeatState::RepeatAll );
    accept();
}

void RepeatDialog::repeatNone()
{
    m_repeatstate->setState( RepeatState::RepeatNone );
    accept();
}

class TrackInfoWidget : public QWidget
{
    Q_OBJECT
public:
    TrackInfoWidget( PlayerControl* control, QWidget* parent = 0 );
    ~TrackInfoWidget();

    void setPlaylist( const QMediaPlaylist &playlist );

private slots:
    void updateInfo();
    void hideExtraLines();
    void showExtraLines();
    void rotateFinished();
    void brightnessChanged(int);

private:

    virtual void hideEvent ( QHideEvent * );
    //virtual void resizeEvent ( QResizeEvent * ) { updateInfo(); };
    virtual void showEvent ( QShowEvent * ) { updateInfo(); };

    void stopRotating();

    QMediaPlaylist m_playlist;
    QMarqueeLabel *m_track;
    QMarqueeLabel *m_album;
    QMarqueeLabel *m_artist;
    bool showingExtraLines;

    PlayerControl *m_control;
    QMediaControlNotifier *m_notifier;
    QtopiaIpcAdaptor *ipcAdaptor;
};

TrackInfoWidget::TrackInfoWidget( PlayerControl* control, QWidget* parent )
    : QWidget( parent )
        ,showingExtraLines(true)
        ,m_control(control)
{
    m_track = new QMarqueeLabel;
    m_track->setAlignment( Qt::AlignRight );
    m_album = new QMarqueeLabel;
    m_album->setAlignment( Qt::AlignRight );
    m_artist = new QMarqueeLabel;
    m_artist->setAlignment( Qt::AlignRight );

    QVBoxLayout *layout = new QVBoxLayout;
    layout->setMargin( 0 );

    layout->addWidget( m_track );
    layout->addWidget( m_album );
    layout->addWidget( m_artist );
    layout->addStretch();
    setLayout( layout );

    m_notifier = new QMediaControlNotifier( QMediaVideoControl::name(), this );
    connect( m_notifier, SIGNAL(valid()), this, SLOT(hideExtraLines()) );
    connect( m_notifier, SIGNAL(invalid()), this, SLOT(showExtraLines()) );
    connect( m_control, SIGNAL(contentChanged(QMediaContent*)), m_notifier, SLOT(setMediaContent(QMediaContent*)));
    connect( m_track, SIGNAL(rotateFinished()), this, SLOT(rotateFinished()) );
    connect( m_album, SIGNAL(rotateFinished()), this, SLOT(rotateFinished()) );
    connect( m_artist, SIGNAL(rotateFinished()), this, SLOT(rotateFinished()) );
    connect( &m_playlist, SIGNAL(playingChanged(QModelIndex)),
        this, SLOT(updateInfo()) );

    ipcAdaptor = new QtopiaIpcAdaptor("Qtopia/PowerStatus");
    QtopiaIpcAdaptor::connect(ipcAdaptor, MESSAGE(brightnessChanged(int)), this, SLOT(brightnessChanged(int)));
}

TrackInfoWidget::~TrackInfoWidget()
{
    delete ipcAdaptor;
}

void TrackInfoWidget::setPlaylist( const QMediaPlaylist &playlist )
{
    m_playlist = playlist;

    updateInfo();
}

void TrackInfoWidget::updateInfo()
{
    QModelIndex playing = m_playlist.playing();
    m_track->setText( m_playlist.data( playing, QMediaPlaylist::Title ).toString() );
    if(m_track->text().isEmpty())
        m_track->hide();
    else
        m_track->show();

    m_album->setText(m_playlist.data( playing, QMediaPlaylist::Album ).toString());
    if(!m_album->text().isEmpty() && showingExtraLines)
        m_album->show();
    else
        m_album->hide();

    m_artist->setText(m_playlist.data( playing, QMediaPlaylist::Artist ).toString());
    if(!m_artist->text().isEmpty() && showingExtraLines)
        m_artist->show();
    else
        m_artist->hide();

    stopRotating();
    QMarqueeLabel::startRotating(m_track, m_album, m_artist);

    update();
}

void TrackInfoWidget::hideExtraLines()
{
    showingExtraLines = false;
    updateInfo();
}

void TrackInfoWidget::showExtraLines()
{
    showingExtraLines = true;
    updateInfo();
}

void TrackInfoWidget::rotateFinished()
{
    if(sender()==NULL || sender() == m_artist)
        QMarqueeLabel::startRotating(m_track, m_album, m_artist);
    else if(sender() == m_track)
        QMarqueeLabel::startRotating(m_album, m_artist, m_track);
    else if(sender() == m_album)
        QMarqueeLabel::startRotating(m_artist, m_track, m_album);
}

void TrackInfoWidget::hideEvent( QHideEvent * )
{
    stopRotating();
}

void TrackInfoWidget::stopRotating()
{
    m_track->stopRotate();
    m_album->stopRotate();
    m_artist->stopRotate();
}

void TrackInfoWidget::brightnessChanged(int value)
{
    if(value >=0 && value <=1)
        stopRotating();
    else if(!m_track->isRotating() && !m_album->isRotating() && !m_artist->isRotating())
        QMarqueeLabel::startRotating(m_track, m_album, m_artist);
}

class ThrottleWidget : public QWidget
{
    Q_OBJECT
public:
    ThrottleWidget( QWidget* parent = 0 );

    void setOpacity( qreal opacity );

    // Set resolution between 0.0 and 1.0, default 0.1
    void setResolution( qreal resolution ) { m_resolution = resolution; }

    // Return current intensity between -1.0 and 1.0
    qreal intensity() const { return m_intensity; }

    QSize sizeHint() const { return QSize( QtopiaApplication::style()->pixelMetric(QStyle::PM_SmallIconSize), QtopiaApplication::style()->pixelMetric(QStyle::PM_SmallIconSize) ); }

signals:
    void pressed();
    void released();

    void intensityChanged( qreal intensity );

protected:
    void mousePressEvent( QMouseEvent* e );
    void mouseReleaseEvent( QMouseEvent* e );
    void mouseMoveEvent( QMouseEvent* e );

    void resizeEvent( QResizeEvent* e );
    void paintEvent( QPaintEvent* e );

private:
    qreal calculateIntensity( const QPoint& point );

    qreal m_intensity;
    qreal m_resolution;
    qreal m_opacity;

    QImage m_control;
    QPoint m_controlpos;
};

ThrottleWidget::ThrottleWidget( QWidget* parent )
    : QWidget( parent ), m_intensity( 0.0 ), m_resolution( 0.1 ), m_opacity( 1.0 )
{ }

void ThrottleWidget::setOpacity( qreal opacity )
{
    m_opacity = opacity;

    update();
}

void ThrottleWidget::mousePressEvent( QMouseEvent* e )
{
    m_intensity = calculateIntensity( e->pos() );

    emit pressed();
}

void ThrottleWidget::mouseReleaseEvent( QMouseEvent* )
{
    m_intensity = 0.0;

    emit released();
}

void ThrottleWidget::mouseMoveEvent( QMouseEvent* e )
{
    qreal intensity = calculateIntensity( e->pos() );
    qreal delta = m_intensity - intensity;
    if( delta < 0 ) {
        delta = -delta;
    }

    if( delta >= m_resolution  ) {
        m_intensity = intensity;

        emit intensityChanged( m_intensity );
    }
}

void ThrottleWidget::resizeEvent( QResizeEvent* )
{
    m_control = QImage();
}

void ThrottleWidget::paintEvent( QPaintEvent* )
{
    static const QString THROTTLE_CONTROL = ":image/mediaplayer/black/throttle";

    if( m_control.isNull() ) {
        QImageReader reader( THROTTLE_CONTROL );
        QSize scaled = reader.size();
        scaled.scale( size(), Qt::KeepAspectRatio );
        reader.setQuality( 49 ); // Otherwise Qt smooth scales
        reader.setScaledSize( scaled );
        m_control = reader.read();
        m_controlpos = QPoint( (width() - m_control.width())/2, (height() - m_control.height())/2 );
    }

    if( m_opacity > 0.01 ) {
        QPainter painter( this );
        painter.setOpacity( m_opacity );

        painter.drawImage( m_controlpos, m_control );
    }
}

qreal ThrottleWidget::calculateIntensity( const QPoint& point )
{
    int center = rect().center().x();

    qreal intensity = (qreal)(point.x() - center) / (qreal)center;

    // Limit intensity between -1..1
    if( intensity < -1.0 ) {
        intensity = -1.0;
    }
    if( intensity > 1.0 ) {
        intensity = 1.0;
    }

    return intensity;
}

class ThrottleControl : public QWidget
{
    Q_OBJECT
public:
    ThrottleControl( QWidget* parent = 0 );

signals:
    void clicked();

    // Intensity changed to either -1, 0 or 1
    void intensityChanged( int intensity );

private slots:
    void processPressed();
    void processReleased();
    void processIntensityChange();
    void processTimeout();

    void activate();
    void deactivate();

    void setOpacity( qreal opacity );

private:
    enum State { Deactivated, PendingActivate, Activated, PendingDeactivate };

    State state() const { return m_state; }
    void setState( State state ) { m_state = state; }

    ThrottleWidget *m_throttle;

    QTimer *m_timer;
    int m_intensity;
    State m_state;
};

ThrottleControl::ThrottleControl( QWidget* parent )
    : QWidget( parent ), m_intensity( 0 ), m_state( Deactivated )
{
    static const int CONTROL_SENSITIVITY = 500; // Sensitivity to user actions in ms

    QVBoxLayout *layout = new QVBoxLayout;
    layout->setMargin( 0 );

    m_throttle = new ThrottleWidget;
    connect( m_throttle, SIGNAL(pressed()),
        this, SLOT(processPressed()) );
    connect( m_throttle, SIGNAL(released()),
        this, SLOT(processReleased()) );
    connect( m_throttle, SIGNAL(intensityChanged(qreal)),
        this, SLOT(processIntensityChange()) );
    m_throttle->setResolution( 0.2 );
    m_throttle->setOpacity( 0.0 );

    layout->addWidget( m_throttle );
    setLayout( layout );

    m_timer = new QTimer( this );
    connect( m_timer, SIGNAL(timeout()), this, SLOT(processTimeout()) );
    m_timer->setInterval( CONTROL_SENSITIVITY );
    m_timer->setSingleShot( true );
}

void ThrottleControl::processPressed()
{
    switch( state() )
    {
    case Deactivated:
        m_timer->start();
        setState( PendingActivate );
        break;
    case PendingDeactivate:
        m_timer->stop();
        setState( Activated );
        processIntensityChange();
        break;
    default:
        // Ignore
        break;
    }
}

void ThrottleControl::processReleased()
{
    switch( state() )
    {
    case PendingActivate:
        m_timer->stop();
        setState( Deactivated );
        emit clicked();
        break;
    case Activated:
        m_timer->start();
        setState( PendingDeactivate );
        processIntensityChange();
        break;
    default:
        // Ignore
        break;
    }
}

void ThrottleControl::processIntensityChange()
{
    static const qreal NEGATIVE_THRESHOLD = -0.35;
    static const qreal POSITIVE_THRESHOLD = 0.35;

    if( state() == Activated || state() == PendingDeactivate ) {
        int intensity = 0;

        qreal value = m_throttle->intensity();
        if( value <= NEGATIVE_THRESHOLD ) {
            intensity = -1;
        } else if( value >= POSITIVE_THRESHOLD ) {
            intensity = 1;
        }

        if( intensity != m_intensity ) {
            m_intensity = intensity;

            emit intensityChanged( m_intensity );
        }
    }
}

void ThrottleControl::processTimeout()
{
    switch( state() )
    {
    case PendingActivate:
        setState( Activated );
        activate();
        break;
    case PendingDeactivate:
        setState( Deactivated );
        deactivate();
    default:
        // Ignore
        break;
    }
}

void ThrottleControl::activate()
{
    static const int FADEIN_DURATION = 500;

    processIntensityChange();

    // Animate opacity
    QTimeLine *animation = new QTimeLine( FADEIN_DURATION, this );
    connect( animation, SIGNAL(valueChanged(qreal)),
        this, SLOT(setOpacity(qreal)) );
    connect( animation, SIGNAL(finished()),
        animation, SLOT(deleteLater()) );
    animation->start();
}

void ThrottleControl::deactivate()
{
    static const int FADEOUT_DURATION = 500;

    if( m_intensity != 0 ) {
        m_intensity = 0;
        emit intensityChanged( m_intensity );
    }

    // Animate opacity
    QTimeLine *animation = new QTimeLine( FADEOUT_DURATION, this );
    animation->setDirection( QTimeLine::Backward );
    connect( animation, SIGNAL(valueChanged(qreal)),
        this, SLOT(setOpacity(qreal)) );
    connect( animation, SIGNAL(finished()),
        animation, SLOT(deleteLater()) );
    animation->start();
}

void ThrottleControl::setOpacity( qreal opacity )
{
    static const qreal FULL_OPACITY = 0.65;

    m_throttle->setOpacity( opacity * FULL_OPACITY );
}

static const int KEY_LEFT_HOLD = Qt::Key_unknown + Qt::Key_Left;
static const int KEY_RIGHT_HOLD = Qt::Key_unknown + Qt::Key_Right;

class ThrottleKeyMapper : public QObject
{
    Q_OBJECT
public:
    ThrottleKeyMapper( ThrottleControl* control, QObject* parent );

    enum Mapping { LeftRight, UpDown };

    void setMapping( Mapping mapping ) { m_mapping = mapping; }

private slots:
    void processIntensityChange( int intensity );

private:
    Mapping m_mapping;
    int m_lastpressed;
};

ThrottleKeyMapper::ThrottleKeyMapper( ThrottleControl* control, QObject* parent )
    : QObject( parent ), m_mapping( LeftRight ), m_lastpressed(0)
{
    connect( control, SIGNAL(intensityChanged(int)),
        this, SLOT(processIntensityChange(int)) );
}

void ThrottleKeyMapper::processIntensityChange( int intensity )
{
    switch( intensity )
    {
    case -1:
        {
        if( m_lastpressed ) {
            // Send release event
            processIntensityChange( 0 );
        }
        m_lastpressed = m_mapping == LeftRight ? KEY_LEFT_HOLD : Qt::Key_Down;
        QKeyEvent event = QKeyEvent( QEvent::KeyPress, m_lastpressed, Qt::NoModifier );
        QCoreApplication::sendEvent( parent(), &event );
        }
        break;
    case 0:
        if( m_lastpressed ) {
            QKeyEvent event = QKeyEvent( QEvent::KeyRelease, m_lastpressed, Qt::NoModifier );
            QCoreApplication::sendEvent( parent(), &event );
            m_lastpressed = 0;
        }
        break;
    case 1:
        {
        if( m_lastpressed ) {
            // Send release event
            processIntensityChange( 0 );
        }
        m_lastpressed = m_mapping == LeftRight ? KEY_RIGHT_HOLD : Qt::Key_Up;
        QKeyEvent event = QKeyEvent( QEvent::KeyPress, m_lastpressed, Qt::NoModifier );
        QCoreApplication::sendEvent( parent(), &event );
        }
        break;
    }
}

class PileLayout : public QLayout
{
public:
    PileLayout( QWidget* parent = 0 )
        : QLayout( parent )
    { }
    ~PileLayout();

    void addLayout( QLayout* layout );
    int count() const { return m_pile.count(); }
    void addItem( QLayoutItem* item );
    QSize sizeHint() const;
    QSize minimumSize() const;
    QLayoutItem *itemAt( int index ) const;
    QLayoutItem *takeAt( int index );
    void setGeometry( const QRect& rect );

private:
    QList<QLayoutItem*> m_pile;
};

PileLayout::~PileLayout()
{
    foreach( QLayoutItem* item, m_pile ) {
        delete item;
    }
}

void PileLayout::addLayout( QLayout* layout )
{
    QWidget* widget = new QWidget;
    widget->setLayout( layout );
    addWidget( widget );
}

void PileLayout::addItem( QLayoutItem* item )
{
    m_pile.append( item );
}

QSize PileLayout::sizeHint() const
{
    QSize hint( 0, 0 );

    foreach( QLayoutItem* item, m_pile ) {
        hint = hint.expandedTo( item->sizeHint() );
    }

    return hint;
}

QSize PileLayout::minimumSize() const
{
    QSize min( 0, 0 );

    foreach( QLayoutItem* item, m_pile ) {
        min = min.expandedTo( item->minimumSize() );
    }

    return min;
}


QLayoutItem* PileLayout::itemAt( int index ) const
{
    return m_pile.value( index );
}

QLayoutItem* PileLayout::takeAt( int index )
{
    if( index >= 0 && index < m_pile.count() ) {
        return m_pile.takeAt( index );
    }

    return 0;
}

void PileLayout::setGeometry( const QRect& rect )
{
    QLayout::setGeometry( rect );

    foreach( QLayoutItem* item, m_pile ) {
        item->setGeometry( rect );
    }
}

static const int KEY_SELECT_HOLD = Qt::Key_unknown + Qt::Key_Select;

PlayerWidget::PlayerWidget( PlayerControl* control, QWidget* parent )
    : QWidget( parent )
      , m_playercontrol( control )
      , m_content( 0 )
      , m_mediacontrol( 0 )
      , m_videoControl( 0 )
#ifndef NO_HELIX
      , m_settingsaction( 0 )
#endif
      , m_videowidget( 0 )
#ifndef NO_THUMBNAIL
      , m_thumbnail( 0 )
#endif
      , m_currentview( None )
      , m_fullscreen( false )
      , m_fullscreenaction( 0 )
      , m_muteaction( 0 )
      , m_muteicon( 0 )
      , m_voteaction( 0 )
      , m_votedialog( 0 )
      , m_repeataction( 0 )
      , m_repeatdialog( 0 )
      , m_skipdialog( 0 )
#ifndef NO_VISUALIZATION
      , m_visualization( 0 )
#endif
      , m_tvScreen(0)
      , rotHelper(this, false)
{
    static const int HOLD_THRESHOLD = 500;
    static const int STRETCH_MAX = 1;

    m_repeatstate = new RepeatState( this );

    m_fullscreenWidget = new QFrame;
    m_fullscreenWidget->setLineWidth(0);
    //m_fullscreenwidget->setLineWidth(2);
    //m_fullscreenwidget->setFrameStyle( QFrame::Box );
    m_fullscreenWidget->setWindowTitle( tr( "Video" ) );
    m_fullscreenWidget->installEventFilter( this );
    m_fullscreenWidget->setFocusPolicy( Qt::StrongFocus );
    m_fullscreenWidget->setFocusProxy(this);


    m_fullscreenWidgetLayout = new QVBoxLayout;
    m_fullscreenWidgetLayout->setSpacing( 0 );
    m_fullscreenWidgetLayout->setMargin( 0 );

    m_fullscreenWidget->setLayout( m_fullscreenWidgetLayout );
    m_fullscreenWidget->hide();

    m_fullscreenControls = new QWidget;
    m_fullscreenControls->setWindowTitle( tr( "Fullscreen Video Controls" ) );
    m_fullscreenControls->setMask( QRegion( -1, -1, 1, 1 ) );
    m_fullscreenControls->installEventFilter( this );
    m_fullscreenControls->setFocusPolicy( Qt::StrongFocus );
    m_fullscreenControls->setFocusProxy(this);

    m_fullscreenControlsLayout = new QVBoxLayout;
    m_fullscreenControlsLayout->setSpacing( 0 );
    m_fullscreenControlsLayout->setMargin( 0 );
    m_fullscreenControlsLayout->addStretch();

    m_fullscreenControls->setLayout( m_fullscreenControlsLayout );
    m_fullscreenControls->hide();

    m_background = new QVBoxLayout;
    m_background->setMargin( 0 );

#ifndef NO_VISUALIZATION
    m_visualization = new VisualizationWidget;
    m_background->addWidget( m_visualization );
#endif
    setLayout( m_background );

    QVBoxLayout *layout = new QVBoxLayout;
    layout->setMargin( 6 );

    m_statewidget = new StateWidget( control );

    QHBoxLayout *hbox = new QHBoxLayout;
    QVBoxLayout *stateWidgetVBox=new QVBoxLayout;
    stateWidgetVBox->addWidget(m_statewidget);
    stateWidgetVBox->addStretch();
    hbox->addLayout( stateWidgetVBox );

    m_muteicon = new QMediaVolumeLabel( QMediaVolumeLabel::MuteVolume );
    hbox->addWidget( m_muteicon );

    hbox->addStretch();

    layout->addLayout( hbox );

    m_videolayout  = new QVBoxLayout;

    m_skipdialog = new SkipDialog( this );
#ifndef NO_HELIX
    //m_helixlogoaudio = new HelixLogo;
    //m_videolayout->addWidget( m_helixlogoaudio );
#endif

#ifndef NO_THUMBNAIL
    m_thumbnail = new ThumbnailWidget( control, this );
    m_videolayout->addWidget( m_thumbnail );
#endif

    m_videolayout->setMargin( 0 );

    layout->addLayout( m_videolayout, STRETCH_MAX );

    layout->addStretch();

    m_controlsLayout = new QVBoxLayout;
    m_controlsLayout->setMargin( 0 );
    m_controlsLayout->addStretch();

    m_progressview = new ProgressView( m_repeatstate );
    m_controlsLayout->addWidget( m_progressview );

    m_volumeview = new VolumeView;
    m_controlsLayout->addWidget( m_volumeview );

    m_seekview = new SeekView;
    m_controlsLayout->addWidget( m_seekview );

    pile = new PileLayout;
    pile->addLayout( m_controlsLayout );

    ThrottleControl *throttle = new ThrottleControl;
    connect( throttle, SIGNAL(clicked()), this, SLOT(cycleView()) );
    pile->addWidget( throttle );

    m_mapper = new ThrottleKeyMapper( throttle, this );

    hbox = new QHBoxLayout;
    hbox->setMargin( 0 );
    hbox->addLayout( pile );

#ifndef NO_HELIX
    //m_helixlogovideo = new HelixLogo;
    //hbox->addWidget( m_helixlogovideo );
#endif

    layout->addLayout( hbox );

#ifndef NO_VISUALIZATION
    m_visualization->setLayout( layout );
#else
    m_background->addLayout( layout );
#endif

    QTimer::singleShot(1, this, SLOT(delayMenuCreation()));

    m_trackinfo = new TrackInfoWidget( control, this );
    //hbox->addWidget( m_trackinfo, STRETCH_MAX );

    m_ismute = false;
    m_keepMutedState = false;
    m_muteicon->hide();

#ifndef NO_HELIX
    //m_helixlogovideo->hide();
#endif

    new KeyHold( Qt::Key_Left, KEY_LEFT_HOLD, HOLD_THRESHOLD, this, this );
    new KeyHold( Qt::Key_Right, KEY_RIGHT_HOLD, HOLD_THRESHOLD, this, this );
    new KeyHold( Qt::Key_MediaPrevious, KEY_LEFT_HOLD, HOLD_THRESHOLD, this, this );
    new KeyHold( Qt::Key_MediaNext, KEY_RIGHT_HOLD, HOLD_THRESHOLD, this, this );

    // Activity monitor
    m_monitor = new ActivityMonitor( 4000, this );
    connect( m_monitor, SIGNAL(inactive()), this, SLOT(processInactivity()) );

    m_pingtimer = new QTimer( this );
    m_pingtimer->setInterval( 1000 );
    connect( m_pingtimer, SIGNAL(timeout()), this, SLOT(pingMonitor()) );

    m_context = new QMediaContentContext( this );
    m_context->addObject( m_progressview );
    m_context->addObject( m_volumeview );
    m_context->addObject( m_seekview );

    QMediaControlNotifier *notifier = new QMediaControlNotifier( QMediaControl::name(), this );
    connect( notifier, SIGNAL(valid()), this, SLOT(activate()) );
    connect( notifier, SIGNAL(invalid()), this, SLOT(deactivate()) );
    m_context->addObject( notifier );

    notifier = new QMediaControlNotifier( QMediaVideoControl::name(), this );
    connect( notifier, SIGNAL(valid()), this, SLOT(activateVideo()) );
    connect( notifier, SIGNAL(invalid()), this, SLOT(deactivateVideo()) );
    m_context->addObject( notifier );

    setFocusProxy( m_statewidget );
    setFocusPolicy( Qt::StrongFocus );

    // Filter application key events for media keys
    qApp->installEventFilter( this );

    // Context sensitive help hint
    setObjectName( "playback" );

    setView( Progress );

    m_fullscreen = true; // to ensure showNormalVideo will do his job
    showNormalVideo();

    QRect rect = QApplication::desktop()->availableGeometry( QScreenInformation( QScreenInformation::Normal ).screenNumber() );
    m_fullscreenWidget->setGeometry(rect);
    m_fullscreenControls->setGeometry(rect);

    m_tvScreen = new QScreenInformation(QScreenInformation::Television);
    if ( m_tvScreen->screenNumber() != -1) {
        connect(m_tvScreen, SIGNAL(changed()), this, SLOT(tvScreenChanged()));
    }
    else {
        delete m_tvScreen;
        m_tvScreen = 0;
    }
    connect( &m_playlist, SIGNAL(playingChanged(QModelIndex)),
        this, SLOT(playingChanged(QModelIndex)) );

    connect( QApplication::desktop(), SIGNAL(resized(int)),  this, SLOT(updateVideoRotation()) );

    QTimer *maskUpdateTimer = new QTimer( this );
    maskUpdateTimer->setSingleShot( false );
    connect( maskUpdateTimer,  SIGNAL(timeout()),  this,  SLOT(updateFullScreenControlsMask()) );
    maskUpdateTimer->start(1000);
    
    connect(&rotHelper, SIGNAL(rotated(bool)), this, SLOT(toggleFullScreenVideo(bool)));
    rotHelper.start();
}

PlayerWidget::~PlayerWidget()
{
    toggleFullScreenVideo( false );
    if( m_videowidget ) {
        delete m_videowidget;
    }
    rotHelper.restore();
}

void PlayerWidget::setPlaylist( const QMediaPlaylist &playlist )
{
    m_playlist = playlist;

    if( m_playlist.rowCount() > 0 ) {
        openCurrentTrack();
    } else {
        qLog(Media) << "PlayerWidget::setPlaylist playlist is null";
    }

    // If playlist is a my shuffle playlist, enable voting and disable repeat
    // Otherwise, disable voting and enable repeat
    if( m_voteaction != NULL)
    {
        if( playlist.isShuffle() ) {
            m_voteaction->setVisible( true );
            m_repeataction->setVisible( false );
        } else {
            m_voteaction->setVisible( false );
            m_repeataction->setVisible( true );
        }
    }

    // Reset repeat state
    m_repeatstate->setState( RepeatState::RepeatNone );

    m_progressview->setPlaylist( m_playlist );
    m_trackinfo->setPlaylist( m_playlist );
#ifndef NO_THUMBNAIL
    m_thumbnail->setPlaylist( m_playlist );
#endif
}

bool PlayerWidget::eventFilter( QObject* o, QEvent* e )
{
    // Guard against recursion
    static QEvent* d = 0;

    //this require fixing qt to pass mouse events to embedder window instread of embedded direct painter surface
    if ( o == m_videowidget && e->type() == QEvent::MouseButtonRelease ) {
        toggleFullScreenVideo( !isFullScreenVideo() );
        return true;
    }

    //pass the key events from the full screen container to PlayerWidget
    if (isFullScreenVideo() &&
        (o == m_fullscreenWidget || o == m_fullscreenControls) &&
        d != e) {

        if ((e->type() == QEvent::KeyPress || e->type() == QEvent::KeyRelease)) {
            QKeyEvent *ke = static_cast<QKeyEvent*>(e);

            switch( ke->key() )
            {
            case Qt::Key_Select:
            case KEY_SELECT_HOLD:
                {
                d = e;
                QCoreApplication::sendEvent( m_statewidget, e );
                d = 0;
                break;
                }
            case Qt::Key_Back:
                break; // just close the fullscreen widget
            default: {
                    d = e;
                    QCoreApplication::sendEvent( this, e );
                    d = 0;
                }
            }

            return true;
        }

        if (e->type() == QEvent::Close) {
            showNormalVideo();
            return true;
        }
    }

    return false;
}

void PlayerWidget::setMediaContent( QMediaContent* content )
{
    if( m_content ) {
        m_content->disconnect( this );
    }

    m_content = content;

    if( content ) {
        connect( content, SIGNAL(mediaError(QString)),
            this, SLOT(displayErrorMessage(QString)) );
    }

    m_context->setMediaContent( content );
}

void PlayerWidget::activate()
{
    m_mediacontrol = new QMediaControl( m_content );
    connect( m_mediacontrol, SIGNAL(volumeMuted(bool)),
        this, SLOT(setMuteDisplay(bool)) );
    connect( m_mediacontrol, SIGNAL(playerStateChanged(QtopiaMedia::State)),
        this, SLOT(changeState(QtopiaMedia::State)) );

    if ( !m_keepMutedState )
        setMuteDisplay( false );

    m_keepMutedState = false;
    m_mediacontrol->setMuted( m_ismute );
}

void PlayerWidget::deactivate()
{
    delete m_mediacontrol;
    m_mediacontrol = 0;
}

void PlayerWidget::activateVideo()
{
    m_videoControl = new QMediaVideoControl( m_content );
    setVideo( m_videoControl->createVideoWidget( this ) );
    updateVideoRotation();

#ifndef NO_THUMBNAIL
    m_thumbnail->setVisible(false);
#endif
}

void PlayerWidget::deactivateVideo()
{
    delete m_videoControl;
    m_videoControl = 0;
    removeVideo();
#ifndef NO_THUMBNAIL
    m_thumbnail->setVisible(true);
#endif
}

void PlayerWidget::displayErrorMessage( const QString& message )
{
    QMessageBox mbox(QMessageBox::Warning, tr( "Media Player Error" ), QString( "<qt>%1</qt>" ).arg( message ), QMessageBox::Ok, this);
    QTimer::singleShot(15000, &mbox, SLOT(accept()));
    mbox.exec();
}

void PlayerWidget::changeState( QtopiaMedia::State state )
{
    switch (state) {

    case QtopiaMedia::Playing:
        setView( Progress );
        if (m_videowidget != 0) {
            QtopiaApplication::setPowerConstraint( QtopiaApplication::Disable );
        }
        break;

    case QtopiaMedia::Paused:
        setView( Progress );
        if (m_videowidget != 0)
            QtopiaApplication::setPowerConstraint( QtopiaApplication::Enable );
        break;

    case QtopiaMedia::Stopped:
        if( m_playercontrol->state() == PlayerControl::Playing ) {
            m_keepMutedState = true;
            continuePlaying();
        } else
            showNormalVideo();
        break;

    case QtopiaMedia::Error:
        showNormalVideo();
        m_playercontrol->setState( PlayerControl::Stopped );
        displayErrorMessage( m_mediacontrol->errorString() );
        if( m_playlist.rowCount() > 1 && m_playlist.nextIndex().isValid() ) {
            m_keepMutedState = true;
            continuePlaying();
            m_playercontrol->setState( PlayerControl::Playing );
        }
        break;
    default:
        // Ignore
        break;
    }
}

void PlayerWidget::setMuteDisplay( bool mute )
{
    if( m_ismute == mute ) {
        return;
    }

    m_ismute = mute;

    if( m_ismute ) {
        m_muteaction->setText( tr( "Mute Off" ) );
        m_muteicon->show();
    } else {
        m_muteaction->setText( tr( "Mute On" ) );
        m_muteicon->hide();
    }

    QResizeEvent e( size(), size() );
    resizeEvent( &e );
}

void PlayerWidget::playingChanged( const QModelIndex& index )
{
    if( index.isValid() ) {
        openCurrentTrack();
    } else {
        m_playercontrol->setState( PlayerControl::Stopped );
    }

    // If repeat state is repeat one, reset repeat
    if( m_repeatstate->state() == RepeatState::RepeatOne ) {
        m_repeatstate->setState( RepeatState::RepeatNone );
    }
}

void PlayerWidget::pingMonitor()
{
    m_monitor->update();
}

void PlayerWidget::showProgress()
{
    setView( Progress );
}

void PlayerWidget::cycleView()
{
    switch( view() )
    {
    case Progress:
        setView( Volume );
        m_monitor->update();
        break;
    case Seek:
        setView( Volume );
        m_monitor->update();
        break;
    case Volume:
        setView( Progress );
        break;
    case None:
        break;
    }
}

void PlayerWidget::continuePlaying()
{
    // If repeat state is repeat one, play again
    // Otherwise, skip forward one playlist item
    if (m_repeatstate->state() == RepeatState::RepeatOne ||
        (m_repeatstate->state() == RepeatState::RepeatAll && m_playlist.rowCount() == 1)) {
        m_mediacontrol->start();
    } else {
        QModelIndex index = m_playlist.nextIndex();

        if (index.isValid()) {
            m_playlist.setPlaying(index);
        } else {
            // If repeat state is repeat all, play from begining
            if (m_repeatstate->state() == RepeatState::RepeatAll)
                m_playlist.playFirst();
            else {
                m_playercontrol->setState(PlayerControl::Stopped);

                if (m_videowidget != 0) {
                    showNormalVideo();
                    QtopiaApplication::setPowerConstraint( QtopiaApplication::Enable );
                }
            }
        }
    }
}

void PlayerWidget::toggleMute()
{
    if (m_mediacontrol)
        m_mediacontrol->setMuted(!m_mediacontrol->isMuted());
}

void PlayerWidget::execSettings()
{
#ifndef NO_HELIX
    if ( isFullScreenVideo() )
        toggleFullScreenVideo( false );

    MediaPlayerSettingsDialog settingsdialog( this );
    QtopiaApplication::execDialog( &settingsdialog );
#endif
}

void PlayerWidget::keyPressEvent(QKeyEvent* e)
{
    static const unsigned int REPEAT_THRESHOLD = 3000; // 3 seconds

    if (e->isAutoRepeat() || !m_mediacontrol) {
        e->ignore();
        return;
    }

    switch( e->key() )
    {
    case Qt::Key_Up:
//    case Qt::Key_VolumeUp:
        {
            e->accept();
            setView( Volume );

            QKeyEvent event = QKeyEvent( QEvent::KeyPress, Qt::Key_Left, Qt::NoModifier );
            QCoreApplication::sendEvent( m_volumeview->keyEventHandler(), &event );

            m_pingtimer->start();
            m_monitor->update();
        }
        break;
    case Qt::Key_Down:
//    case Qt::Key_VolumeDown:
        {
            e->accept();
            setView( Volume );

            QKeyEvent event = QKeyEvent( QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier );
            QCoreApplication::sendEvent( m_volumeview->keyEventHandler(), &event );

            m_pingtimer->start();
            m_monitor->update();
        }
        break;
    case Qt::Key_Left:
    case Qt::Key_MediaPrevious:
        // If more than the repeat threshold into the track, seek to the beginning
        // Otherwise, skip backward one playlist item
        e->accept();
        if( m_mediacontrol->position() > REPEAT_THRESHOLD ) {
            m_mediacontrol->seek( 0 );
        } else {
            m_playlist.playPrevious();
        }
        break;
    case Qt::Key_MediaNext:
    case Qt::Key_Right:
        {
            e->accept();
            // Skip forward one playlist item
            QModelIndex index = m_playlist.nextIndex();
            if( index.isValid() ) {
                m_playlist.setPlaying( index );
            } else {
                // If repeat state is repeat all, skip to beginning
                if( m_repeatstate->state() == RepeatState::RepeatAll ) {
                    m_playlist.playFirst();
                }
            }
        }
        break;
    case KEY_LEFT_HOLD:
        e->accept();
        if( m_playercontrol->state() != PlayerControl::Stopped && m_mediacontrol->length() > 0 ) {
            setView( Seek);

            QKeyEvent event = QKeyEvent( QEvent::KeyPress, Qt::Key_Left, Qt::NoModifier );
            QCoreApplication::sendEvent( m_seekview->keyEventHandler(), &event );

            m_pingtimer->start();
            m_monitor->update();
        }
        break;
    case KEY_RIGHT_HOLD:
        e->accept();
        if( m_playercontrol->state() != PlayerControl::Stopped && m_mediacontrol->length() > 0 ) {
            setView( Seek );

            QKeyEvent event = QKeyEvent( QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier );
            QCoreApplication::sendEvent( m_seekview->keyEventHandler(), &event );

            m_pingtimer->start();
            m_monitor->update();
        }
        break;
    case Qt::Key_1:
        e->accept();
        if( !m_playlist.isShuffle() ) {
            m_repeatstate->setState( RepeatState::RepeatOne );
        }
        break;
    case Qt::Key_Asterisk:
        e->accept();
        if( !m_playlist.isShuffle() ) {
            m_repeatstate->setState( RepeatState::RepeatAll );
        }
        break;
    case Qt::Key_0:
        e->accept();
        if( !m_playlist.isShuffle() ) {
            m_repeatstate->setState( RepeatState::RepeatNone );
        }
        break;
    default:
        // Ignore
        QWidget::keyPressEvent(e);
        break;
    }
}

void PlayerWidget::keyReleaseEvent( QKeyEvent* e )
{
    if( e->isAutoRepeat() || !m_mediacontrol ) { e->ignore(); return; }

    switch( e->key() )
    {
    case Qt::Key_Up:
//    case Qt::Key_VolumeUp:
        {
            e->accept();
            QKeyEvent event = QKeyEvent( QEvent::KeyRelease, Qt::Key_Left, Qt::NoModifier );
            QCoreApplication::sendEvent( m_volumeview->keyEventHandler(), &event );

            m_pingtimer->stop();
        }
        break;
    case Qt::Key_Down:
//    case Qt::Key_VolumeDown:
        {
            e->accept();
            QKeyEvent event = QKeyEvent( QEvent::KeyRelease, Qt::Key_Right, Qt::NoModifier );
            QCoreApplication::sendEvent( m_volumeview->keyEventHandler(), &event );

            m_pingtimer->stop();
        }
        break;
    case KEY_LEFT_HOLD:
        if( m_playercontrol->state() != PlayerControl::Stopped && m_currentview == Seek ) {
            e->accept();
            QKeyEvent event = QKeyEvent( QEvent::KeyRelease, Qt::Key_Left, Qt::NoModifier );
            QCoreApplication::sendEvent( m_seekview->keyEventHandler(), &event );

            m_pingtimer->stop();
        }
        break;
    case KEY_RIGHT_HOLD:
        if( m_playercontrol->state() != PlayerControl::Stopped && m_currentview == Seek ) {
            e->accept();
            QKeyEvent event = QKeyEvent( QEvent::KeyRelease, Qt::Key_Right, Qt::NoModifier );
            QCoreApplication::sendEvent( m_seekview->keyEventHandler(), &event );

            m_pingtimer->stop();
        }
        break;
    default:
        // Ignore
        QWidget::keyReleaseEvent(e);
    }
}

void PlayerWidget::showEvent( QShowEvent* )
{
    if( m_videowidget ) {
        QtopiaApplication::setPowerConstraint( QtopiaApplication::Disable );
    }
#ifndef NO_VISUALIZATION
    else {
        m_visualization->setActive( true );
    }
#endif
}

void PlayerWidget::hideEvent( QHideEvent* )
{
    if( m_videowidget ) {
        QtopiaApplication::setPowerConstraint( QtopiaApplication::Enable );
    }

#ifndef NO_VISUALIZATION
    m_visualization->setActive( false );
#endif
}

void PlayerWidget::mouseReleaseEvent( QMouseEvent* e)
{
    m_skipdialog->timer->start(3000);
    QtopiaApplication::execDialog( m_skipdialog, false );

    QWidget::mouseReleaseEvent(e);
}

void PlayerWidget::resizeEvent( QResizeEvent* e)
{
    QWidget::resizeEvent(e);

    // adjust the trackinfo layouting.
    QRect mappedRect;
    QRect geom;
    if(m_muteicon->isVisible())
    {
        mappedRect.setTopLeft(mapTo(this, m_muteicon->geometry().topLeft()));
        mappedRect.setBottomRight(mapTo(this, m_muteicon->geometry().bottomRight()));
        geom.setLeft(mappedRect.right()+5);
        geom.setTop(mappedRect.top());
    }
    else
    {
        mappedRect.setTopLeft(mapTo(this, m_statewidget->geometry().topLeft()));
        mappedRect.setBottomRight(mapTo(this, m_statewidget->geometry().bottomRight()));
        geom.setLeft(mappedRect.right()+5);
        geom.setTop(mappedRect.top());
    }
    mappedRect.setTopLeft(mapTo(this, pile->geometry().topLeft()));
    mappedRect.setBottomRight(mapTo(this, pile->geometry().bottomRight()));
    geom.setRight(mappedRect.right()-5);
    mappedRect.setTopLeft(mapTo(this, pile->geometry().topLeft()));
    mappedRect.setBottomRight(mapTo(this, pile->geometry().bottomRight()));
    geom.setBottom(mappedRect.top()-5);

    m_trackinfo->setGeometry(geom);
    m_trackinfo->raise();
    //m_trackinfo->update();

    //ensure the volume/progress/seek bars have the same geometry
    QList<QWidget*> views;
    views << m_volumeview << m_progressview << m_seekview;
    int maxHeight = 0;
    foreach( QWidget *view, views )
        maxHeight = qMax( maxHeight, view->sizeHint().height() );
    foreach( QWidget *view, views )
        view->setFixedHeight( maxHeight );

}

void PlayerWidget::setView( View view )
{
    if ( view == m_currentview ) {
        return;
    }

    m_monitor->update();

    m_currentview = view;

    switch( m_currentview )
    {
    case Progress:
        m_volumeview->hide();
        m_seekview->hide();

        m_progressview->show();
        m_mapper->setMapping( ThrottleKeyMapper::LeftRight );
        break;
    case Volume:
        m_progressview->hide();
        m_seekview->hide();

        m_volumeview->show();
        m_mapper->setMapping( ThrottleKeyMapper::UpDown );
        break;
    case Seek:
        m_progressview->hide();
        m_volumeview->hide();

        m_seekview->show();
        m_mapper->setMapping( ThrottleKeyMapper::LeftRight );
        break;
    case None:
        m_progressview->hide();
        m_volumeview->hide();
        m_seekview->hide();

        m_mapper->setMapping( ThrottleKeyMapper::LeftRight );
        break;
    }


    updateFullScreenControlsMask();
}


void PlayerWidget::updateFullScreenControlsMask()
{
    if ( isFullScreenVideo() ) {
        QList<QWidget*> views;
        views << m_volumeview << m_seekview << m_progressview;
        QRegion mask;

        foreach( QWidget *viewWidget,  views ) {
            if ( viewWidget->isVisible() ) {
                if ( ! viewWidget->mask().isEmpty() )
                    mask = mask.united( viewWidget->mask() );
                else
                    mask = mask.united( viewWidget->geometry() );
            }
        }

        if ( mask.isEmpty() ) {
            mask = QRegion( -1,  -1,  1, 1 );
        } else {
            // we need this as a workaround about the bug with drawing the
            // child widgets if the parent's mask doesn't cover top pos.
            mask = mask.united( QRect(0, 0, 1, 1 ) );
        }

        m_fullscreenControls->setMask( mask );
    }
}

void PlayerWidget::processInactivity()
{
    if ( isFullScreenVideo() ) {
        setView( None );
    } else {
        setView( Progress );
    }
}


void PlayerWidget::updateVideoRotation()
{
#ifdef Q_WS_QWS
    if ( m_videoControl ) {
        QtopiaVideo::VideoRotation rotation = QtopiaVideo::Rotate0;

        QScreen *screen = QScreen::instance();
        if ( !screen->subScreens().isEmpty() ) {
            int screenNum = screen->subScreenIndexAt( m_fullscreenWidget->mapToGlobal( m_fullscreenWidget->rect().center() ) );
            if ( screenNum != -1 )
                screen = screen->subScreens()[screenNum];
        }

        if ( m_fullscreen && ( screen->width() < screen->height() ) )
#ifndef FULLSCREEN_VIDEO_ROTATION
            rotation = QtopiaVideo::Rotate90;
#else
            rotation = FULLSCREEN_VIDEO_ROTATION;
#endif

        if ( m_tvScreen && m_tvScreen->isVisible() )
            rotation = QtopiaVideo::Rotate0;

        m_videoControl->setVideoRotation( rotation );
    }
#endif
}


bool PlayerWidget::isFullScreenVideo() const
{
    return m_fullscreen;
}

void PlayerWidget::toggleFullScreenVideo( bool fullScreen )
{
    if ( fullScreen != m_fullscreen ) {
        m_fullscreen = fullScreen;
        if ( fullScreen && m_videowidget ) {
            setView( None );
        } else {
            if ( view() == None )
                setView( Progress );
        }
        updateVideoRotation();
        layoutVideoWidget();


        if ( m_fullscreenaction )
            m_fullscreenaction->setChecked( m_fullscreen );

        m_monitor->update();
    }
}

void PlayerWidget::showFullScreenVideo()
{
    toggleFullScreenVideo( true );
}

void PlayerWidget::showNormalVideo()
{
    toggleFullScreenVideo( false );
}


void PlayerWidget::layoutVideoWidget()
{
    if ( !m_videowidget )
        return;

    if ( isFullScreenVideo() ) {
        QString title = m_fullscreenWidget->windowTitle();
        m_fullscreenWidget->setWindowTitle( QLatin1String( "_allow_on_top_" ) );
        m_fullscreenWidget->setWindowFlags( m_fullscreenWidget->windowFlags() | Qt::WindowStaysOnTopHint );
        m_fullscreenWidget->setWindowState( Qt::WindowFullScreen );

        m_fullscreenWidgetLayout->addWidget( m_videowidget, 1 );
        m_fullscreenWidget->raise();
        m_fullscreenWidget->setWindowTitle( title );

        //show controls widgets over video:
        title = m_fullscreenControls->windowTitle();
        m_fullscreenControls->setWindowTitle( QLatin1String( "_allow_on_top_" ) );
        m_fullscreenControls->setWindowFlags( m_fullscreenControls->windowFlags() | Qt::WindowStaysOnTopHint );
        m_fullscreenControls->setWindowState( Qt::WindowFullScreen );

        m_fullscreenControlsLayout->addWidget( m_progressview );
        m_fullscreenControlsLayout->addWidget( m_seekview );
        m_fullscreenControlsLayout->addWidget( m_volumeview );

        m_fullscreenControls->raise();
        //m_fullscreenControls->setWindowTitle( title );

        updateFullScreenControlsMask();
    } else {
        m_fullscreenWidget->showNormal();
        m_fullscreenWidget->hide();

        m_fullscreenControls->showNormal();
        m_fullscreenControls->hide();

        m_videolayout->addWidget( m_videowidget );
        m_controlsLayout->addWidget( m_progressview );
        m_controlsLayout->addWidget( m_seekview );
        m_controlsLayout->addWidget( m_volumeview );
    }
}

void PlayerWidget::setVideo( QWidget* widget )
{
    m_videowidget = widget;
    m_videowidget->installEventFilter( this );

    if (m_tvScreen != 0 && m_tvScreen->isVisible())
        m_tvScreen->setClonedScreen(-1);
    else
        layoutVideoWidget();

#ifndef NO_VISUALIZATION
    m_visualization->setActive( false );
#endif

#ifndef NO_HELIX
    //m_helixlogoaudio->hide();
    //m_helixlogovideo->show();
#endif

    m_fullscreenaction->setEnabled(true);
    QtopiaApplication::setPowerConstraint( QtopiaApplication::Disable );
}

void PlayerWidget::removeVideo()
{
    delete m_videowidget;
    m_videowidget = 0;

    if (m_tvScreen != 0)
        m_tvScreen->setClonedScreen(QApplication::desktop()->primaryScreen());

#ifndef NO_VISUALIZATION
    m_visualization->setActive( true );
#endif

#ifndef NO_HELIX
    //m_helixlogovideo->hide();
    //m_helixlogoaudio->show();
#endif

    if (m_fullscreenaction)
        m_fullscreenaction->setEnabled(false);
    QtopiaApplication::setPowerConstraint( QtopiaApplication::Enable );
}

void PlayerWidget::openCurrentTrack()
{
    // Open and play current playlist item
    if(!m_playlist.playing().isValid())
        m_playlist.playFirst();
    QUrl url = qvariant_cast<QUrl>(m_playlist.data( m_playlist.playing(), QMediaPlaylist::Url ));

    m_playercontrol->open(url);
}

void PlayerWidget::execVoteDialog()
{
    if( m_votedialog == NULL )
    {
        m_votedialog = new VoteDialog( this );
        connect( m_votedialog, SIGNAL(snoozeVoted()), this, SLOT(continuePlaying()) );
        connect( m_votedialog, SIGNAL(banVoted()), this, SLOT(continuePlaying()) );
    }
    QtopiaApplication::execDialog( m_votedialog, false );
}

void PlayerWidget::execRepeatDialog()
{
    if( m_repeatdialog == NULL )
        m_repeatdialog = new RepeatDialog( m_repeatstate, this );

    QtopiaApplication::execDialog( m_repeatdialog, false );

    if ( isFullScreenVideo() )
        layoutVideoWidget();
}

void PlayerWidget::delayMenuCreation()
{
    // Construct soft menu bar
    QMenu *menu = QSoftMenuBar::menuFor( this );

    m_fullscreenaction = new QAction( QIcon( ":icon/fullscreen" ), tr( "Show Fullscreen" ), this );
    m_fullscreenaction->setCheckable(true);
    m_fullscreenaction->setChecked( isFullScreenVideo() );
    connect( m_fullscreenaction, SIGNAL(triggered(bool)), this, SLOT(toggleFullScreenVideo(bool)) );
    menu->addAction( m_fullscreenaction );
    m_fullscreenaction->setEnabled( m_videowidget != 0 );


    m_muteaction = new QAction( QIcon( ":icon/mute" ), tr( "Mute" ), this );
    connect( m_muteaction, SIGNAL(triggered()), this, SLOT(toggleMute()) );
    menu->addAction( m_muteaction );
    m_muteaction->setText( tr( "Mute On" ) );

    m_voteaction = new QAction( QIcon( ":icon/mediaplayer/black/vote" ), tr( "Vote..." ), this );
    connect( m_voteaction, SIGNAL(triggered()), this, SLOT(execVoteDialog()) );
    menu->addAction( m_voteaction );

    m_repeataction = new QAction( QIcon( ":icon/mediaplayer/black/repeat" ), tr( "Repeat..." ), this );
    connect( m_repeataction, SIGNAL(triggered()), this, SLOT(execRepeatDialog()) );
    menu->addAction( m_repeataction );

    menu->addSeparator();

#ifndef NO_HELIX
    m_settingsaction = new QAction( QIcon( ":icon/settings" ), tr( "Settings..." ), this );
    connect( m_settingsaction, SIGNAL(triggered()), this, SLOT(execSettings()) );
    menu->addAction( m_settingsaction );
#endif

    if( m_voteaction != NULL)
    {
        if( m_playlist.isShuffle() ) {
            m_voteaction->setVisible( true );
            m_repeataction->setVisible( false );
        } else {
    m_voteaction->setVisible( false );
    m_repeataction->setVisible( true );
        }
    }

    // just to show menu button, PlayerWidget menu is used
    // since key events are processed by PlayerWidget
    QSoftMenuBar::menuFor( m_fullscreenWidget );

}

void PlayerWidget::tvScreenChanged()
{
    if (m_videowidget == 0)
        return;

    if (m_tvScreen->isVisible()) {
        showNormalVideo();

        QRect rect = QApplication::desktop()->availableGeometry(m_tvScreen->screenNumber());
        m_fullscreenWidget->setGeometry(rect);
        m_fullscreenControls->setGeometry(rect);

        if ( m_videoControl )
            m_videoControl->setVideoRotation( QtopiaVideo::Rotate0 );

        showFullScreenVideo();

    }
    else {
        showNormalVideo();

        QRect rect = QApplication::desktop()->availableGeometry( QScreenInformation( QScreenInformation::Normal ).screenNumber() );
        m_fullscreenWidget->setGeometry(rect);
        m_fullscreenControls->setGeometry(rect);
    }
}

#include "playerwidget.moc"

