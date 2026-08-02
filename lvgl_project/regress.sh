#!/bin/bash
# Regression: start/stop UAC twice via the production scripts, verify:
#  - start: uac_bridge runs
#  - stop: no hang, bridge exits, ADB restored
LOG=/tmp/regress.log
echo "=== regression $(date +%H:%M:%S) ===" > $LOG

cd /opt/aku/web

for cycle in 1 2; do
    echo "--- cycle $cycle: start ---" >> $LOG
    ./audio_start.sh > /tmp/start_$cycle.log 2>&1 &
    sleep 8
    if ps -C uac_bridge >/dev/null 2>&1; then
        echo "cycle $cycle: bridge RUNNING" >> $LOG
    else
        echo "cycle $cycle: bridge MISSING!" >> $LOG
        cat /tmp/start_$cycle.log >> $LOG
    fi

    echo "--- cycle $cycle: stop ---" >> $LOG
    T0=$(date +%s%N)
    timeout 30 ./audio_stop.sh >> $LOG 2>&1
    RC=$?
    T1=$(date +%s%N)
    MS=$(( (T1 - T0) / 1000000 ))
    echo "cycle $cycle: stop rc=$RC (${MS}ms)" >> $LOG

    sleep 1
    if ps -C uac_bridge >/dev/null 2>&1; then
        echo "cycle $cycle: bridge STILL RUNNING (BAD)" >> $LOG
    else
        echo "cycle $cycle: bridge exited" >> $LOG
    fi

    UDC=$(cat /sys/kernel/config/usb_gadget/g1/UDC 2>/dev/null | tr -d '\0')
    echo "cycle $cycle: g1 UDC=[$UDC]" >> $LOG
    if [ "$UDC" = "musb-hdrc.1.auto" ]; then
        echo "cycle $cycle: ADB RESTORED" >> $LOG
    else
        echo "cycle $cycle: ADB MISSING (BAD)" >> $LOG
    fi
done

echo "=== done ===" >> $LOG
cat $LOG
