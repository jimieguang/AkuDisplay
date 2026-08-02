#!/bin/bash
# UAC 闊抽鍚姩鑴氭湰锛堜紭鍖栫増锛?# - 浣庡欢杩燂細80ms RT 缂撳啿
# - 璁惧鏂紑鏃舵寚鏁伴€€閬匡紝涓嶄細鍚冩弧 CPU
# - 閫€鍑烘椂鑷姩鎭㈠ ADB gadget

UDC_PATH="/sys/kernel/config/usb_gadget/g1/UDC"
ADB_GADGET="/sys/kernel/config/usb_gadget/g1"
DEVICE_ID="UAC1Gadget"
CMD="alsaloop -C hw:1,0 -P hw:0,0 -r 48000 -c 2 -f S16_LE -t 80000"

log() { echo "[$(date '+%H:%M:%S')] $1"; }

udc_unbind() {
    if [ -f "$UDC_PATH" ]; then
        local current_udc=$(tr -d '\0' < "$UDC_PATH")
        if [ -n "$current_udc" ]; then
            log "Unbinding UDC: $current_udc"
            echo "" > "$UDC_PATH"
            sleep 1
        fi
    fi
}

restore_adb_gadget() {
    if [ -d "$ADB_GADGET" ] && [ -f "$ADB_GADGET/configs/c.1/ffs.adb" ]; then
        local udc=$(ls /sys/class/udc/ 2>/dev/null | head -n1)
        if [ -n "$udc" ]; then
            echo "$udc" > "$UDC_PATH" 2>/dev/null
            log "ADB gadget restored"
        fi
    fi
}

device_check() {
    local timeout=10 interval=0.5
    for ((i=0; i<=$timeout; i++)); do
        arecord -l 2>/dev/null | grep -q "$DEVICE_ID" && \
        aplay -l 2>/dev/null | grep -q "$DEVICE_ID" && return 0
        sleep $interval
    done
    log "ERROR: Device $DEVICE_ID not found"
    exit 4
}

cleanup() {
    log "Shutting down..."
    kill $alsapid 2>/dev/null
    wait $alsapid 2>/dev/null
    restore_adb_gadget
    exit 0
}
trap cleanup SIGINT SIGTERM

udc_unbind
eval "./uacFunc.sh start" &
device_check


set -o pipefail

backoff=1
max_backoff=60
alsapid=0

while true; do
    log "Starting audio stream..."
    eval "$CMD" &
    alsapid=$!
    wait $alsapid
    exit_code=$?

    if [ $exit_code -eq 0 ]; then
        backoff=1
        log "Audio stream ended normally"
    else
        if arecord -l 2>/dev/null | grep -q "UAC1Gadget"; then
            log "Device still present, retry in 200ms"
            pkill -P $alsapid 2>/dev/null
            sleep 0.2
            continue
        else
            log "Device missing, retry in ${backoff}s"
            pkill -P $alsapid 2>/dev/null
            sleep $backoff
            backoff=$(( backoff * 2 ))
            [ $backoff -gt $max_backoff ] && backoff=$max_backoff
            continue
        fi
        pkill -P $alsapid 2>/dev/null
    fi

    sleep $backoff
done
