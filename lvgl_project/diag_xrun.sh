#!/bin/bash
LOG=/tmp/uac_xrun_test.log
echo "=== uac xrun test start $(date +%H:%M:%S) ===" > $LOG
cd /opt/aku/web
U=$(ls /sys/class/udc/ | head -n1)
echo "" > /sys/kernel/config/usb_gadget/g1/UDC 2>>$LOG
sleep 1
./uacFunc.sh start >> $LOG 2>&1
echo "--- card list ---" >> $LOG
aplay -l >> $LOG 2>&1
alsaloop -C hw:1,0 -P hw:0,0 -r 48000 -c 2 -f S16_LE -t 80000 >> $LOG 2>&1 &
PID=$!
echo "alsaloop PID=$PID" >> $LOG
for i in $(seq 1 150); do
  if ! kill -0 $PID 2>/dev/null; then
    echo "$(date +%H:%M:%S) alsaloop EXITED (was running $i s)" >> $LOG
    break
  fi
  ST=$(cat /proc/$PID/stat 2>/dev/null | awk '{print $3}')
  T=$(awk '{print $14+$15}' /proc/$PID/stat 2>/dev/null)
  echo "$(date +%H:%M:%S) state=$ST ticks=$T" >> $LOG
  sleep 1
done
kill -9 $PID 2>/dev/null
./uacFunc.sh stop >> $LOG 2>&1
echo "$U" > /sys/kernel/config/usb_gadget/g1/UDC 2>>$LOG
echo "=== monitor done ===" >> $LOG
