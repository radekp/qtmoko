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

#include "camera.h"
#include <GfxPainter>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <libv4l1-videodev.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <signal.h>
#include <sys/time.h>
#include <sched.h>
#include <unistd.h>

Camera::Camera(GfxCanvasItem *parent)
: GfxCanvasWindow(QSize(240, 320), parent), _display(true), _active(false), 
    fd(0), frames(0),
    up(true), offset(this), item(-1), oldImg(0), cameraOffset(this),
    deleteText(0), acceptContext(false)
{
    offset.y().setValue(90);
    offset.x().setValue(-110);
    offset.z().setValue(10);

    highlight = new GfxCanvasRoundedRect(&offset);
    highlight->width().setValue(30);
    highlight->height().setValue(40);
    highlight->setColor(Qt::blue);
    highlight->setCornerCurve(0);
    highlight->setLineWidth(2);
    highlight->visible().setValue(0.);
    highlight->z().setValue(10);

    setActive(true);
}

Camera::~Camera()
{
    setActive(false);
    delete highlight;
    if(oldImg) delete oldImg;
    if(deleteText) delete deleteText;
    qDeleteAll(items);
}

bool Camera::active() const
{
    return _active;
}

void Camera::setActive(bool act)
{
    if(act == _active)
        return;

    dirty();
    _active = act;
    if(active())
        cameraOn();
    else
        cameraOff();

}

QRect Camera::boundingRect()
{
    if(!active())
        return QRect();

    QRect rv(int(layerX() - 120), int(layerY() - 160), 240, 320);
    return rv;
}

static const signed short redAdjust[] = {
-161,-160,-159,-158,-157,-156,-155,-153,
-152,-151,-150,-149,-148,-147,-145,-144,
-143,-142,-141,-140,-139,-137,-136,-135,
-134,-133,-132,-131,-129,-128,-127,-126,
-125,-124,-123,-122,-120,-119,-118,-117,
-116,-115,-114,-112,-111,-110,-109,-108,
-107,-106,-104,-103,-102,-101,-100, -99,
 -98, -96, -95, -94, -93, -92, -91, -90,
 -88, -87, -86, -85, -84, -83, -82, -80,
 -79, -78, -77, -76, -75, -74, -72, -71,
 -70, -69, -68, -67, -66, -65, -63, -62,
 -61, -60, -59, -58, -57, -55, -54, -53,
 -52, -51, -50, -49, -47, -46, -45, -44,
 -43, -42, -41, -39, -38, -37, -36, -35,
 -34, -33, -31, -30, -29, -28, -27, -26,
 -25, -23, -22, -21, -20, -19, -18, -17,
 -16, -14, -13, -12, -11, -10,  -9,  -8,
  -6,  -5,  -4,  -3,  -2,  -1,   0,   1,
   2,   3,   4,   5,   6,   7,   9,  10,
  11,  12,  13,  14,  15,  17,  18,  19,
  20,  21,  22,  23,  25,  26,  27,  28,
  29,  30,  31,  33,  34,  35,  36,  37,
  38,  39,  40,  42,  43,  44,  45,  46,
  47,  48,  50,  51,  52,  53,  54,  55,
  56,  58,  59,  60,  61,  62,  63,  64,
  66,  67,  68,  69,  70,  71,  72,  74,
  75,  76,  77,  78,  79,  80,  82,  83,
  84,  85,  86,  87,  88,  90,  91,  92,
  93,  94,  95,  96,  97,  99, 100, 101,
 102, 103, 104, 105, 107, 108, 109, 110,
 111, 112, 113, 115, 116, 117, 118, 119,
 120, 121, 123, 124, 125, 126, 127, 128,
};

static const signed short greenAdjust1[] = {
  34,  34,  33,  33,  32,  32,  32,  31,
  31,  30,  30,  30,  29,  29,  28,  28,
  28,  27,  27,  27,  26,  26,  25,  25,
  25,  24,  24,  23,  23,  23,  22,  22,
  21,  21,  21,  20,  20,  19,  19,  19,
  18,  18,  17,  17,  17,  16,  16,  15,
  15,  15,  14,  14,  13,  13,  13,  12,
  12,  12,  11,  11,  10,  10,  10,   9,
   9,   8,   8,   8,   7,   7,   6,   6,
   6,   5,   5,   4,   4,   4,   3,   3,
   2,   2,   2,   1,   1,   0,   0,   0,
   0,   0,  -1,  -1,  -1,  -2,  -2,  -2,
  -3,  -3,  -4,  -4,  -4,  -5,  -5,  -6,
  -6,  -6,  -7,  -7,  -8,  -8,  -8,  -9,
  -9, -10, -10, -10, -11, -11, -12, -12,
 -12, -13, -13, -14, -14, -14, -15, -15,
 -16, -16, -16, -17, -17, -17, -18, -18,
 -19, -19, -19, -20, -20, -21, -21, -21,
 -22, -22, -23, -23, -23, -24, -24, -25,
 -25, -25, -26, -26, -27, -27, -27, -28,
 -28, -29, -29, -29, -30, -30, -30, -31,
 -31, -32, -32, -32, -33, -33, -34, -34,
 -34, -35, -35, -36, -36, -36, -37, -37,
 -38, -38, -38, -39, -39, -40, -40, -40,
 -41, -41, -42, -42, -42, -43, -43, -44,
 -44, -44, -45, -45, -45, -46, -46, -47,
 -47, -47, -48, -48, -49, -49, -49, -50,
 -50, -51, -51, -51, -52, -52, -53, -53,
 -53, -54, -54, -55, -55, -55, -56, -56,
 -57, -57, -57, -58, -58, -59, -59, -59,
 -60, -60, -60, -61, -61, -62, -62, -62,
 -63, -63, -64, -64, -64, -65, -65, -66,
};

static const signed short greenAdjust2[] = {
  74,  73,  73,  72,  71,  71,  70,  70,
  69,  69,  68,  67,  67,  66,  66,  65,
  65,  64,  63,  63,  62,  62,  61,  60,
  60,  59,  59,  58,  58,  57,  56,  56,
  55,  55,  54,  53,  53,  52,  52,  51,
  51,  50,  49,  49,  48,  48,  47,  47,
  46,  45,  45,  44,  44,  43,  42,  42,
  41,  41,  40,  40,  39,  38,  38,  37,
  37,  36,  35,  35,  34,  34,  33,  33,
  32,  31,  31,  30,  30,  29,  29,  28,
  27,  27,  26,  26,  25,  24,  24,  23,
  23,  22,  22,  21,  20,  20,  19,  19,
  18,  17,  17,  16,  16,  15,  15,  14,
  13,  13,  12,  12,  11,  11,  10,   9,
   9,   8,   8,   7,   6,   6,   5,   5,
   4,   4,   3,   2,   2,   1,   1,   0,
   0,   0,  -1,  -1,  -2,  -2,  -3,  -4,
  -4,  -5,  -5,  -6,  -6,  -7,  -8,  -8,
  -9,  -9, -10, -11, -11, -12, -12, -13,
 -13, -14, -15, -15, -16, -16, -17, -17,
 -18, -19, -19, -20, -20, -21, -22, -22,
 -23, -23, -24, -24, -25, -26, -26, -27,
 -27, -28, -29, -29, -30, -30, -31, -31,
 -32, -33, -33, -34, -34, -35, -35, -36,
 -37, -37, -38, -38, -39, -40, -40, -41,
 -41, -42, -42, -43, -44, -44, -45, -45,
 -46, -47, -47, -48, -48, -49, -49, -50,
 -51, -51, -52, -52, -53, -53, -54, -55,
 -55, -56, -56, -57, -58, -58, -59, -59,
 -60, -60, -61, -62, -62, -63, -63, -64,
 -65, -65, -66, -66, -67, -67, -68, -69,
 -69, -70, -70, -71, -71, -72, -73, -73,
};

static const signed short blueAdjust[] = {
-276,-274,-272,-270,-267,-265,-263,-261,
-259,-257,-255,-253,-251,-249,-247,-245,
-243,-241,-239,-237,-235,-233,-231,-229,
-227,-225,-223,-221,-219,-217,-215,-213,
-211,-209,-207,-204,-202,-200,-198,-196,
-194,-192,-190,-188,-186,-184,-182,-180,
-178,-176,-174,-172,-170,-168,-166,-164,
-162,-160,-158,-156,-154,-152,-150,-148,
-146,-144,-141,-139,-137,-135,-133,-131,
-129,-127,-125,-123,-121,-119,-117,-115,
-113,-111,-109,-107,-105,-103,-101, -99,
 -97, -95, -93, -91, -89, -87, -85, -83,
 -81, -78, -76, -74, -72, -70, -68, -66,
 -64, -62, -60, -58, -56, -54, -52, -50,
 -48, -46, -44, -42, -40, -38, -36, -34,
 -32, -30, -28, -26, -24, -22, -20, -18,
 -16, -13, -11,  -9,  -7,  -5,  -3,  -1,
   0,   2,   4,   6,   8,  10,  12,  14,
  16,  18,  20,  22,  24,  26,  28,  30,
  32,  34,  36,  38,  40,  42,  44,  46,
  49,  51,  53,  55,  57,  59,  61,  63,
  65,  67,  69,  71,  73,  75,  77,  79,
  81,  83,  85,  87,  89,  91,  93,  95,
  97,  99, 101, 103, 105, 107, 109, 112,
 114, 116, 118, 120, 122, 124, 126, 128,
 130, 132, 134, 136, 138, 140, 142, 144,
 146, 148, 150, 152, 154, 156, 158, 160,
 162, 164, 166, 168, 170, 172, 175, 177,
 179, 181, 183, 185, 187, 189, 191, 193,
 195, 197, 199, 201, 203, 205, 207, 209,
 211, 213, 215, 217, 219, 221, 223, 225,
 227, 229, 231, 233, 235, 238, 240, 242,
};


#define CLAMP(x) x < 0 ? 0 : x & 0xff

static inline void yuv2rgb(int y, int u, int v, unsigned short *rgb)
{
    register  int r, g, b;

    r = y + redAdjust[v];
    g = y + greenAdjust1[u] + greenAdjust2[v];
    b = y + blueAdjust[u];

    r = CLAMP(r);
    g = CLAMP(g);
    b = CLAMP(b);

    *rgb = (unsigned short)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

Camera::State Camera::state() const
{
    if(deleteText)
        return Deleting;
    else if(item != -1)
        return Reviewing;
    else 
        return Live;
}

QImage Camera::img() 
{
    if(!active())
        return QImage();

    QImage rv(240, 320, QImage::Format_RGB16);
    unsigned short *out = (unsigned short *)rv.bits();

    grab(out);
    return rv;
}

void Camera::grab(unsigned short *out, int xstart, int maxwidth)
{
#ifdef QT_QWS_GREENPHONE
    int width = 320;
    int height = 240;
    int currentFrame = (_lastFrame - 1) % mbuf.frames;
    unsigned char *buf_orig = frames + mbuf.offsets[currentFrame];
    for(int yy_base = 0; yy_base < width; yy_base += 64) {

        for(int x = xstart; x < height && x < maxwidth; ++x) 
        { 
            for (int y = yy_base; y < width && y < yy_base + 64; y++)
            {
                unsigned short *dest = out + x + y * height;
                unsigned char *buf = buf_orig + 
                    ((height - 1) - x) * 2 * width +
                    (y & 0xFFFFFFFE) * 2;

                int u = buf[0];
                int v = buf[2];
                yuv2rgb(buf[1 + (y & 0x1) * 2], u, v, dest);
            }

        }
    }
#endif
}

void Camera::paint(GfxPainter &g)
{
    if(!active())
        return;

    if(_display) {
        unsigned short *out = (unsigned short *)g.backBuffer();

        int off = int(cameraOffset.value());
        int xstart = -off;
        int width = 240 - xstart;

        out -= xstart;

        if(width > 0)
            grab(out, xstart);
    }

    GfxCanvasItem::paint(g);
}

GfxEvent Camera::displayOffEvent()
{
    return GfxEvent(this, this, &Camera::displayOff);
}

GfxEvent Camera::displayOnEvent()
{
    return GfxEvent(this, this, &Camera::displayOn);
}

void Camera::displayOff()
{
    if(_display) {
        dirty();
        _display = false;
    }
}

void Camera::displayOn()
{
    if(!_display) {
        dirty();
        _display = true;
    }
}

#define	VIDEO_DEVICE "/dev/video0"

void Camera::cameraOn()
{
#ifdef QT_QWS_GREENPHONE
    fd = open(VIDEO_DEVICE, O_RDWR);
    if(fd == -1)
        perror("Open");

    struct { int r1; int r2; } _pm = { 15, 15 };
    if(ioctl(fd, 215, &_pm) == -1)
        perror("Palette mode");

    int width = 320;
    int height = 240;
    struct video_window wind;
    memset(&wind, 0, sizeof(wind));
    wind.width = width;
    wind.height = height;

    if ( ioctl( fd, VIDIOCSWIN, &wind ) < 0 ) 
        perror("Set size");

    memset( &mbuf, 0, sizeof( mbuf ) );
    if ( ioctl( fd, VIDIOCGMBUF, &mbuf ) < 0 ) 
        perror("mbuf");

    frames = (unsigned char *)mmap( 0, mbuf.size, PROT_READ | PROT_WRITE,
                                    MAP_SHARED, fd, 0 );
    if(!frames)
        perror("mmap");

    if(ioctl(fd, VIDIOCCAPTURE, 0) == -1)
        perror("Capture");

    _lastFrame = 0;
    tickFor(10000000); // infinite
#endif
}

void Camera::cameraOff()
{
#ifdef QT_QWS_GREENPHONE
    munmap( frames, mbuf.size );
    ioctl(fd, VIDIOCCAPTURE, -1);
    close(fd);
    fd = 0;
    frames = 0;

    cancelClock();
#endif
}

void Camera::tick(int)
{
    if(!active())
        return;

#ifdef QT_QWS_GREENPHONE
    int cap = 0;
    ioctl(fd, 228, &cap);
    if(cap != _lastFrame) {
        _lastFrame = cap;
        if(_display)
            dirty();
    }
#endif
}

void Camera::keyReleaseEvent(QKeyEvent *e)
{
    switch(e->key()) {
    case Qt::Key_Select:
    case Qt::Key_Down:
    case Qt::Key_Up:
    case Qt::Key_Left:
    case Qt::Key_Right:
    case Qt::Key_Back:
    case Qt::Key_Context1:
        break;
    default:
        return;
    }
    if(e->key() != Qt::Key_Context1 || acceptContext) {
        e->accept();
        acceptContext = false;
    }
}

void Camera::moveTo(int newItem, bool force)
{
    if(newItem == item && !force)
        return;

    if(item == -1) 
        emit stateChanged(Reviewing);
    else if(newItem == -1)
        emit stateChanged(Live);

    int mul = (newItem > item)?1:-1;
    GfxCanvasImage *i = 0;
    item = newItem;

    if(item != -1) {
        i = new GfxCanvasImage(imgs.at(newItem), this);
        i->x().setValue(mul * 240);
        tl.move(i->x(), 0, 150);
    }
    if(oldImg) {
        tl.move(oldImg->x(), -mul * 240, 150);
        tl.pause(*oldImg, 150);
        tl.execute(oldImg->destroyEvent());
    } else {
        tl.pause(*this, 150);
        tl.execute(displayOffEvent());
        tl.reset(cameraOffset);
        tl.move(cameraOffset, -240, 150);
    }
    oldImg = i;

    tl.reset(highlight->visible());
    tl.reset(highlight->x());
    tl.move(highlight->visible(), 0, 150);
    tl.pause(highlight->x(), 150);
    if(i) {
        tl.set(highlight->x(), items.at(item)->x().value());
        tl.move(highlight->visible(), 1., 150);

        QPoint p = items.at(item)->mapTo(this, QPoint(0, 0));
        int diff = -p.x();
        int td = int(offset.x().value()) + diff;
        int min = -110 + 50 * items.count();
        if(td > min) {
            diff = min - int(offset.x().value());
        }
        tl.reset(offset.x());
        tl.moveBy(offset.x(), diff, 300);
    } else {
        displayOn();

        tl.reset(cameraOffset);
        tl.move(cameraOffset, 0., 150);

        if(items.count()) {
            QPoint p = items.at(0)->mapTo(this, QPoint(0, 0));
            int diff = -p.x();
            int td = int(offset.x().value()) + diff;
            int min = -110 + 50 * items.count();
            if(td > min) {
                diff = min - int(offset.x().value());
            }
            tl.reset(offset.x());
            tl.moveBy(offset.x(), diff, 300);
        }
    }
}

void Camera::keyPressEvent(QKeyEvent *e)
{
    switch(e->key()) {
    case Qt::Key_Select:
    case Qt::Key_Down:
    case Qt::Key_Up:
    case Qt::Key_Left:
    case Qt::Key_Right:
    case Qt::Key_Back:
    case Qt::Key_Context1:
        break;
    default:
        return;
    }
    if(e->key() != Qt::Key_Context1 || deleteText)
        e->accept();

    if(deleteText) {
        if(e->key() != Qt::Key_Context1 && e->key() != Qt::Key_Back)
            return;
    }
    if(e->key() == Qt::Key_Select) {
        if(oldImg) {
            moveTo(-1);
            return;
        }
        GfxCanvasColor *c = new GfxCanvasColor(Qt::white, QSize(240, 320), this);
        c->visible().setValue(0);
        c->z().setValue(100);

        tl.move(c->visible(), 1., 150);
        tl.move(c->visible(), 0., 150);
        tl.pause(*c, 300);
        tl.execute(c->destroyEvent());

        QImage i = img();
        imgs.prepend(i);
        i = i.scaled(30, 40);

        GfxPainter p(i);
        p.fillRect(QRect(0, 0, i.width(), 2), Qt::white);
        p.fillRect(QRect(0, 0, 2, i.height()), Qt::white);
        p.fillRect(QRect(0, i.height() - 2, i.width(), 2), Qt::white);
        p.fillRect(QRect(i.width() - 2, 0, 2, i.height()), Qt::white);

        GfxCanvasImage *gci = new GfxCanvasImage(i, &offset);
        items.prepend(gci);
        gci->x().setValue(-imgs.count() * 50 + 10);
        if(!up)
            gci->y().setValue(90);
        mtl.complete();
        mtl.pause(offset.x(), 150);
        mtl.moveBy(offset.x(), 50, 150);
    } else if(e->key() == Qt::Key_Down) {
        if(!up || oldImg)
            return;
        up = false;
        for(int ii = 0; ii < items.count() && ii < 6; ++ii) {
            tl.reset(items.at(ii)->visible());
            tl.reset(items.at(ii)->y());
            tl.pause(items.at(ii)->visible(), ii * 30);
            tl.pause(items.at(ii)->y(), ii * 30);
            tl.move(items.at(ii)->visible(), 0., 300);
            tl.move(items.at(ii)->y(), 90., 300);
        }
    } else if(e->key() == Qt::Key_Up) {
        if(up || oldImg)
            return;
        up = true;
        for(int ii = 0; ii < items.count() && ii < 6; ++ii) {
            tl.reset(items.at(ii)->visible());
            tl.reset(items.at(ii)->y());
            tl.pause(items.at(ii)->visible(), ii * 30);
            tl.pause(items.at(ii)->y(), ii * 30);
            tl.move(items.at(ii)->visible(), 1., 300);
            tl.move(items.at(ii)->y(), 0., 300);
        }
    } else if(e->key() == Qt::Key_Right) {
        if(!up) 
            return;
        int newItem = item + 1;
        if(newItem >= imgs.count()) {
            return;
        }
        moveTo(newItem);
    } else if(e->key() == Qt::Key_Left) {
        if(!up)
            return;
        int newItem = item - 1;
        if(newItem < -1) {
            return;
        }
        moveTo(newItem);
    } else if(e->key() == Qt::Key_Back) {
       if(item != -1 && !deleteText) {
           deleteText = new GfxCanvasText(QSize(240, 30), this);
           deleteText->setText("Delete?");
           deleteText->z().setValue(10);
           deleteText->setColor(Qt::white);
           deleteText->visible().setValue(0.);
           QFont f;
           f.setPointSize(32);
           deleteText->setFont(f);

           deleteText->y().setValue(-180);
           tl.reset(offset.y());
           tl.reset(offset.visible());
           tl.move(offset.y(), 70, 100);
           tl.move(offset.y(), 180, 200);
           tl.pause(offset.visible(), 100);
           tl.move(offset.visible(), 0., 200);
           tl.move(deleteText->y(), 90, 300);
           tl.move(deleteText->visible(), 1., 300);
           emit stateChanged(Deleting);
       } else if(deleteText) {
           tl.reset(deleteText->y());
           tl.reset(offset.y());
           tl.reset(offset.visible());
           tl.reset(deleteText->visible());
           tl.move(deleteText->y(), -180, 300);
           tl.move(offset.y(), 90, 300);
           tl.move(offset.visible(), 1., 300);
           tl.move(deleteText->visible(), 0., 300);
           tl.pause(*deleteText, 300);
           tl.execute(deleteText->destroyEvent());
           emit stateChanged(Reviewing);
           deleteText = 0;
       }
    } else if(e->key() == Qt::Key_Context1) {
        if(deleteText) {
           tl.reset(deleteText->y());
           tl.reset(offset.y());
           tl.reset(offset.visible());
           tl.reset(deleteText->visible());
           tl.move(deleteText->y(), -180, 300);
           tl.move(offset.y(), 90, 300);
           tl.move(offset.visible(), 1., 300);
           tl.move(deleteText->visible(), 0., 300);
           tl.pause(*deleteText, 300);
           tl.execute(deleteText->destroyEvent());
           emit stateChanged(Reviewing);
           deleteText = 0;
           for(int ii = 0; ii < items.count() && ii < item; ++ii) 
               items.at(ii)->x().setValue(items.at(ii)->x().value() + 50.);
           delete items.at(item);
           items.removeAt(item);
           imgs.removeAt(item);
           offset.x().setValue(offset.x().value() - 50.);
           if(imgs.isEmpty())
               item = -1;
           else if(item >= imgs.count())
               item = imgs.count() - 1;
           moveTo(item, true);

           if(item == -1)
               emit stateChanged(Live);

           acceptContext = true;
        }
    }
}
