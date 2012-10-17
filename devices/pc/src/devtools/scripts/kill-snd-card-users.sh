#!/bin/sh
users=`lsof -t /dev/snd/pcmC0D0p`
echo "killing $users"
kill $users
