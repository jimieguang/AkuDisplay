#!/bin/bash
# UAC audio start script (optimized, v2 no-gadget-unbind compatible)
# - low latency: blocking uac_bridge (period 128 = 2.7ms), RT SCHED_FIFO
# - fallback to alsaloop if uac_bridge is missing
# - exponential backoff when device missing
# - on exit: only stops the audio stream; uac_speaker gadget stays bound
#   (deliberately - unbinding can hang the kernel; ADB returns on next boot)

UDC_PATH="/sys/kernel/config/usb_gadget/g1/UDC"
ADB_GADGET="/sys/kernel/config/usb_gadget/g1"
DEVICE_ID="UAC1Gadget"
BRIDGE="/opt/aku/lvgl/uac_bridge"

log() { echo "[$(date '+%H:%M:%S')] $1"; }

# Prefer the blocking bridge (CPU ~3% vs alsaloop 45-100%, latency 2.7ms vs 80ms).
# Fall back to alsaloop if the bridge binary is not deployed yet.
if [ -x "$BRIDGE" ]; then
    CMD="$BRIDGE 128 1"
else
    CMD="alsaloop -C hw:1,0 -P hw:0,0 -r 48000 -c 2 -f S16_LE -t 80000"
    log "WARN: $BRIDGE not found, falling back to alsaloop"
fi

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
    # NOTE: do NOT restore ADB here - uac_speaker still owns the UDC and
    # touching it is what has frozen this board. ADB returns on next boot.
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
    # NOTE: no `eval` here! eval "$CMD" & would fork an intermediate shell
    # so $! is the SHELL pid, not the bridge's; cleanup's `kill $alsapid`
    # would leave the real bridge running orphaned (PCM never released ->
    # gadget unbind hangs the kernel). Direct execution makes $! the bridge.
    $CMD &
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
