#!/bin/sh
. /opt/qtmoko/qpe.env

if [ "$(pidof udevd)" ] 
then
  echo 1
  # running on udev
else
  # running on devtmpfs
  wifi-poweron.sh &
  echo 2
fi

# Power on modem on GTA04A4 and higher
if [ -f /sys/class/gpio/gpio186/value ];
then
   echo 1 >/sys/class/gpio/gpio186/value
fi

rm -f /dev/input/mice
echo '1' > /proc/sys/kernel/printk
echo 0 > /sys/devices/virtual/graphics/fbcon/cursor_blink
echo timer > /sys/class/leds/gta04:green:power/trigger
echo 0 > /sys/class/leds/gta04\:red\:power/brightness
echo 0 > /sys/class/leds/gta04\:green\:power/brightness
stty -F /dev/tty1 -echo
mkdir -p /var/cache/apt/archives/partial

atd /var/spool/at

touch /tmp/restart-qtopia
while [ -e /tmp/restart-qtopia ]; do
qpe 2>&1 | logger -t 'Qtopia'
done

killall -q atd
