#!/bin/sh

#start KindleLazy by inserting modules and running background app

cd "$(dirname "$0")"

insmod hid.ko  2>&1
sleep 1
insmod usbhid.ko 2>&1
sleep 1
insmod mousedev.ko 2>&1
sleep 1

killall kindlelazy > /dev/null 2>&1
sleep 3
./kindlelazy &
