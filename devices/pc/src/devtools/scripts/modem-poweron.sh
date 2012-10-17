#!/bin/sh
if [ -f /sys/class/gpio/gpio186/value ];
then
    for i in $(seq 0 1 299)
    do
        if [ -e /dev/ttyHS_Application ];
        then
                exit 0   # modem ok
        fi
        if [  $(( $i % 50 )) -eq 0 ];
        then
            echo 1 >/sys/devices/virtual/gpio/gpio186/value
            sleep 0.2
            echo 0 >/sys/devices/virtual/gpio/gpio186/value
        else
            sleep 0.1
        fi
    done
else
    exit 0  # gta04a03
fi
echo "failed to power on modem"
exit 1
