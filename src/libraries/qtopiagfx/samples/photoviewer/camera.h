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

#ifndef CAMERA_H
#define CAMERA_H

#include "gfxcanvas.h"
#include <libv4l1-videodev.h>

class Camera : public QObject, public GfxCanvasWindow, public GfxClock
{
    Q_OBJECT
public:
    enum State { Live, Reviewing, Deleting };
    Camera(GfxCanvasItem *parent);
    ~Camera();

    bool active() const;
    void setActive(bool);

    virtual QRect boundingRect();
    virtual void paint(GfxPainter &);
    virtual void tick(int time);

    virtual void keyReleaseEvent(QKeyEvent *);
    virtual void keyPressEvent(QKeyEvent *);
    QImage img();
    State state() const;

signals:
    void stateChanged(Camera::State);

private:
    GfxEvent displayOffEvent();
    GfxEvent displayOnEvent();
    void displayOff();
    void displayOn();
    void moveTo(int, bool = false);
    bool _display;

    
    void grab(unsigned short *, int = 0, int = 240);

    int fd;
    struct video_mbuf mbuf;
    unsigned char *frames;

    void cameraOn();
    void cameraOff();
    bool _active;
    int _lastFrame;
    GfxTimeLine tl;
    GfxTimeLine mtl;

    bool up;
    GfxCanvasItem offset;
    QList<GfxCanvasItem *> items;
    QList<QImage> imgs;
    int item;
    GfxCanvasImage *oldImg;
    GfxCanvasRoundedRect *highlight;
    GfxCanvasItemDirtyValue cameraOffset;
    GfxCanvasText *deleteText;
    bool acceptContext;
};

#endif
