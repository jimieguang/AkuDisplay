#!/bin/bash
# stop uac bridge + restore ADB
pkill -f /tmp/uac_bridge
sleep 1
cd /opt/aku/web
./uacFunc.sh stop > /dev/null 2>&1
U=$(ls /sys/class/udc/ | head -n1)
echo "$U" > /sys/kernel/config/usb_gadget/g1/UDC
cat /tmp/uac_bridge.log
echo "--- adb restored: $U ---"
