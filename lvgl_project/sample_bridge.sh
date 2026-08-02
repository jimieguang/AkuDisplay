#!/bin/bash
# sample uac_bridge CPU while user plays music (expect ticks to climb fast)
PID=$(ps -eo pid,comm | grep uac_bridge | awk '{print $1}')
if [ -z "$PID" ]; then echo "NO BRIDGE"; exit 1; fi
echo "bridge PID=$PID"
T1=$(awk '{print $14+$15}' /proc/$PID/stat)
for i in 1 2 3 4 5 6 7 8; do
  sleep 2
  T2=$(awk '{print $14+$15}' /proc/$PID/stat)
  S=$(awk '{print $3}' /proc/$PID/stat)
  echo "t$i ticks=$T2 delta=$((T2-T1)) state=$S $(date +%H:%M:%S)"
  T1=$T2
done
