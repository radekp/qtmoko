#!/bin/sh
if ps ax | grep -v grep | grep respawn-tty.sh > /dev/null
then
    echo "ending ok, respawn-tty.sh already running"
else
    respawn-tty.sh 1 &
    respawn-tty.sh 2 &
    respawn-tty.sh 3 &
    respawn-tty.sh 4 &
    respawn-tty.sh 5 &
    respawn-tty.sh 6 &
fi
