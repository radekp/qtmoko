#!/bin/bash
# script executed after resume, next suspend is disabled until if finishes up

#/opt/qtmoko/bin/gps-event.sh resume
#/opt/qtmoko/bin/btfix.sh

# make sure that vibrator is not vibrating which sometimes happens
echo 0 > /sys/class/leds/gta02::vibrator/brightness