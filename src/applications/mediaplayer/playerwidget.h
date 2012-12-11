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

#ifndef PLAYERWIDGET_H
#define PLAYERWIDGET_H

#include "playercontrol.h"
#include "statewidget.h"
#include "rotate.h"

#include <media.h>
#include <qmediacontent.h>
#include <qmediacontrol.h>
#include <qmediatools.h>
#include <private/activitymonitor_p.h>

#include <QtGui>
#include <QMediaPlaylist>

class QScreenInformation;

class VisualizationWidget;

class PlayerControl;
class QMediaVideoControl;

class ProgressView;
class VolumeView;
class SeekView;

class ThrottleKeyMapper;

class TrackInfoWidget;

class VoteDialog;

class RepeatState;
class RepeatDialog;
class SkipDialog;
class ThumbnailWidget;

class PileLayout;

class PlayerWidget : public QWidget
{
    Q_OBJECT
        friend class SkipDialog;
public:
    PlayerWidget( PlayerControl* control, QWidget* parent = 0 );
    ~PlayerWidget();

   const QMediaPlaylist &playlist() const { return m_playlist; }
    // Set playlist and begin playback
    void setPlaylist( const QMediaPlaylist &playlist );

    bool eventFilter( QObject* o, QEvent* e );

#ifndef NO_HELIX
    QAction *settingsAction(){ return m_settingsaction; }
#endif

public slots:
    void setMediaContent( QMediaContent* content );

private slots:
    void activate();
    void deactivate();

    void activateVideo();
    void deactivateVideo();

    void displayErrorMessage( const QString& message );

    void changeState( QtopiaMedia::State state );

    void setMuteDisplay( bool mute );

    void playingChanged( const QModelIndex& index );

    void pingMonitor();

    void processInactivity();

    void showProgress();

    void cycleView();

    void continuePlaying();

    void toggleMute();

    void execSettings();

    void execVoteDialog();

    void execRepeatDialog();

    void delayMenuCreation();

    void showFullScreenVideo();
    void showNormalVideo();
    //void toggleFullScreenVideo();
    void toggleFullScreenVideo( bool fullScreen );
    void updateFullScreenControlsMask();
    void updateVideoRotation();

    void tvScreenChanged();

protected:
    void keyPressEvent( QKeyEvent* e );
    void keyReleaseEvent( QKeyEvent* e );

    void showEvent( QShowEvent* e );
    void hideEvent( QHideEvent* e );
    void resizeEvent( QResizeEvent* e);
    void mouseReleaseEvent( QMouseEvent* e );

private:
    enum View { Progress, Volume, Seek, None };

    View view() const { return m_currentview; }
    void setView( View view );

    void setVideo( QWidget* widget );
    void removeVideo();
    void layoutVideoWidget();

    void openCurrentTrack();

    bool isFullScreenVideo() const;


    PlayerControl *m_playercontrol;
    StateWidget *m_statewidget;

    QMediaContent *m_content;
    QMediaControl *m_mediacontrol;
    QMediaVideoControl *m_videoControl;
    QMediaContentContext *m_context;

#ifndef NO_HELIX
    QAction *m_settingsaction;
#endif


    QVBoxLayout *m_background;
    PileLayout *pile;

    QVBoxLayout *m_videolayout;
    QWidget *m_videowidget;
    QFrame *m_fullscreenWidget;
    QWidget *m_fullscreenControls;

    QVBoxLayout *m_controlsLayout;
    QVBoxLayout *m_fullscreenControlsLayout;
    QVBoxLayout *m_fullscreenWidgetLayout;

#ifndef NO_VISUALIZATION
    VisualizationWidget *m_visualization;
#endif
#ifndef NO_THUMBNAIL
    ThumbnailWidget *m_thumbnail;
#endif

#ifndef NO_HELIX
    QWidget *m_helixlogoaudio;
    QWidget *m_helixlogovideo;
#endif

    ProgressView *m_progressview;
    VolumeView *m_volumeview;
    SeekView *m_seekview;

    View m_currentview;
    bool m_fullscreen;
    QAction *m_fullscreenaction;

    ActivityMonitor *m_monitor;
    QTimer *m_pingtimer;

    ThrottleKeyMapper *m_mapper;

    QMediaPlaylist m_playlist;

    bool m_continue;

    bool m_ismute;
    bool m_keepMutedState;
    QAction *m_muteaction;
    QWidget *m_muteicon;

    TrackInfoWidget *m_trackinfo;

    QAction *m_voteaction;
    VoteDialog *m_votedialog;

    QAction *m_repeataction;
    RepeatState *m_repeatstate;
    RepeatDialog *m_repeatdialog;
    SkipDialog *m_skipdialog;

    QScreenInformation* m_tvScreen;
    RotateHelper rotHelper;
};

#ifndef NO_HELIX
class MediaPlayerSettingsDialog : public QDialog
{
    Q_OBJECT
public:
    MediaPlayerSettingsDialog( QWidget* parent = 0 );

    // QDialog
    void accept();

private:
    void readConfig();
    void writeConfig();

    void applySettings();

    QMediaContent *m_content;

    QComboBox *m_speedcombo;

    QLineEdit *m_connecttimeout;
    QLineEdit *m_servertimeout;
    QValidator *m_validator;
};
#endif

#endif
