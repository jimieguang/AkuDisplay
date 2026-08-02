killall audio_start.sh
./uacFunc.sh stop

# 恢复 ADB gadget
ADB_UDC="/sys/kernel/config/usb_gadget/g1/UDC"
udc=$(ls /sys/class/udc/ 2>/dev/null | head -n1)
if [ -n "$udc" ] && [ -f "$ADB_UDC" ]; then
    echo "$udc" > "$ADB_UDC" 2>/dev/null
fi