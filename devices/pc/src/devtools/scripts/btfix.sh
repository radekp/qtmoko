#!/bin/sh

# hcid may crash on device detach
if [ ! -n "`pidof hcid`" ]; then
        /etc/init.d/bluetooth restart
        logger "UDEV: Restarting bluetooth"
fi