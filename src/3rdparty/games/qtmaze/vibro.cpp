/*  vibro.cpp
 *
 *  Vibro feedback routines
 *
 *  (c) 2009 Anton Olkhovik <ant007h@gmail.com>
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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <fcntl.h>
#include <pthread.h>
#include <QVibrateAccessory>

#include "vibro.h"

QVibrateAccessory vib;

pthread_t vibro_timer;
int timer_set=0;

#define VIBRATION_TIME 33

int init_vibro()
{
    QVibrateAccessory vib;
}

void* callback(void *data)
{
    timer_set = 1;
    usleep(VIBRATION_TIME*1000);
    vib.setVibrateNow(false);
    timer_set = 0;
    return NULL;
}

int set_vibro(BYTE level)
{
    if (!timer_set)
    {
        vib.setVibrateNow(true);
        pthread_create( &vibro_timer, NULL, callback, NULL);
    }
    return 0;
}

int close_vibro()
{
    if (timer_set) pthread_join(vibro_timer, NULL);
    return 0;
}
