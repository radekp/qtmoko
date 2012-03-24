#!/bin/sh
mount -t tmpfs none /var/log
/etc/init.d/sysklogd start
/etc/init.d/klogd start

echo ""
echo "+---------------------------------------+"
echo "| We are now logging to ramdisk. Please |"
echo "| note that this can exhaust all your   |"
echo "| RAM after some time.                  |"
echo "|                                       |"
echo "| You also might want to select log     |"
echo "| categories now and restart QtMoko     |"
echo "+---------------------------------------+"