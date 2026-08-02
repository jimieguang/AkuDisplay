#!/bin/bash
PID=$(ps -eo pid,comm | grep uac_bridge | awk '{print $1}')
if [ -z "$PID" ]; then echo "NO BRIDGE"; exit 1; fi
echo "bridge PID=$PID"
ps -o pid,pcpu,stat,comm -p $PID
echo "== threads =="
for t in /proc/$PID/task/*; do
  echo "  $(basename $t): $(cat $t/comm 2>/dev/null) state=$(awk '{print $3}' $t/stat 2>/dev/null)"
done
echo "== 5s cpu sample =="
T1=$(awk '{print $14+$15}' /proc/$PID/stat)
sleep 5
T2=$(awk '{print $14+$15}' /proc/$PID/stat)
echo "cpu ticks/5s: $((T2-T1))"
