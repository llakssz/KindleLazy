#!/bin/sh

#stop KindleLazy by killing app and removing modules
#then flip the page turn direction and restart
cd "$(dirname "$0")"
killall kindlelazy 2>&1
sleep 2
rmmod mousedev.ko
sleep 1
rmmod usbhid.ko
sleep 1
rmmod hid.ko
sleep 1
./kindlelazy "flip"

insmod hid.ko  2>&1
sleep 1
insmod usbhid.ko 2>&1
sleep 1
insmod mousedev.ko 2>&1
sleep 1

killall kindlelazy > /dev/null 2>&1
sleep 3
./kindlelazy &