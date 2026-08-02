#!/bin/bash
LOG=/tmp/diag_uac2.log
echo "=== compare alsaloop latency vs CPU $(date +%H:%M:%S) ===" > $LOG
cd /opt/aku/web
U=$(ls /sys/class/udc/ | head -n1)
echo "" > /sys/kernel/config/usb_gadget/g1/UDC 2>>$LOG
sleep 1
./uacFunc.sh start >> $LOG 2>&1

meas() {
  local desc="$1"; shift
  local args="$*"
  echo "--- $desc ($args) ---" >> $LOG
  alsaloop $args >> $LOG 2>&1 &
  PID=$!
  sleep 3
  local t1=$(awk '{print $14+$15}' /proc/$PID/stat 2>/dev/null)
  sleep 3
  local t2=$(awk '{print $14+$15}' /proc/$PID/stat 2>/dev/null)
  local w=$(cat /proc/$PID/wchan 2>/dev/null)
  local st=$(awk '{print $3}' /proc/$PID/stat 2>/dev/null)
  echo "  ticks/3s=$((t2-t1)) wchan=$w state=$st" >> $LOG
  kill -9 $PID 2>/dev/null
  sleep 1
}

meas "tlatency 80ms"    -C hw:1,0 -P hw:0,0 -r 48000 -c 2 -f S16_LE -t 80000
meas "tlatency 200ms"   -C hw:1,0 -P hw:0,0 -r 48000 -c 2 -f S16_LE -t 200000
meas "tlatency 500ms"   -C hw:1,0 -P hw:0,0 -r 48000 -c 2 -f S16_LE -t 500000
meas "tlatency 80ms +buf16384" -C hw:1,0 -P hw:0,0 -r 48000 -c 2 -f S16_LE -t 80000 -B 16384

./uacFunc.sh stop >> $LOG 2>&1
echo "$U" > /sys/kernel/config/usb_gadget/g1/UDC 2>>$LOG
echo "=== done ===" >> $LOG
