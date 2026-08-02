#!/bin/bash
# while user plays: kill bridge, record 3s from gadget, measure real rate
killall audio_start.sh 2>/dev/null
sleep 2
timeout 8 arecord -D hw:1,0 -f S16_LE -r 48000 -c 2 -d 3 /tmp/rate_test.raw 2>&1 | tail -2
SIZE=$(stat -c %s /tmp/rate_test.raw 2>/dev/null)
echo "recorded $SIZE bytes in 3s"
echo "implied rate: $((SIZE / 4 / 3)) Hz (48k expected)"
cd /opt/aku/web
nohup ./audio_start.sh > /tmp/uac_start.log 2>&1 &
sleep 8
ps -eo pid,pcpu,comm | grep uac_bridge
