#!/bin/sh

DEV_NAME=/dev/mmcblk0p1

# check if we run from NAND (assuming ubifs with no_chk_data_crc option)
# if yes we can export whole sdcard
if mount | grep "no_chk_data_crc"; then
    DEV_NAME=/dev/mmcblk0
fi

ifdown usb0
# sync all buffers
sync ; echo 3 > /proc/sys/vm/drop_caches
# trying to unmount all mounted drives
if [ -n `mount | grep "^${DEV_NAME}\[\[:digit:\]\]* "` ]; then
    mount | grep "^${DEV_NAME}p[[:digit:]]* "| awk "{print \$1}"| xargs -n 1 umount 2>/dev/null| true
fi

# mount checking
if mount | grep "^${DEV_NAME}p[[:digit:]]* "; then
    1>&2 echo
    1>&2 echo "the above disk or partition is still mounted, can't suspend device"
    1>&2 echo "unmount it first using umount"
    exit 1
fi
rmmod g_ether
modprobe g_file_storage file=${DEV_NAME} stall=0 removable=y
