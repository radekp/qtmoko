#!/bin/sh

ps auxw | grep -v grep | grep -v autorotation-on.sh | grep autorotation && exit 1

echo 1 > /tmp/autorotation_running.dontdelete

cd /tmp
nohup /opt/qtmoko/bin/autorotation.perl &


