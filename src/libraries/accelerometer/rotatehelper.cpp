#include "rotatehelper.h"

#include <QtopiaServiceRequest>
#include <QValueSpaceItem>

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

RotateHelper::RotateHelper(QObject *parent, bool doRotation, int dflg) : QObject(parent)
, doRotation(doRotation)
{
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
	if(doRotation) {
        QValueSpaceItem vsiRot("/UI/Rotation/Current");
        initial_rotation= vsiRot.value().toUInt();
    }

	// start accelerometer
	accelerometer_start(timeinms, RotateHelper::accel_callback, this);
}

void RotateHelper::stop()
{
	// stop accelerometer
	accelerometer_stop();
}

// restore to whatever the rotation was when we started
void RotateHelper::restore()
{
	if(initial_rotation != -1){
		QtopiaServiceRequest svreq("RotationManager", "setCurrentRotation(int)");
		svreq << initial_rotation;
		svreq.send();
		emit rotated(IS_LANDSCAPE(initial_rotation));
	}
}

void RotateHelper::maybe_rotate(int deg)
{
    if(doRotation) {
        QValueSpaceItem vsiRot("/UI/Rotation/Current");
        int currot= vsiRot.value().toUInt();

        if(deg == currot)
            return;

        QtopiaServiceRequest svreq("RotationManager", "setCurrentRotation(int)");
        svreq << deg;
        svreq.send();
    }
    emit rotated(IS_LANDSCAPE(deg));
}

void RotateHelper::sample()
{
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

void RotateHelper::accel_sample(double acx,
				double acy,
				double acz)
{
	x = 1000 * acx;
	y = 1000 * acy;
	z = 1000 * acz;

	sample();
}

void RotateHelper::accel_callback(void *closure,
				  double acx,
				  double acy,
				  double acz)
{
  RotateHelper *rotHelper = (RotateHelper *)closure;
  rotHelper->accel_sample(acx, acy, acz);
}
