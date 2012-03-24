#!/bin/sh
echo 0 >/sys/devices/virtual/gpio/gpio145/value
echo 1 >/sys/devices/virtual/gpio/gpio145/value

#rfkill unblock gps <- does not work yet, no NMEA comming

stty 9600 </dev/ttyO1