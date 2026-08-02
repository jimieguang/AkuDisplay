#!/bin/bash
# Diagnostic: (1) v0.3 bridge quick-exit on SIGTERM (2) does gadget unbind hang?
LOG=/tmp/diag_v03.log
echo "=== diag v0.3 $(date +%H:%M:%S) ===" > $LOG
cd /opt/aku/web

# --- start UAC ---
echo "" > /sys/kernel/config/usb_gadget/g1/UDC 2>>$LOG
sleep 1
./uacFunc.sh start >> $LOG 2>&1
sleep 2
nohup /opt/aku/lvgl/uac_bridge 128 1 > /tmp/bridge_v03.log 2>&1 &
BPID=$!
echo "bridge PID=$BPID" >> $LOG
sleep 3
ps -o pid,stat,comm -C uac_bridge >> $LOG 2>&1

# --- test 1: SIGTERM -> quick exit? ---
T0=$(date +%s%N)
kill -TERM $BPID
# wait for it to disappear, up to 5s
EXITED=0
for i in $(seq 1 50); do
    if ! ps -C uac_bridge >/dev/null 2>&1; then EXITED=1; break; fi
    sleep 0.1
done
T1=$(date +%s%N)
MS=$(( (T1 - T0) / 1000000 ))
echo "test1: SIGTERM -> exit in ${MS}ms (exited=$EXITED)" >> $LOG
cat /tmp/bridge_v03.log >> $LOG

# --- test 2: gadget unbind after bridge exited ---
timeout 15 ./uacFunc.sh stop >> $LOG 2>&1
echo "test2: uacFunc.sh stop rc=$? (124=HANG)" >> $LOG

# restore ADB
U=$(ls /sys/class/udc/ | head -n1)
echo "$U" > /sys/kernel/config/usb_gadget/g1/UDC 2>>$LOG
echo "=== done ===" >> $LOG
cat $LOG
