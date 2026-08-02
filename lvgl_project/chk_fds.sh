#!/bin/bash
echo "== bridge open fds =="
PID=$(ps -eo pid,comm | grep uac_bridge | awk '{print $1}')
ls -la /proc/$PID/fd/ 2>/dev/null | grep snd
echo "== card1 sub dirs =="
ls -la /proc/asound/card1/pcm0c0/ 2>/dev/null
ls -la /proc/asound/card1/pcm0p0/ 2>/dev/null
echo "== capture hw_params =="
cat /proc/asound/card1/pcm0c0/sub0/hw_params 2>/dev/null
echo "== playback hw_params =="
cat /proc/asound/card1/pcm0p0/sub0/hw_params 2>/dev/null
echo "== card0 playback =="
cat /proc/asound/card0/pcm0p0/sub0/hw_params 2>/dev/null
