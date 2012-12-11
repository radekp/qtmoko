#ifndef PLAYERFULLSCREENWIDGET_H
#define PLAYERFULLSCREENWIDGET_H

#include <QFrame>
#include <QPixmap>
#include <QPainter>
#include <QEvent>
#include <QApplication>
#include <QMouseEvent>
#include <QTimer>
#include <QDesktopWidget>
#include <QDebug>
#ifdef QTOPIA
#include <QtopiaApplication>
#include <QtopiaServiceRequest>
#endif

// Fullscreen widget that can signal when it's hidden (e.g. by incomming call
// or similar event).
class PlayerFullscreenWidget : public QFrame
{
    Q_OBJECT
public:
    PlayerFullscreenWidget();

signals:
    void deactivated();
    void pause();
    void volumeUp();
    void volumeDown();    

public slots:
    void showScreen();

protected:
    bool event(QEvent *);
    void paintEvent(QPaintEvent *);
    void mousePressEvent(QMouseEvent *);
    void mouseReleaseEvent(QMouseEvent *);
    void mouseMoveEvent(QMouseEvent *);
    void enterFullScreen();

private slots:
    void adjustVolume();
    
private:
    int radius;
    int downX;
    int downY;
    int lastX;
    int lastY;
    int dim;                    // 1 backlight off, 0 backlight on, -2 backlight on (just undimmed by sliding), 2 screen off (just dimmed by sliding)
    bool adjustingVolume;
};

#endif // PLAYERFULLSCREENWIDGET_H
