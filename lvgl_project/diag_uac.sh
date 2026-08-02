#!/bin/bash
LOG=/tmp/diag_uac.log
echo "=== diag start $(date +%H:%M:%S) ===" > $LOG
cd /opt/aku/web
# unbind g1 (ADB) first, same as production audio_start.sh
U=$(ls /sys/class/udc/ | head -n1)
echo "unbind g1, udc=$U" >> $LOG
echo "" > /sys/kernel/config/usb_gadget/g1/UDC 2>>$LOG
sleep 1
./uacFunc.sh start >> $LOG 2>&1
echo "--- card list ---" >> $LOG
aplay -l >> $LOG 2>&1
alsaloop -C hw:1,0 -P hw:0,0 -r 48000 -c 2 -f S16_LE -t 80000 >> $LOG 2>&1 &
PID=$!
echo "PID=$PID" >> $LOG
for i in 1 2 3 4 5; do
  sleep 1
  W=$(cat /proc/$PID/wchan 2>/dev/null)
  S=$(cat /proc/$PID/status 2>/dev/null | grep -E 'State|Threads' | tr '\n' ' ')
  U=$(cat /proc/$PID/stat 2>/dev/null | awk '{print $14+$15}')
  echo "sample$i wchan=[$W] $S ticks=[$U]" >> $LOG
done
kill -9 $PID 2>/dev/null
./uacFunc.sh stop >> $LOG 2>&1
echo "$U" > /sys/kernel/config/usb_gadget/g1/UDC 2>>$LOG
echo "=== diag done ===" >> $LOG
