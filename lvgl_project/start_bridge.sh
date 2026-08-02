#!/bin/bash
cd /opt/aku/web
echo "" > /sys/kernel/config/usb_gadget/g1/UDC
sleep 1
./uacFunc.sh start > /dev/null 2>&1
sleep 2
nohup /tmp/uac_bridge 128 > /tmp/uac_bridge.log 2>&1 &
echo "bridge PID=$!"
sleep 2
ps -o pid,pcpu,stat,comm -C uac_bridge
cat /tmp/uac_bridge.log
echo "--- adb will be restored on stop ---"
