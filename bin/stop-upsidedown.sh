#!/bin/bash

#unmount, delete local copy (not important)
#kill lxinit and it will restart X
#not needed to restore xinput touch screen orientation because lxinit fixes that

cd "$(dirname "$0")"

umount /var/local/xorg.conf
rm xorg.conf
killall lxinit