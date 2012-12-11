#include "playerfullscreenwidget.h"

void PlayerFullscreenWidget::showScreen()
{
    showMaximized();
    enterFullScreen();
}

bool PlayerFullscreenWidget::event(QEvent * event)
{
#ifdef QTOPIA
    if (event->type() == QEvent::WindowDeactivate) {
        lower();
        emit deactivated();
    } else if (event->type() == QEvent::WindowActivate) {
        QString title = windowTitle();
        setWindowTitle(QLatin1String("_allow_on_top_"));
        raise();
        setWindowTitle(title);
    }
#endif
    return QWidget::event(event);
}

void PlayerFullscreenWidget::enterFullScreen()
{
#ifdef QTOPIA
    // Show editor view in full screen
    setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
    setWindowState(Qt::WindowFullScreen);
    raise();
#endif
}

PlayerFullscreenWidget::PlayerFullscreenWidget() : QFrame()
  , downX(-1)
  , dim(0)
{
    radius = width() > height() ? height() / 4 : width() / 4;
}

void PlayerFullscreenWidget::paintEvent(QPaintEvent *)
{
    if(downX < 0)
        return;

    QPainter p(this);

#ifdef QT_QWS_NEO
    p.scale(0.5, 0.5);
#endif

    // Circle for unlocking area
    QPoint center = this->rect().center();
    p.drawEllipse(center, radius, radius);

    p.drawText(this->rect(), Qt::AlignCenter,
                tr("pause\n\n\n\n\n\nvol down                vol up\n\n\n\n\n\ndim"));


    // Mouse pointer
    QPoint mP(center.x() + lastX - downX, center.y() + lastY - downY);
    p.setBrush(Qt::white);
    p.setPen( Qt::white);
    p.drawEllipse(mP, radius / 4, radius / 4);
}

void PlayerFullscreenWidget::adjustVolume()
{
    int deltaX = lastX - downX;
    if(deltaX > radius)
        emit volumeUp();
    else if(-deltaX > radius)
        emit volumeDown();
    else {
        adjustingVolume = false;
        return;
    }
    adjustingVolume = true;
    QTimer::singleShot(200, this, SLOT(adjustVolume()));
}

void PlayerFullscreenWidget::mousePressEvent(QMouseEvent * e)
{
    downX = lastX = e->x();
    downY = e->y();
    adjustingVolume = false;
    dim = (dim > 0);
    update();
}

void PlayerFullscreenWidget::mouseReleaseEvent(QMouseEvent *)
{
    downX = lastX = -1;
    update();
}

void PlayerFullscreenWidget::mouseMoveEvent(QMouseEvent * e)
{
    int deltaX = abs(e->x() - downX);
    int deltaY = e->y() - downY;

    lastX = e->x();
    lastY = e->y();

    if(deltaY > radius || deltaY < -radius)
    {
        if(dim == 0 || dim == 1)
        {
            if(deltaY < -radius) {       // slide up
                dim = 1;                 // undim
                hide();                  // leave fullscreen
            }

#ifdef QTOPIA
            QtopiaServiceRequest e1("QtopiaPowerManager", "setBacklight(int)");
            e1 << (dim ? -1 : 0);
            e1.send();
#endif
            dim = (dim > 0 ? -2 : 2);
        }
    }
    else if(deltaX > radius && !adjustingVolume)
        adjustVolume();

    if(!adjustingVolume)
        update();
}
