/*  form.h
 *
 *  Main game routines
 *
 *  (c) 2009-2010 Anton Olkhovik <ant007h@gmail.com>
 *
 *  This file is part of QtMaze (port of Mokomaze) - labyrinth game.
 *
 *  QtMaze is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  QtMaze is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with QtMaze.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef EXAMPLE_H
#define EXAMPLE_H
#include "ui_formbase.h"
#include "renderarea.h"

class Form : public QWidget, public Ui_FormBase
{
    Q_OBJECT

private:
    QTimer *timer;
    RenderArea *renderArea;
    QPixmap prev_pixmap, prev_p_pixmap, prev_i_pixmap;
    QPixmap next_pixmap, next_p_pixmap, next_i_pixmap;
    QPixmap reset_pixmap, reset_p_pixmap, reset_i_pixmap;
    QPixmap close_pixmap;
    void CheckLoadedPictures();
    void DisableScreenSaver();
    void EnableScreenSaver();
    void SetMenuVis(bool x);
    void SetLevelNo();
    void MoveBall(double x, double y);
    //--------------------------------
    void InitState(bool redraw = true);
    int line(double x0, double y0, double x1, double y1,    double vx0,double vy0, double vx1,double vy1);
    void ZeroAnim();
    void ProcessGameState();
    int testbump(double x,double y,   double mm_vx,double mm_vy);
    int edgebump(int tx,int ty,   double x,double y,   double mm_vx,double mm_vy);
    void tout(double ax, double ay);
    void apply_temp_phys_res();
    void post_temp_phys_res(double x, double y, double mm_vx, double mm_vy);
    void post_phys_res(double x, double y, double mm_vx, double mm_vy);
    void BumpVibrate(double speed);
    void setButtonsPics();
    void acc_timerAction(double acx, double acy);
    static void accel_callback(void *closure, double acx, double acy, double acz);

public:
    Form( QWidget *parent = 0, Qt::WFlags f = 0 );
    ~Form();

private slots:
    void timerAction();
    bool eventFilter(QObject *target, QEvent *event);
    void ScreenTouchedPause();
    void ScreenTouchedContinue();

protected:
    bool event(QEvent *);

};

#endif
