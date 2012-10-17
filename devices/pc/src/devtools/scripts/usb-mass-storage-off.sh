#!/bin/sh
rmmod g_file_storage
mount -a
modprobe g_ether
ifup usb0
