#!/bin/bash

#unmount file first (maybe silly to do), copy conf from system to public area
#replace counter clock wise setting with clockwise
#mount and remount (makes the read only work) - read only so that makexconfig doesn't overwrite our CW change
#kill lxinit, it will restart X
#set the input flipped so the touch screen is comfortable upside down.

#these changes are not permenant, reboot will restore rotation and input rotation.
#input rotation does not survive lxinit, so we do it afterwards

#maybe good idea to check if everything was successful and THEN fix the touch input... :)

umount /var/local/xorg.conf
cp /var/local/xorg.conf /mnt/us/extensions/kindlelazy/bin/xorg.conf
sed -i 's/        Option      "Rotate"  "CCW"/        Option      "Rotate"  "CW"/g' /mnt/us/extensions/kindlelazy/bin/xorg.conf
mount --bind /mnt/us/extensions/kindlelazy/bin/xorg.conf /var/local/xorg.conf
mount -o remount,ro,bind /var/local/xorg.conf
killall lxinit
sleep 10
/mnt/us/extensions/kindlelazy/bin/xinput set-prop 'multitouch' 'Multitouch Device Orientation' 1
