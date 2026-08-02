#!/bin/bash
PID=$(ps -eo pid,comm | grep uac_bridge | awk '{print $1}')
echo "bridge PID=$PID"
echo "--- sample A (playing?) ---"
kill -USR1 $PID; sleep 1; tail -1 /tmp/uac_start.log
echo "--- wait 5s (user should STOP playback) ---"
sleep 5
echo "--- sample B ---"
kill -USR1 $PID; sleep 1; tail -1 /tmp/uac_start.log
echo "--- gadget rate info ---"
cat /proc/asound/card1/pcm0c/sub0/hw_params 2>/dev/null
cat /proc/asound/card1/pcm0c0/sub0/hw_params 2>/dev/null
ls /proc/asound/card1/pcm0c/ 2>/dev/null
ls /proc/asound/card1/pcm0p/ 2>/dev/null
