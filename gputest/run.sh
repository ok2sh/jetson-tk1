#!/bin/bash
cd ~/gputest
tag=$1
temps(){ for z in /sys/class/thermal/thermal_zone*; do printf "%s=%s " $(cat $z/type) $(cat $z/temp); done; echo; }
echo "=== run $tag $(date) ==="
sudo cat /sys/kernel/debug/dri/1/pstate 2>/dev/null | head -20
sudo dmesg -C
echo "temps before: $(temps)"
( while :; do temps >> /tmp/temps.$tag; sleep 2; done ) & sp=$!
echo "--- gltest"; ./gltest
echo "--- cltest"; ./cltest
echo "temps after cl/gl: $(temps)"
kill $sp
echo "--- dmesg"; sudo dmesg | grep -v "^$" | head -40
