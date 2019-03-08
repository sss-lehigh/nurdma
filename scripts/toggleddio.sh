#! /bin/bash

DEV=00:05.0
REG=0x1c0.l
ON=0x42030170
OFF=0x43030170

if [[ $# -lt 1 ]]; then
    echo "usage: $0 <on/off>"
fi
echo $1
if [[ $1 == "on" ]]; then
    sudo setpci -s $DEV $REG=$ON
else
    sudo setpci -s $DEV $REG=$OFF
fi
