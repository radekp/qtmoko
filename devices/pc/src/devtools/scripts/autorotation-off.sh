#!/bin/sh

echo 0 > /tmp/autorotation_running.dontdelete

sleep 5

killall autorotation
killall autorotation.perl
killall autorotation.sh 

killall -9 autorotation
killall -9 autorotation.perl
killall -9 autorotation.sh 

