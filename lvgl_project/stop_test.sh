#!/bin/bash
# v0.7 full-stop test: kill -> bridge exit -> unbind -> ADB restore
LOG=/tmp/stop_test.log
echo "=== stop test $(date +%H:%M:%S) ===" > $LOG

# 1. kill audio_start.sh (SIGTERM -> cleanup -> kill bridge)
T0=$(date +%s%N)
killall audio_start.sh 2>/dev/null

# 2. wait for bridge to exit (up to 5s)
EXITED=0
for i in $(seq 1 50); do
    if ! ps -C uac_bridge >/dev/null 2>&1; then EXITED=1; break; fi
    sleep 0.1
done
T1=$(date +%s%N)
MS=$(( (T1-T0)/1000000 ))
echo "test1: bridge exited=$EXITED in ${MS}ms" >> $LOG
tail -2 /tmp/uac_start.log >> $LOG

# 3. unbind uac gadget (with timeout guard)
timeout 15 ./uacFunc.sh stop >> $LOG 2>&1
echo "test2: uacFunc.sh stop rc=$? (124=HANG)" >> $LOG

# 4. restore ADB
U=$(ls /sys/class/udc/ | head -n1)
echo "$U" > /sys/kernel/config/usb_gadget/g1/UDC 2>>$LOG
sleep 1
UDC=$(cat /sys/kernel/config/usb_gadget/g1/UDC 2>/dev/null | tr -d '\0')
echo "test3: g1 UDC=[$UDC] (expect musb-hdrc.1.auto)" >> $LOG

# 5. verify no residual processes
ps -eo pid,comm | grep -E "uac_bridge|audio_start" >> $LOG 2>&1
echo "=== done ===" >> $LOG
cat $LOG
