#!/bin/bash
# clean experiment: sample capF during play vs idle, exact timing
PID=$(ps -eo pid,comm | grep uac_bridge | awk '{print $1}')
if [ -z "$PID" ]; then echo "NO BRIDGE"; exit 1; fi
echo "bridge PID=$PID"

snap() {
  kill -USR1 $PID
  sleep 0.5
  tail -1 /tmp/uac_start.log | sed 's/.*capF=\([0-9]*\) playF=\([0-9]*\).*/\1 \2/'
}

echo "phase A: PLAYING (15s) - start"
sleep 15
A1=$(snap)
sleep 10
A2=$(snap)
echo "A1=$A1 A2=$A2"
CA1=$(echo $A1 | awk '{print $1}'); CA2=$(echo $A2 | awk '{print $1}')
echo "phase A delta capF: $((CA2-CA1)) / 10s = $(( (CA2-CA1)/10 )) fps"

echo "phase B: IDLE (15s) - start"
sleep 15
B1=$(snap)
sleep 10
B2=$(snap)
echo "B1=$B1 B2=$B2"
CB1=$(echo $B1 | awk '{print $1}'); CB2=$(echo $B2 | awk '{print $1}')
echo "phase B delta capF: $((CB2-CB1)) / 10s = $(( (CB2-CB1)/10 )) fps"
