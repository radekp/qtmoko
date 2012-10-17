#!/bin/sh
update-rc.d klogd defaults
update-rc.d sysklogd defaults

echo ""
echo "+---------------------------------------+"
echo "| Please note, that permanent logging   |"
echo "| can wear off your flash memory faster |"
echo "|                                       |"
echo "| Dont forget to disable logging later  |"
echo "+---------------------------------------+"