#!/bin/sh
if [ -f /etc/init.d/sysklogd ];
then
        /etc/init.d/sysklogd stop
        /etc/init.d/klogd stop
else
        /etc/init.d/rsyslog stop
fi
umount /var/log
