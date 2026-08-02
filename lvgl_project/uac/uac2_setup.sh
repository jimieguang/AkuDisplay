#!/bin/bash
# uac2_setup.sh - [实验] 重建 48kHz UAC2 gadget（akubox）
#
# 用途：仅当你需要测试 UAC2（替代生产 UAC1）时使用。生产链路是
# /opt/aku/web/uacFunc.sh 的 uac1 gadget + uac_bridge，本脚本不参与开机自启。
#
# 注意（实测坑）：
#   1. UDC 同一时刻只能被一个 gadget 占用（开机时 adb 的 g1 已占用，本脚本会自动解绑）。
#   2. 频繁热解绑/重绑 UDC 会把 USB OUT 端弄坏（Windows 播放端点失效，
#      AUDCLNT_E_DEVICE_INVALIDATED），只能重启设备恢复。
#   3. 采集方向必须 48kHz：bridge 严格按 48kHz 打开 PCM 且无重采样。
#
# 用法（root，重启后执行一次）：
#   /root/uac2_setup.sh     （或本脚本所在路径）

G=/sys/kernel/config/usb_gadget/uac2
UDC=$(ls /sys/class/udc/ | head -n1)

if [ -d "$G" ] && [ -n "$(cat $G/UDC 2>/dev/null)" ]; then
    echo "uac2 already bound"; exit 0
fi

# 释放 UDC（解绑其他 gadget，如开机时的 adb g1）
for g in /sys/kernel/config/usb_gadget/*; do
    if [ -f "$g/UDC" ] && [ -n "$(cat $g/UDC 2>/dev/null)" ]; then
        echo "" > "$g/UDC" 2>/dev/null
        sleep 1
    fi
done

rm -rf $G 2>/dev/null
mount none /sys/kernel/config -t configfs >/dev/null 2>&1

mkdir -p $G/strings/0x409
echo 0x1d6b > $G/idVendor
echo 0x0104 > $G/idProduct
echo "AKU-UAC2-$(date +%s)" > $G/strings/0x409/serialnumber
echo "AKU" > $G/strings/0x409/manufacturer
echo "AKU USB Speaker UAC2" > $G/strings/0x409/product

mkdir -p $G/configs/c.1/strings/0x409
echo "Audio Config" > $G/configs/c.1/strings/0x409/configuration
echo 250 > $G/configs/c.1/MaxPower

mkdir $G/functions/uac2.usb0
F=$G/functions/uac2.usb0
echo 3 > $F/c_chmask; echo 48000 > $F/c_srate; echo 2 > $F/c_ssize
echo 1 > $F/c_mute_present; echo 1 > $F/c_volume_present
echo -25600 > $F/c_volume_min; echo 0 > $F/c_volume_max; echo 256 > $F/c_volume_res
echo 3 > $F/p_chmask; echo 48000 > $F/p_srate; echo 2 > $F/p_ssize
echo 1 > $F/p_mute_present; echo 1 > $F/p_volume_present
echo -25600 > $F/p_volume_min; echo 0 > $F/p_volume_max; echo 256 > $F/p_volume_res
echo "Source/Sink" > $F/function_name

ln -s $F $G/configs/c.1/uac2.usb0
echo $UDC > $G/UDC
sleep 1
echo "uac2 gadget bound to $UDC, capture rate=$(cat $F/c_srate)"
