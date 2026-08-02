#!/bin/bash
# while user plays: kill bridge, arecord 3s from gadget, check data
killall audio_start.sh 2>/dev/null
sleep 2
timeout 8 arecord -D hw:1,0 -f S16_LE -r 48000 -c 2 -d 3 /tmp/rec3s.raw 2>&1 | tail -1
SIZE=$(stat -c %s /tmp/rec3s.raw 2>/dev/null)
echo "recorded $SIZE bytes"
if [ "$SIZE" -gt 1000 ]; then
  echo "DATA FLOWING: ~$((SIZE/4/3)) Hz"
else
  echo "NO DATA from gadget"
fi
cd /opt/aku/web
nohup ./audio_start.sh > /tmp/uac_start.log 2>&1 &
sleep 8
ps -eo pid,pcpu,comm | grep uac_bridge
