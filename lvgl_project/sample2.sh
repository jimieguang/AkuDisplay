#!/bin/bash
# wait 10s (user playing), then 2x SIGUSR1 stats 8s apart
sleep 10
PID=$(ps -eo pid,comm | grep uac_bridge | awk '{print $1}')
if [ -z "$PID" ]; then echo "NO BRIDGE"; exit 1; fi
kill -USR1 $PID
sleep 1
echo "--- sample A $(date +%H:%M:%S) ---"
tail -1 /tmp/uac_start.log
sleep 8
kill -USR1 $PID
sleep 1
echo "--- sample B $(date +%H:%M:%S) ---"
tail -1 /tmp/uac_start.log
