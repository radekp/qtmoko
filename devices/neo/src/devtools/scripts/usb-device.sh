#!/bin/sh
echo 0 > /sys/devices/platform/s3c2440-i2c/i2c-0/0-0073/pcf50633-gpio/reg-fixed-voltage.1/gta02-pm-gsm.0/power_on
echo device > /sys/devices/platform/s3c2410-ohci/usb_mode
