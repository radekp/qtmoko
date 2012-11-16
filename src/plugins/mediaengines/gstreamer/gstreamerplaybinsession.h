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

#ifndef GSTREAMERPLAYBINSESSION_H
#define GSTREAMERPLAYBINSESSION_H

#include <QUrl>
#include <QUuid>
#include <QMediaServerSession>

#include <gst/gst.h>

namespace gstreamer
{

class Engine;
class Message;
class PlaybinSessionPrivate;

class PlaybinSession : public QMediaServerSession
{
    Q_OBJECT

public:
    PlaybinSession(Engine* engine, QUuid const& id, QUrl const& url);
    ~PlaybinSession();

    bool isValid() const;

    void start();
    void pause();
    void stop();

    void suspend();
    void resume();
    bool isSuspended() const;

    void seek(quint32 ms);
    quint32 length();

    void setVolume(int volume);
    int volume() const;

    void setMuted(bool mute);
    bool isMuted() const;

    QtopiaMedia::State playerState() const;

    QString errorString();

    void setDomain(QString const& domain);
    QString domain() const;

    QStringList interfaces();

    QString id() const;
    QString reportData() const;

private slots:
    void busMessage(Message const& msg);
    void updateSinkFormat();
    void repaintLastFrame();

private:
    void getStreamsInfo();
    void readySession();
    void setQtState(QtopiaMedia::State);
    bool setGstState(GstState);

    PlaybinSessionPrivate*  d;
};

}   // ns gstreamer

#endif
