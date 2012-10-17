#!/bin/sh
while true; do
    /sbin/getty 38400 tty$1
done
