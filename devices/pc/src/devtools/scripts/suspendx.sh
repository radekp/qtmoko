#!/bin/sh
if [ -f /tmp/.X0-lock ]
then
  XPID=`cat /tmp/.X0-lock`
  kill -STOP $XPID
fi
