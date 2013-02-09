#!/bin/sh
if [ -f /etc/init.d/sysklogd ];
then
	update-rc.d -f klogd remove
	update-rc.d -f sysklogd remove
else
	update-rc.d -f rsyslog remove
fi

