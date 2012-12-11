#!/bin/sh
echo "***************************"
echo "Modem device is not working"
echo "***************************"
echo ""

echo "$(date) modem reenumerated" >> /modem_reenumerate.log

dmesg | tail -n 50 >>/modem_reenumerate.log

for i in 10 9 8 7 6 5 4 3 2 1
do
    echo "rebooting in $i seconds"
    sleep 1
done

killall qpe
