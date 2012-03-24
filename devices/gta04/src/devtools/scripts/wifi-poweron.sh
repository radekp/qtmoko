#!/bin/sh
udevd &         # start udevd to supply firmware for wifi
sleep 10
modprobe libertas_sdio
sleep 10