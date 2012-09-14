/*  accelerometers.cpp
 *
 *  Accelerometer stuff
 *
 *  (c) 2009-2010 Anton Olkhovik <ant007h@gmail.com>
 *
 *  GTA04 support based on original accelerometers.c from OpenMooCow
 *    - accelerometer moobox simulator.
 *  (c) 2008 Thomas White <taw27@srcf.ucam.org>
 *  (c) 2008 Joachim Breitner <mail@joachim-breitner.de>
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

#ifndef QT_QWS_GTA04
#include <sys/ioctl.h>
#include <sys/time.h>
#endif

#include <sys/types.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>

#ifdef QT_QWS_GTA04
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/utsname.h>
#else
#include <linux/input.h>
#include <linux/joystick.h>
#endif

#include <pthread.h>
#include "accelerometers.h"

pthread_t thread;

struct thread_data_t
{
  int finished;
  useconds_t interval_us;
  accelerometer_cb_t callback;
  void *closure;
};

struct thread_data_t global_thread_data = { 0, 0, NULL, NULL };

double acx=0,acy=0,acz=0;

#ifdef QT_QWS_GTA04

#define GET_DATA_INTERVAL 12500 //25000

typedef enum {
	ACCEL_UNKNOWN,
        ACCEL_GTA04	        /* GTA04 */
} AccelType;

typedef struct {

	int		fd;

	AccelType	type;

	int		x;
	int		y;
	int		z;

	/* Temporary values for use during reading */
	int		lx;
	int		ly;
	int		lz;

} AccelHandle;

struct input_event {
	struct timeval time;
	uint16_t type;
	uint16_t code;
	int32_t value;
};
#define EV_SYN (0x00)
#define EV_REL (0x02)
#define EV_ABS (0x03)
#define SYN_REPORT (0x00)
#define REL_X (0x00)
#define REL_Y (0x01)
#define REL_Z (0x02)
#define ABS_X (0x00)
#define ABS_Y (0x01)
#define ABS_Z (0x02)

AccelHandle *accelerometer_open()
{
        AccelHandle *accel;

        /* Initialise accelerometer data structure */
        accel = (AccelHandle*)malloc(sizeof(AccelHandle));
        if ( accel == NULL ) return NULL;
        accel->x = 0;
        accel->y = 0;
        accel->z = 0;
        accel->lx = 0;
        accel->ly = 0;
        accel->lz = 0;
        accel->type = ACCEL_UNKNOWN;

        struct stat st;
        char gta04_accel_file[] = "/dev/input/event1";

        if (stat(gta04_accel_file, &st) == 0)
        {
                accel->fd = open(gta04_accel_file, O_RDONLY, O_NONBLOCK); //, O_RDONLY, 0);
                if ( accel->fd != -1 )
                {
                        printf("GTA04 accelerometer opened\n");
			accel->type = ACCEL_GTA04;
                        return accel;
                }
                else
                {
                        fprintf(stderr, "GTA04 accelerometer file exists but can't be opened. " \
                                        "Check access permissions\n");
                }
        }
        else
        {
            fprintf(stderr, "GTA04 accelerometer file doesn't exist\n");
        }

        fprintf(stderr, "GTA04 accelerometer not available\n");
        return accel;
}

static void accelerometer_shutdown(AccelHandle *accel)
{
	if ( accel->type == ACCEL_GTA04 ) {
		close(accel->fd);
	}
}

int accelerometer_moo_gta04(AccelHandle *accel)
{
	struct input_event ev;
	size_t rval;
	fd_set fds;
	struct timeval t;
	int got_measurement = 0;

	FD_ZERO(&fds);
	FD_SET(accel->fd, &fds);
	t.tv_sec = 0;
	t.tv_usec = 0;
	select(1+accel->fd, &fds, NULL, NULL, &t);

	if ( FD_ISSET(accel->fd, &fds) ) {
            
		rval = read(accel->fd, &ev, sizeof(ev));
		if ( rval != sizeof(ev) ) {
			fprintf(stderr, "Accelerometer: couldn't read data\n");
			return 0;
		}

	} else {
		return 0;	/* No data */
	}

	if ( ev.type == EV_REL ) {
		if ( ev.code == REL_X ) {
			accel->lx = ev.value;
		}
		if ( ev.code == REL_Y ) {
			accel->ly = ev.value;
		}
		if ( ev.code == REL_Z ) {
			accel->lz = ev.value;
		}
	}

	if ( ev.type == EV_ABS ) {
		if ( ev.code == ABS_X ) {
			accel->lx = ev.value;
		}
		if ( ev.code == ABS_Y ) {
			accel->ly = ev.value;
		}
		if ( ev.code == ABS_Z ) {
			accel->lz = ev.value;
		}
	}

	if ( ev.type == EV_SYN ) {
		if ( ev.code == SYN_REPORT ) {
			accel->x = accel->lx;
			accel->y = accel->ly;
			accel->z = accel->lz;
			got_measurement = 1;
		}
	}

	/* The GTA04 implementation of the accelerometer interface
	   aims for the reported acceleration values to be in units of
	   1g.  That seems to be about right for the QtMaze game, so I
	   guess that units of 1g is in line with what the
	   accelerometer interface has generated on other devices.
	   (And we need to guess, because I don't think it's
	   explicitly documented anywhere.)  On my GTA04, sitting flat
	   on the table, the raw Z acceleration value is about 256, so
	   we divide by 256 here. */
	acx = - accel->x / 256.0;
	acy = - accel->y / 256.0;
	acz = - accel->z / 256.0;

        if (acx<-1) acx=-1; if (acx>1) acx=1;
        if (acy<-1) acy=-1; if (acy>1) acy=1;
        //if (acz<-1) acz=-1; if (acz>1) acz=1;
	
	return got_measurement;
}

int accelerometer_moo(AccelHandle *accel)
{
	switch ( accel->type ) {
		case ACCEL_UNKNOWN : {
			return 0;
		}
		case ACCEL_GTA04 : {
			return accelerometer_moo_gta04(accel);
		}
		/* Add other types here. */
	}

	return 0;
}

/* The accelerometer work thread */
void* accel_work(void *data)
{
	AccelHandle *accel;
	thread_data_t *my_thread_data = (thread_data_t *)data;

	accel = accelerometer_open();
	while ( !(my_thread_data->finished) ) {

		/* Call accelerometer_moo() until we have an
		   acceleration measurement. */
		while (!accelerometer_moo(accel))
		  ;

		/* If the library user specified a callback, call
		   it. */
		if (my_thread_data->callback)
		  (*my_thread_data->callback)(my_thread_data->closure,
					      acx, acy, acz);

		/* Sleep before taking another measurement. */
                usleep(my_thread_data->interval_us);
        }

	accelerometer_shutdown(accel);

	return NULL;
}

#else // Code for devices other than GTA04

#define JS_DEVICE_NEO "/dev/input/js1"
#define JS_DEVICE_PC  "/dev/input/js0"
#define MAX_AXIS_NEO  1024.0 //1000.0
#define MAX_AXIS_PC  64000.0 //32767.0
double max_axis = MAX_AXIS_NEO;
int pc_mode=0;

#define GET_DATA_INTERVAL 2000

/* The accelerometer work thread */
void* accel_work(void *data)
{
	thread_data_t *my_thread_data = (thread_data_t *)data;

        int fd;
        if ((fd = open(JS_DEVICE_NEO, O_RDONLY)) < 0)
        {
                fprintf(stderr, "Accelerometer: error opening file " JS_DEVICE_NEO "\n");
                pc_mode = 1;
                max_axis = MAX_AXIS_PC;
                if ((fd = open(JS_DEVICE_PC, O_RDONLY)) < 0)
                {
                        fprintf(stderr, "Accelerometer: error opening file " JS_DEVICE_PC "\n");
                        return NULL;
                }
        }

        unsigned char axes = 2;
        //unsigned char buttons = 2;
        ioctl(fd, JSIOCGAXES, &axes);
        //ioctl(fd, JSIOCGBUTTONS, &buttons);

        int *axis;
        //char *button;
        struct js_event js;

        axis = (int*)calloc(axes, sizeof(int));
        //button = (char*)calloc(buttons, sizeof(char));

        while ( !(my_thread_data->finished) )
        {
                size_t rval;
                fd_set fds;
                struct timeval t;

                FD_ZERO(&fds);
                FD_SET(fd, &fds);
                t.tv_sec = 0;
                t.tv_usec = 0;
                select(1+fd, &fds, NULL, NULL, &t);

                if ( FD_ISSET(fd, &fds) )
                {
                        rval = read(fd, &js, sizeof(js));
                        if ( rval != sizeof(js) )
                        {
                                fprintf(stderr, "Accelerometer: error reading\n");
                                return NULL;
                        }

                        switch(js.type & ~JS_EVENT_INIT)
                        {
                        //case JS_EVENT_BUTTON:
                        //        button[js.number] = js.value;
                        //        break;
                        case JS_EVENT_AXIS:
                                axis[js.number] = js.value;
                                break;
                        }

                        acx = axis[0] / max_axis;
                        acy = axis[1] / max_axis;
                        acz = axis[2] / max_axis;

                        if (acx<-1) acx=-1;
                        if (acy<-1) acy=-1;
                        if (acz<-1) acz=-1;

                        if (acx>1) acx=1;
                        if (acy>1) acy=1;
                        if (acz>1) acz=1;
                }
                else
                {
                        /* No data */
                }

		/* If the library user specified a callback, call
		   it. */
		if (my_thread_data->callback)
		  (*my_thread_data->callback)(my_thread_data->closure,
					      acx, acy, acz);

                usleep(my_thread_data->interval_us);
        }

        close(fd);

	return NULL;
}

#endif

void accelerometer_start(int interval_ms,
			 accelerometer_cb_t callback,
			 void *closure)
{
	global_thread_data.finished = 0;
	global_thread_data.interval_us = 1000 * interval_ms;
	if (global_thread_data.interval_us < GET_DATA_INTERVAL)
	{
	  global_thread_data.interval_us = GET_DATA_INTERVAL;
	}
	global_thread_data.callback = callback;
	global_thread_data.closure = closure;
        pthread_create( &thread, NULL, accel_work, &global_thread_data);
}

void accelerometer_stop()
{
	global_thread_data.finished = 1;
	pthread_join(thread, NULL);
}

double getacx()
{
	return acx;
}
double getacy()
{
#ifdef QT_QWS_GTA04
	return acy;
#else
        return pc_mode?-acy:acy;
#endif
}
double getacz()
{
	return acz;
}
