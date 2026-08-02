#!/bin/bash
echo "== UAC capture (gadget) =="
cat /proc/asound/card1/pcm0c0/sub0/hw_params 2>/dev/null || ls /proc/asound/card1/
echo "== codec playback =="
cat /proc/asound/card0/pcm0p0/sub0/hw_params 2>/dev/null
echo "== card1 pcm nodes =="
ls /proc/asound/card1/
