/*
 *	Lightweight At Daemon
 *
 *	Compile with:
 *		gcc -s -Wall -Wstrict-prototypes atd.c -o atd
 *
 *	Copyright (C) 1996, Paul Gortmaker.
 *	Copyright (C) 2001, Russell Nelson
 *
 *	Released under the GNU General Public License, version 2,
 *	included herein by reference.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/rtc.h>
/*#include "rtc.h"*/
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/select.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <syslog.h>

#if defined(QT_QWS_SL5XXX)
#include <asm/sharp_apm.h>
#include <asm/hardware.h>

# define DAEMON
# define BUGGY_RTC_SELECT
#elif defined(QT_QWS_GREENPHONE)
# include "greenphone.h"
# define NO_LINUX_RTC
# define BUGGY_RTC_SELECT
#elif defined(QT_QWS_NEO)
# define USE_WKALM
# define DAEMON
#else
# define BROKEN_RTC_ALARM
# define DAEMON
#endif


int compare_rtc_to_tm(struct rtc_time *rtc, struct tm *tm)
{
  int i;

          i = rtc->tm_year - tm->tm_year;
  if (!i) i = rtc->tm_mon - tm->tm_mon;
  if (!i) i = rtc->tm_mday - tm->tm_mday;
  if (!i) i = rtc->tm_hour - tm->tm_hour;
  if (!i) i = rtc->tm_min - tm->tm_min;
  if (!i) i = rtc->tm_sec - tm->tm_sec;
  if (!i) return 0;
  if (i > 0) return 1;
  return -1;
}

void waitfor(time_t t) {
  int rtcfd, tfd, retval= 0;
  unsigned long data;
  struct rtc_time rtc_tm;
  time_t now;
  struct tm *tm;
  struct timeval tv;
  int nfds;
  fd_set afds;
  int setTime = 0; /*We don't have bool in C...*/
#ifdef USE_WKALM
    struct rtc_wkalrm wake = {0};
#endif

#ifdef DAEMON
  syslog(LOG_DEBUG, "waitfor %ld\n", t);
#else
  printf("waitfor %ld\n", t);
#endif

#ifndef NO_LINUX_RTC
  rtcfd = open ("/dev/rtc", O_RDONLY);
  if (rtcfd ==  -1) {
    rtcfd = open ("/dev/rtc0", O_RDONLY);
  }
  if (rtcfd ==  -1) {
#ifdef DAEMON
    syslog(LOG_ERR, "/dev/rtc: %m\n");
    closelog();
#else
    perror("/dev/rtc");
#endif
    exit(errno);
  }
#endif

  tfd = open ("trigger", O_RDWR);

  if (tfd ==  -1) {
#ifdef DAEMON
    syslog(LOG_ERR, "trigger: %m\n");
    closelog();
#else
    perror("trigger");
#endif
    exit(errno);
  }

  /* Read the RTC time/date */
#ifndef NO_LINUX_RTC
  retval = ioctl(rtcfd, RTC_RD_TIME, &rtc_tm);
  if (retval == -1) {
#ifdef DAEMON
      syslog(LOG_ERR, "ioctl: %m\n");
      closelog();
#else
      perror("ioctl");
#endif
      exit(errno);
  }
#elif defined(QT_QWS_GREENPHONE)
  greenphone_read_time(&rtc_tm);
#endif

#ifdef DAEMON
  syslog(LOG_DEBUG, "Current RTC date/time is %d-%d-%d, %02d:%02d:%02d.\n",
    rtc_tm.tm_mday, rtc_tm.tm_mon + 1, rtc_tm.tm_year + 1900,
    rtc_tm.tm_hour, rtc_tm.tm_min, rtc_tm.tm_sec);
#else
  printf("Current RTC date/time is %d-%d-%d, %02d:%02d:%02d.\n",
    rtc_tm.tm_mday, rtc_tm.tm_mon + 1, rtc_tm.tm_year + 1900,
    rtc_tm.tm_hour, rtc_tm.tm_min, rtc_tm.tm_sec);
#endif

  tm = gmtime(&t);
#ifdef DAEMON
  syslog(LOG_DEBUG, "Alarm date/time is %d-%d-%d, %02d:%02d:%02d.\n",
    tm->tm_mday, tm->tm_mon + 1, tm->tm_year + 1900,
    tm->tm_hour, tm->tm_min, tm->tm_sec);
#else
  printf("Alarm date/time is %d-%d-%d, %02d:%02d:%02d.\n",
    tm->tm_mday, tm->tm_mon + 1, tm->tm_year + 1900,
    tm->tm_hour, tm->tm_min, tm->tm_sec);
#endif

  if (t && compare_rtc_to_tm(&rtc_tm, tm) >= 0) {
#ifndef NO_LINUX_RTC
    close(rtcfd);
#endif
    close(tfd);
    return;
  }

#ifdef BROKEN_RTC_ALARM
  /* Workaround for broken same day only RTC */

  /* If the next alarm is not today, set the alarm for midnight */
  if (tm->tm_mday != rtc_tm.tm_mday ||
      tm->tm_mon != rtc_tm.tm_mon ||
      tm->tm_year != rtc_tm.tm_year) {
    tm->tm_hour = 23;
    tm->tm_min = 59;
    tm->tm_sec = 59;
    } 
#endif
  if (t) {
    /* set the alarm */
#ifndef BROKEN_RTC_ALARM
    rtc_tm.tm_mday  = tm->tm_mday; 
    rtc_tm.tm_mon = tm->tm_mon;
    rtc_tm.tm_year = tm->tm_year;
#endif
    rtc_tm.tm_sec = tm->tm_sec;
    rtc_tm.tm_min = tm->tm_min;
    rtc_tm.tm_hour = tm->tm_hour;

#ifndef NO_LINUX_RTC
    #ifdef USE_WKALM
    wake.time.tm_sec = tm->tm_sec;
    wake.time.tm_min = tm->tm_min;
    wake.time.tm_hour = tm->tm_hour;
    wake.time.tm_mday = tm->tm_mday;
    wake.time.tm_mon = tm->tm_mon;
    wake.time.tm_year = tm->tm_year;
    /* wday, yday, and isdst fields are unused by Linux */
    wake.time.tm_wday = -1;
    wake.time.tm_yday = -1;
    wake.time.tm_isdst = -1;
    wake.enabled = 1;

    retval = ioctl(rtcfd, RTC_WKALM_SET, &wake);
    #else
    retval = ioctl(rtcfd, RTC_ALM_SET, &rtc_tm);
    #endif
    if (retval == -1) {
#ifdef DAEMON
      syslog(LOG_ERR, "ioctl: %m\n");
      closelog();
#else
      perror("ioctl");
#endif
      exit(errno);
    }
#elif defined(QT_QWS_GREENPHONE)
    greenphone_set_alarm(&rtc_tm);
#endif

    /* Read the current alarm settings */
#ifndef NO_LINUX_RTC
    #ifdef USE_WKALM
    retval = ioctl(rtcfd, RTC_WKALM_RD, &wake);
    rtc_tm=wake.time;
    #else
    retval = ioctl(rtcfd, RTC_ALM_READ, &rtc_tm);
    #endif
    if (retval == -1) {
#ifdef DAEMON
      syslog(LOG_ERR, "ioctl: %m\n");
      closelog();
#else
      perror("ioctl");
#endif
      exit(errno);
    }
#elif defined(QT_QWS_GREENPHONE)
    greenphone_read_alarm(&rtc_tm);
#endif

#ifdef DAEMON
    syslog(LOG_DEBUG, "Alarm time now set to %d-%02d-%02d %02d:%02d:%02d.\n",
	   rtc_tm.tm_mday, rtc_tm.tm_mon + 1, rtc_tm.tm_year + 1900,
	   rtc_tm.tm_hour, rtc_tm.tm_min, rtc_tm.tm_sec);
#else
    printf("Alarm time now set to %d-%02d-%02d %02d:%02d:%02d.\n",
	   rtc_tm.tm_mday, rtc_tm.tm_mon + 1, rtc_tm.tm_year + 1900,
	   rtc_tm.tm_hour, rtc_tm.tm_min, rtc_tm.tm_sec);
#endif

    /* Enable alarm interrupts */
#ifndef NO_LINUX_RTC
    retval = ioctl(rtcfd, RTC_AIE_ON, 0);
    if (retval == -1) {
#ifdef DAEMON
      syslog(LOG_ERR, "ioctl: %m\n");
      closelog();
#else
      perror("ioctl");
#endif
      exit(errno);
    }
#endif
  }

#ifdef DAEMON
  syslog(LOG_DEBUG, "Waiting for alarm...");
#else
  printf("Waiting for alarm...");
  fflush(stdout);
#endif
  /* This blocks until the alarm ring causes an interrupt */
  FD_ZERO(&afds);
#ifndef BUGGY_RTC_SELECT
  if (t) FD_SET(rtcfd, &afds);
#endif
  FD_SET(tfd, &afds);

  nfds = tfd + 1;
#ifndef NO_LINUX_RTC
  if (rtcfd > tfd) nfds = rtcfd + 1;
#endif

#ifndef BUGGY_RTC_SELECT
  /* Wait up to ten minutes. */
  tv.tv_sec = 10*60;
#else
  /* Only up to 30 seconds, since we can't use the RTC */
  tv.tv_sec = 30;
#endif
  tv.tv_usec = 0;
  if (select(nfds, &afds, (fd_set *) 0,  (fd_set *) 0, &tv) < 0) {
    if (errno != EINTR)
#ifdef DAEMON
      {
	syslog(LOG_ERR, "select: %m\n");
	closelog();
      }
#else
      perror("select");
#endif
      exit(errno);
  }

#ifndef NO_LINUX_RTC
  if (FD_ISSET(rtcfd, &afds)) {
    retval = read(rtcfd, &data, sizeof(unsigned long));
    if (retval == -1) {
#ifdef DAEMON
      syslog(LOG_ERR, "read: %m\n");
      closelog();
#else
      perror("read");
#endif
      exit(errno);
    }
  }
#endif

  if (FD_ISSET(tfd, &afds)) {
      char buf[8];
      int i;
      retval = read(tfd, buf, 8);
      if (retval == -1) {
#ifdef DAEMON
	  syslog(LOG_ERR, "read: %m\n");
	  closelog();
#else
	  perror("read");
#endif
	  exit(errno);
      }
#ifdef DAEMON
      syslog(LOG_DEBUG, "TRIGGER %d ", retval);
#else
      printf( "TRIGGER %d ", retval );
#endif
      for ( i = 0; i < retval; i++ )
	  if (( setTime = ( buf[i] == 'W' ) ))
	      break;
  }
#ifdef DAEMON
  syslog(LOG_DEBUG, " okay. Alarm rang.\n");
#else
  printf(" okay. Alarm rang.\n");
#endif



  if ( setTime ) {
      /* Set the RTC time/date */
#ifdef DAEMON
      syslog(LOG_DEBUG, "Setting RTC\n");
#else
      printf( "Setting RTC\n" );
#endif
      now = time(NULL);
      tm = gmtime(&now);
      rtc_tm.tm_mday = tm->tm_mday;
      rtc_tm.tm_mon = tm->tm_mon;
      rtc_tm.tm_year = tm->tm_year;
      rtc_tm.tm_hour = tm->tm_hour;
      rtc_tm.tm_min = tm->tm_min;
      rtc_tm.tm_sec = tm->tm_sec;

#ifndef NO_LINUX_RTC
      retval = ioctl(rtcfd, RTC_SET_TIME, &rtc_tm);
      if (retval == -1) {
#ifdef DAEMON
	  syslog(LOG_ERR, "ioctl: %m\n");
	  closelog();
#else
	  perror("ioctl");
#endif
	  exit(errno);
      }
#else
#ifdef DAEMON
      syslog(LOG_ERR, "not setting time\n");
#else
      printf("not setting time\n");
#endif
#endif
  }




  /* Disable alarm interrupts */
#ifndef NO_LINUX_RTC
  retval = ioctl(rtcfd, RTC_AIE_OFF, 0);
  if (retval == -1) {
#ifdef DAEMON
    syslog(LOG_ERR, "ioctl: %m\n");
    closelog();
#else
    perror("ioctl");
#endif
    exit(errno);
  }
  close(rtcfd);
#elif defined(QT_QWS_GREENPHONE)
  greenphone_disable_alarm();
#endif

  close(tfd);

}


int runjob(char *fn)
{
  pid_t pid;
  int status;

  pid = fork();
  if (pid == -1)
    return -1;
  if (pid == 0) {
#ifdef DAEMON
    syslog(LOG_DEBUG, "running %s\n", fn);
#else
    printf("running %s\n", fn);
#endif
    execl(fn, fn, NULL);
    exit(127);
  }
  do {
#ifdef DAEMON
    syslog(LOG_DEBUG, "waiting for %d\n", pid);
#else
    printf("waiting for %d\n", pid);
#endif
    if (waitpid(pid, &status, 0) == -1) {
      if (errno != EINTR)
        return -1;
      }
    break;
  } while(1);
  return 0;
}

unsigned long nameok(const char *name)
{
  char *middle;
  char *end;
  unsigned long that;

  /* check for timestamp.pid */
  that = strtol(name, &middle, 10);
  if (*middle != '.')
    return 0;
  strtol(middle+1, &end, 10);
  if (name + strlen(name) != end)
    return 0;
  return that;
}

void die(const char *s)
{
#ifdef DAEMON
  syslog(LOG_ERR, "%s\n", s);
  closelog();
#else
  fprintf(stderr, "%s\n", s);
#endif
  exit(2);
}

static void enable_rtc_wakeup(void)
{
#ifdef QT_QWS_SL5XXX
  int fd, cur_val, ret;
  /* enable RTC wakeup */
  fd = open ("/dev/apm_bios", O_WRONLY);

  if (fd ==  -1) {
#ifdef DAEMON
    syslog(LOG_ERR, "/dev/apm_bios: %m\n");
    closelog();
#else
    perror("/dev/apm_bios");
#endif
    exit(errno);
  }

  cur_val = ioctl(fd, APM_IOCGWUPSRC, 0);
  if (cur_val == -1) {
#ifdef DAEMON
    syslog(LOG_ERR, "ioctl: %m\n");
    closelog();
#else
    perror("ioctl");
#endif
    exit(errno);
  }

  ret = ioctl(fd, APM_IOCSWUPSRC, cur_val | PWER_RTC);
  if (ret == -1) {
#ifdef DAEMON
    syslog(LOG_ERR, "ioctl: %m\n");
    closelog();
#else
    perror("ioctl");
#endif
    exit(errno);
  }
  close(fd);
#endif
}

int main(int argc, char *argv[]) {
  struct dirent *dirent;
  DIR *dir;
  unsigned long that, next, now;

#ifdef DAEMON
  if (argc != 2) {
    fprintf(stderr, "usage: atd spooldir\n");
    exit(2);
  }
  {
    int	pid;
    pid = fork();
    if (pid > 0) {
      exit(0);
    } else if (pid < 0) {
      exit(-1);
    }
  }
  if (setsid() == -1) {
    exit(-1);
  }
  close(0);
  close(1);
  close(2);
  openlog("atd", LOG_PID, LOG_DAEMON);
#else
  if (argc != 2) die("usage: atd spooldir");
#endif
  if (chdir(argv[1]) < 0) die("cannot chdir");
  if (mkfifo("trigger.new", 0777) < 0) die("cannot mkfifo trigger.new");
  if (rename("trigger.new","trigger")) die("cannot rename trigger.new");

  enable_rtc_wakeup();

  while(1) {
    /* run all the jobs in the past */
    now = time(NULL);
    dir = opendir(".");
    while ((dirent = readdir(dir))) {
      that = nameok(dirent->d_name);
      /* run jobs which are in the past, and find the next job in the future */
      /* avoid race conditions. Run jobs scheduled for the next second now */
      if (that)
        if (that <= now + 1) runjob(dirent->d_name);
    }
    closedir(dir);

    /* find the next job in the future.  A job we just ran might have
     * rescheduled itself.  */
    dir = opendir(".");
    next = ULONG_MAX;
    while ((dirent = readdir(dir))) {
      that = nameok(dirent->d_name);
      /* schedule jobs that we haven't just run */
      if (that) 
        if (that > now + 1)
          if (that < next) next = that;
    }
    closedir(dir);

#ifdef DAEMON
    syslog(LOG_DEBUG, "next: %ld\n", next);
#else
    printf("next: %ld\n", next);
#endif
    if (next == ULONG_MAX) next = 0;
    waitfor(next);
  }
#ifdef DAEMON
  closelog();
#endif
  return 0;
} 
