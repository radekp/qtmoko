/*  accelerometers.h
 *
 *  Accelerometer stuff
 *
 *  (c) 2009-2010 Anton Olkhovik <ant007h@gmail.com>
 *  (c) 2012 Neil Jerram <neil@ossau.homelinux.net>
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

#ifndef ACCELEROMETERS_H
#define ACCELEROMETERS_H

#include <qtopiaglobal.h>

typedef void (*accelerometer_cb_t)(void *, double, double, double);

QTOPIA_EXPORT void accelerometer_start(int interval_ms,
				       accelerometer_cb_t callback,
				       void *closure);
QTOPIA_EXPORT void accelerometer_stop();

/* On the GTA04, the values returned by the following are in units of
   about 1g.  That seems to be about right for the QtMaze game, so I
   guess that units of 1g is in line with what the accelerometer
   interface generates on other devices; but I haven't verified that
   and I don't think it's explicitly documented anywhere. */
QTOPIA_EXPORT double getacx();
QTOPIA_EXPORT double getacy();
QTOPIA_EXPORT double getacz();

#endif /* ACCELEROMETERS_H */

