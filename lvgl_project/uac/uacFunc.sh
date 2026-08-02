#!/bin/sh
#
# Start USB Audio Gadget (UAC) for Speaker
# v2: idempotent start - if the gadget already exists (kept alive by the
#     no-unbind stop strategy), skip creation and just re-bind the UDC.
#

start() {
    G=/sys/kernel/config/usb_gadget/uac_speaker
    UDC=$(ls /sys/class/udc/ | head -n1)

    # Already fully running? (directory + bound UDC)
    if [ -d "$G" ] && [ -n "$(cat $G/UDC 2>/dev/null | tr -d '\0')" ]; then
        echo "Already running"
        return 0
    fi

    # Directory exists but not bound (leftover from a previous session)
    if [ -d "$G" ]; then
        echo "Re-binding UDC"
        echo $UDC > $G/UDC
        return 0
    fi

    printf "Starting USB Audio Gadget: "
    # 确保configfs已挂载
    mount none /sys/kernel/config -t configfs > /dev/null 2>&1

    # 创建gadget目录结构
    mkdir $G
    cd $G

    # 设备描述符
    echo 0x1d6b > idVendor    # Linux Foundation
    echo 0x0104 > idProduct   # Multifunction Composite Gadget
    echo 0x0100 > bcdDevice   # v1.0.0
    echo 0x0200 > bcdUSB      # USB 2.0

    # 字符串描述符
    mkdir strings/0x409
    echo "00000001" > strings/0x409/serialnumber
    echo "AKU" > strings/0x409/manufacturer
    echo "AKU USB Speaker" > strings/0x409/product

    # 创建配置
    mkdir configs/c.1
    mkdir configs/c.1/strings/0x409
    echo "Audio Config" > configs/c.1/strings/0x409/configuration
    echo 250 > configs/c.1/MaxPower

    # 创建UAC功能
    mkdir functions/uac1.usb0

    # 关联功能到配置
    ln -s functions/uac1.usb0 configs/c.1

    # 启用gadget
    echo $UDC > UDC
}


stop() {
    printf "Stopping USB Audio Gadget: "
    cd /sys/kernel/config/usb_gadget/uac_speaker

    if [ -d "configs/c.1/uac1.usb0" ]; then
        # 禁用gadget
        echo "" > UDC

        # 移除功能链接
        rm configs/c.1/uac1.usb0

        # 移除功能目录
        rmdir functions/uac1.usb0

        cd ..
        rmdir uac_speaker/configs/c.1/strings/0x409
        rmdir uac_speaker/configs/c.1
        rmdir uac_speaker/strings/0x409
        rmdir uac_speaker
        echo "OK"
    else
        echo "Not running"
    fi
}

case "$1" in
    start)
        start
        ;;
    stop)
        stop
        ;;
    restart|reload)
        stop
        sleep 1
        start
        ;;
    *)
        echo "Usage: $0 {start|stop|restart}"
        exit 1
esac

exit $?
