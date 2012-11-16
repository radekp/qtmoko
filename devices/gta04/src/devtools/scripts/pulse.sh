#!/bin/sh
if [ -f /usr/bin/pulseaudio ];
then
	while [ 1 = 1  ]
	do
		logger -t pulse.sh "Starting pulseaudio"
		pulseaudio 2>&1 | logger -t pulse.sh
	done
fi
