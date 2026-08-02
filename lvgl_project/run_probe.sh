#!/bin/bash
cd /opt/aku/web
U=$(ls /sys/class/udc/ | head -n1)
echo "" > /sys/kernel/config/usb_gadget/g1/UDC
sleep 1
./uacFunc.sh start > /dev/null 2>&1
sleep 2
echo "=== period 64 (1.3ms) ==="
/tmp/probe_block 64 60
echo "=== period 128 (2.7ms) ==="
/tmp/probe_block 128 60
echo "=== period 256 (5.3ms) ==="
/tmp/probe_block 256 60
./uacFunc.sh stop > /dev/null 2>&1
echo "$U" > /sys/kernel/config/usb_gadget/g1/UDC
echo "=== done ==="
