#!/bin/sh

#stop KindleLazy by killing app and removing modules
cd "$(dirname "$0")"
killall kindlelazy 2>&1
sleep 2
rmmod usbhid.ko
sleep 1
rmmod hid.ko