#!/bin/sh
/etc/init.d/sysklogd stop
/etc/init.d/klogd stop
umount /var/log