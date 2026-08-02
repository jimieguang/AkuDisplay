#!/bin/bash
# UAC audio stop (v3 - full unbind + ADB restore)
#
# v3 restores the complete teardown now that the root cause of the kernel
# freezes is fixed (v0.7 bridge exits cleanly in <100ms via snd_pcm_drop;
# the old hang was: eval "$CMD" & orphaned the bridge -> PCM never released
# -> unbinding the gadget waited forever).
#
# Order matters: wait for audio_start.sh to fully exit (its cleanup kills
# uac_bridge), THEN unbind the gadget, THEN restore ADB.

killall audio_start.sh 2>/dev/null

# Wait up to ~10s for audio_start.sh to finish its cleanup
for i in $(seq 1 50); do
    if ! ps -C audio_start.sh >/dev/null 2>&1; then
        break
    fi
    sleep 0.2
done

./uacFunc.sh stop

# Restore ADB gadget
ADB_UDC="/sys/kernel/config/usb_gadget/g1/UDC"
udc=$(ls /sys/class/udc/ 2>/dev/null | head -n1)
if [ -n "$udc" ] && [ -f "$ADB_UDC" ]; then
    echo "$udc" > "$ADB_UDC" 2>/dev/null
fi
