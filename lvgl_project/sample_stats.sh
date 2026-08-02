#!/bin/bash
# sample live stats 3x while music plays
PID=$(ps -eo pid,comm | grep uac_bridge | awk '{print $1}')
if [ -z "$PID" ]; then echo "NO BRIDGE"; exit 1; fi
echo "bridge PID=$PID"
for i in 1 2 3; do
  sleep 3
  kill -USR1 $PID
  sleep 1
  echo "--- sample $i $(date +%H:%M:%S) ---"
  tail -1 /tmp/uac_start.log
done
