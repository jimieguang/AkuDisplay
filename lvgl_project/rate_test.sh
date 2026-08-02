#!/bin/bash
# decisive test: record 3s from gadget directly, file size reveals real rate
# 3s @ 48k stereo S16 = 576000 bytes; 3s @ 57.9k = ~695000
killall audio_start.sh 2>/dev/null
sleep 3
timeout 10 arecord -D hw:1,0 -f S16_LE -r 48000 -c 2 -d 3 /tmp/rate_test.raw 2>&1
SIZE=$(stat -c %s /tmp/rate_test.raw 2>/dev/null)
echo "recorded $SIZE bytes in 3s"
echo "implied rate: $((SIZE / 4 / 3)) Hz (48k expected)"
# restart bridge
cd /opt/aku/web
nohup ./audio_start.sh > /tmp/uac_start.log 2>&1 &
sleep 8
ps -eo pid,pcpu,comm | grep uac_bridge
