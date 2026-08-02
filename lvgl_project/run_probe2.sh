#!/bin/bash
cd /opt/aku/web
U=$(ls /sys/class/udc/ | head -n1)
echo "" > /sys/kernel/config/usb_gadget/g1/UDC
sleep 1
./uacFunc.sh start > /dev/null 2>&1
sleep 2
echo "=== period 64 x2000 reads ==="
/tmp/probe_block 64 2000
echo "=== period 128 x1000 reads ==="
/tmp/probe_block 128 1000
./uacFunc.sh stop > /dev/null 2>&1
echo "$U" > /sys/kernel/config/usb_gadget/g1/UDC
echo "=== done ==="
