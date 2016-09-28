#!/bin/bash

#unmount, delete local copy (not important)
#kill lxinit and it will restart X
#not needed to restore xinput touch screen orientation because lxinit fixes that

umount /var/local/xorg.conf
rm /mnt/us/extensions/kindlelazy/bin/xorg.conf
killall lxinit