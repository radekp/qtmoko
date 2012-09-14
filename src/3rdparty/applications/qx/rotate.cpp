#include "rotate.h"

#ifdef QTOPIA
#include <QtopiaServiceRequest>
#include <QValueSpaceItem>
#endif

#include <stdlib.h>

#include <accelerometers.h>

/**
 * The acceleromer algorithms and code taken from omnewrotate-0.5.4
 * Copyright © 2008 Rui Miguel Silva Seabra <rms@1407.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * To use create an instance of RotateHelper
 * RotateHelper *rh= new RotateHelper();
 * then start it off..
 * rh->start(); // the default sample rate is 500ms
 * to stop it...
 * rh->stop();
 * to restore to upright...
 * rh->restore();
 * 
 */

#if QTOPIA

RotateHelper::RotateHelper(QObject *parent, int dflg) : QObject(parent)
{
	timer= NULL;
	x = 0;
	y = 0;
	z = 0;

	/* Position flags */
	face_up = 1;
	vertical = 0;
	left = 0;
	right = 0;
	up = 0;
	down = 0;
	last_pos= -1;
	current_pos = -1;
	debug = dflg;
	initial_rotation= -1;
}

RotateHelper::~RotateHelper()
{
	stop();
}

// 90 degrees and 270 degrees is landscape
#define IS_LANDSCAPE(d) (d == 90 || d == 270)

// return true if currently in landscape orientation
bool RotateHelper::isLandscape()
{
	QValueSpaceItem vsiRot("/UI/Rotation/Current");
    int currot= vsiRot.value().toUInt();
	return IS_LANDSCAPE(currot);
}

void RotateHelper::start(int timeinms)
{
	// remember where we were when we started
	QValueSpaceItem vsiRot("/UI/Rotation/Current");
    initial_rotation= vsiRot.value().toUInt();
	
	// don't allow multiple timers to run
	if(timer != NULL){
		stop();
	}

	// start accelerometer
	accelerometer_start(timeinms, NULL, NULL);

	// start up a single shot timer to check accelerometers
	// it will be restarted each time
	timer = new QTimer(this);
	connect(timer, SIGNAL(timeout()), this, SLOT(sample()));
	timer->start(timeinms);
}

void RotateHelper::stop()
{
	if(timer != NULL){
		timer->stop();
		delete timer;
		timer= NULL;
	}

	// stop accelerometer
	accelerometer_stop();
}

void rotate(int degree)
{
    switch(degree)
    {
        case 90:
            system("xrandr -o 1");
            break;
        case 180:
            system("xrandr -o 2");
            break;
        case 270:
            system("xrandr -o 3");
            break;
        default:
            system("xrandr -o 0");
        break;
    }
}

// restore to whatever the rotation was when we started
void RotateHelper::restore()
{
	if(initial_rotation != -1){
                rotate(initial_rotation);
		emit rotated(IS_LANDSCAPE(initial_rotation));
	}
}

void RotateHelper::maybe_rotate(int deg)
{
    rotate(deg);
    emit rotated(IS_LANDSCAPE(deg));
}

void RotateHelper::sample()
{
	x = 1000 * getacx();
	y = 1000 * getacy();
	z = 1000 * getacz();

	if(packet_reader()){
		int pos= define_position();
		if(pos != last_pos)
			maybe_rotate(pos);
		last_pos= pos;
	}else{
		qWarning("RotateHelper: packet reader failed");
		stop();
	}
}


int RotateHelper::neighbour(int value, int target, int neighbour)
{
    return (target - abs(neighbour) < value && value <= target + abs(neighbour));
}

int RotateHelper::define_position(void)
{

	/* Conclude all facts about current position */
	if(neighbour(z,0,500))
		vertical=1;
	else
		vertical=0;

	if(z>=0)
		face_up=1;
	else
		face_up=0;

	if(neighbour(x,-1000,300))
		left=1;
	else
		left=0;

	if(neighbour(x,1000,300))
		right=1;
	else
		right=0;

	if(neighbour(y,1000,300))
		down=1;
	else
		down=0;

	if(neighbour(y,-1000,300))
		up=1;
	else
		up=0;

	if(down) current_pos=DOWN;
	else if(up) current_pos=UP;
	else if(left) current_pos=LEFT;
	else if(right) current_pos=RIGHT;

	if(debug)
	{
		qDebug("v(x,y,z)=(% 5d,% 5d,% 5d) == ",x,y,z);

		qDebug(" face %s", face_up?"up":"down");
		qDebug(" %s-ish", vertical?"vertical":"horizontal");
		if(left) qDebug(" left");
		if(right) qDebug(" right");
		if(up) qDebug(" up");
		if(down) qDebug(" down");

		qDebug("\n");
	}

	return current_pos;
}

bool RotateHelper::packet_reader()
{
	return (x || y || z);
}

#else // QTOPIA

RotateHelper::RotateHelper(QObject *parent, int dflg) : QObject(parent)
{
    Q_UNUSED(dflg);
}

RotateHelper::~RotateHelper()
{
}

// return true if currently in landscape orientation
bool RotateHelper::isLandscape()
{
    return false;
}

void RotateHelper::start(int timeinms)
{
    Q_UNUSED(timeinms);
}

void RotateHelper::stop()
{
}

void RotateHelper::restore()
{
}

#endif // QTOPIA
