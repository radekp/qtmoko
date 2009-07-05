#ifndef APPRUNNINGSCREEN_H
#define APPRUNNINGSCREEN_H

#include <QWidget>
#include <QEvent>
#include <QApplication>
#include <QMouseEvent>
#include <QTimer>

// This is fullscreen dialog displayed when X application is running. We need
// to avoid any Qtopia drawing when X app is running so that it's screen is not
// damaged by Qtopia redraws. After long (5s) press we take screenshot of the X
// app, hide and give control back to QX main dialog.
class AppRunningScreen : public QWidget
{
    Q_OBJECT

public:
    AppRunningScreen();
    void showScreen();

signals:
    void longPress();

private:
    QTimer *killTimer;

protected:
    void mousePressEvent(QMouseEvent *);
    void mouseReleaseEvent(QMouseEvent *);
    bool event(QEvent *);
    void enterFullScreen();

private slots:
    void killTimerElapsed();
};

#endif // APPRUNNINGSCREEN_H
