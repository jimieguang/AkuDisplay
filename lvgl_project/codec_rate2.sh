#!/bin/bash
# precise codec rate: play 10s(480000 frames) wav, time it
killall audio_start.sh 2>/dev/null
sleep 3
python3 -c "
import wave
w = wave.open('/tmp/rate10s.wav','w')
w.setnchannels(2); w.setsampwidth(2); w.setframerate(48000)
w.writeframes(b'\x00\x00\x00\x00' * 480000)
w.close()
print('wav: 10s @48k')
"
T0=$(date +%s%N)
timeout 20 aplay -D hw:0,0 -q /tmp/rate10s.wav 2>&1
RC=$?
T1=$(date +%s%N)
MS=$(( (T1-T0)/1000000 ))
echo "aplay rc=$RC elapsed=${MS}ms"
echo "implied codec rate: $(( 48000 * 10000 / MS )) Hz (48k expected)"
cd /opt/aku/web
nohup ./audio_start.sh > /tmp/uac_start.log 2>&1 &
sleep 8
ps -eo pid,pcpu,comm | grep uac_bridge
